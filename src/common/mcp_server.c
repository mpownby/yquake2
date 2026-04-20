/*
 * Optional MCP (Model Context Protocol) bridge server. See header.
 *
 * Architecture:
 *   - One TCP listening socket on mcp_server_bind:mcp_server_port (default
 *     127.0.0.1, port 0 disables the whole subsystem).
 *   - At most one connected client at a time. New connections displace the
 *     previous one (Claude is the only intended caller).
 *   - Requests are line-delimited JSON-RPC: each line is one
 *     {"method":"...","params":{...}} object, response is one
 *     {"result":...} or {"error":"..."} line.
 *   - Polled once per Qcommon_Frame() — single-threaded, synchronous
 *     dispatch, the requester just waits.
 *
 * Methods (milestone 1):
 *   q2_console      params:{cmd:string}  -> {output:string, commands_executed:int}
 *   q2_screenshot   stub                 -> error "not implemented"
 *   q2_get_state    stub                 -> error "not implemented"
 */

#include "header/common.h"
#include "header/mcp_server.h"
#include "header/mcp_json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
   typedef SOCKET mcp_socket_t;
#  define MCP_INVALID_SOCKET INVALID_SOCKET
#  define MCP_SOCKET_ERROR   SOCKET_ERROR
#  define mcp_close          closesocket
#  define mcp_errno()        WSAGetLastError()
#  define MCP_EWOULDBLOCK    WSAEWOULDBLOCK
#  define MCP_EAGAIN         WSAEWOULDBLOCK
#else
#  include <sys/socket.h>
#  include <sys/select.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <errno.h>
   typedef int mcp_socket_t;
#  define MCP_INVALID_SOCKET (-1)
#  define MCP_SOCKET_ERROR   (-1)
#  define mcp_close          close
#  define mcp_errno()        errno
#  define MCP_EWOULDBLOCK    EWOULDBLOCK
#  define MCP_EAGAIN         EAGAIN
#endif

/* ----- configuration ---------------------------------------------------- */

static cvar_t *mcp_server_port;
static cvar_t *mcp_server_bind;

/* ----- socket state ----------------------------------------------------- */

#define MCP_RX_BUFLEN  8192
#define MCP_CAP_BUFLEN (64 * 1024)

static mcp_socket_t s_listen = MCP_INVALID_SOCKET;
static mcp_socket_t s_client = MCP_INVALID_SOCKET;

static char   s_rx_buf[MCP_RX_BUFLEN];
static size_t s_rx_len; /* current valid bytes in s_rx_buf */

/* ----- screenshot provider --------------------------------------------- */

static mcp_screenshot_provider_fn s_screenshot_provider;

void
MCP_RegisterScreenshotProvider(mcp_screenshot_provider_fn fn)
{
	s_screenshot_provider = fn;
}

/* ----- base64 encoder --------------------------------------------------- */

static const char b64chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *
mcp_base64_encode(const unsigned char *in, size_t inlen, size_t *outlen)
{
	size_t olen = 4 * ((inlen + 2) / 3);
	char *out = malloc(olen + 1);
	if (!out)
	{
		return NULL;
	}

	size_t i = 0, j = 0;
	while (i + 3 <= inlen)
	{
		unsigned int v = ((unsigned int)in[i] << 16)
		               | ((unsigned int)in[i + 1] << 8)
		               | (unsigned int)in[i + 2];
		out[j++] = b64chars[(v >> 18) & 0x3F];
		out[j++] = b64chars[(v >> 12) & 0x3F];
		out[j++] = b64chars[(v >> 6) & 0x3F];
		out[j++] = b64chars[v & 0x3F];
		i += 3;
	}
	if (i < inlen)
	{
		unsigned int v = (unsigned int)in[i] << 16;
		if (i + 1 < inlen)
		{
			v |= (unsigned int)in[i + 1] << 8;
		}
		out[j++] = b64chars[(v >> 18) & 0x3F];
		out[j++] = b64chars[(v >> 12) & 0x3F];
		out[j++] = (i + 1 < inlen) ? b64chars[(v >> 6) & 0x3F] : '=';
		out[j++] = '=';
	}
	out[j] = '\0';
	if (outlen) { *outlen = j; }
	return out;
}

/* ----- output capture --------------------------------------------------- */

static char   s_cap_buf[MCP_CAP_BUFLEN];
static size_t s_cap_len;
static int    s_cap_active;

void
MCP_CaptureBegin(void)
{
	s_cap_len = 0;
	s_cap_buf[0] = '\0';
	s_cap_active = 1;
}

const char *
MCP_CaptureEnd(void)
{
	s_cap_active = 0;
	return s_cap_buf;
}

void
MCP_CaptureAppend(const char *msg)
{
	if (!s_cap_active || !msg)
	{
		return;
	}
	size_t n = strlen(msg);
	size_t avail = MCP_CAP_BUFLEN - 1 - s_cap_len;
	if (n > avail)
	{
		n = avail;
	}
	if (n > 0)
	{
		memcpy(s_cap_buf + s_cap_len, msg, n);
		s_cap_len += n;
		s_cap_buf[s_cap_len] = '\0';
	}
}

/* ----- socket helpers --------------------------------------------------- */

static void
mcp_set_nonblocking(mcp_socket_t s)
{
#ifdef _WIN32
	u_long nb = 1;
	ioctlsocket(s, FIONBIO, &nb);
#else
	int flags = fcntl(s, F_GETFL, 0);
	if (flags >= 0)
	{
		fcntl(s, F_SETFL, flags | O_NONBLOCK);
	}
#endif
}

static void
mcp_close_client(void)
{
	if (s_client != MCP_INVALID_SOCKET)
	{
		mcp_close(s_client);
		s_client = MCP_INVALID_SOCKET;
	}
	s_rx_len = 0;
}

static int
mcp_send_all(const char *data, size_t len)
{
	while (len > 0)
	{
		int n = send(s_client, data, (int)len, 0);
		if (n == MCP_SOCKET_ERROR)
		{
			int e = mcp_errno();
			if (e == MCP_EWOULDBLOCK || e == MCP_EAGAIN)
			{
				/* In single-threaded use this is rare; spin briefly. */
				continue;
			}
			return 0;
		}
		data += n;
		len -= (size_t)n;
	}
	return 1;
}

static void
mcp_send_line(const char *s)
{
	size_t n = strlen(s);
	if (!mcp_send_all(s, n) || !mcp_send_all("\n", 1))
	{
		mcp_close_client();
	}
}

/* ----- response builders ------------------------------------------------ */

static void
mcp_send_error(const char *message)
{
	char buf[1024];
	char *p = buf;
	char *end = buf + sizeof(buf);

	const char *prefix = "{\"error\":";
	size_t plen = strlen(prefix);
	if (p + plen >= end) { return; }
	memcpy(p, prefix, plen); p += plen;

	if (!MCP_Json_AppendEscapedString(&p, end - 1, message))
	{
		return;
	}

	if (p + 1 >= end) { return; }
	*p++ = '}';
	*p = '\0';

	mcp_send_line(buf);
}

static void
mcp_send_console_result(const char *output, int commands_executed)
{
	/* Output may be up to 64KB and JSON-escaping can roughly double it,
	   so allocate generously on the heap. */
	size_t cap = (size_t)strlen(output) * 6 + 256;
	char *buf = malloc(cap);
	if (!buf)
	{
		mcp_send_error("out of memory");
		return;
	}
	char *p = buf;
	char *end = buf + cap;

	const char *prefix = "{\"result\":{\"output\":";
	size_t plen = strlen(prefix);
	memcpy(p, prefix, plen); p += plen;

	if (!MCP_Json_AppendEscapedString(&p, end - 64, output))
	{
		free(buf);
		mcp_send_error("response buffer overflow");
		return;
	}

	int wrote = snprintf(p, (size_t)(end - p),
	                     ",\"commands_executed\":%d}}", commands_executed);
	if (wrote < 0 || wrote >= (int)(end - p))
	{
		free(buf);
		mcp_send_error("response buffer overflow");
		return;
	}

	mcp_send_line(buf);
	free(buf);
}

/* ----- request dispatch ------------------------------------------------- */

static void
mcp_handle_console(const char *request)
{
	char cmd[2048];
	if (!MCP_Json_FindString(request, "cmd", cmd, sizeof(cmd)))
	{
		mcp_send_error("q2_console: missing string param 'cmd'");
		return;
	}

	size_t cmdlen = strlen(cmd);
	if (cmdlen + 1 < sizeof(cmd))
	{
		cmd[cmdlen] = '\n';
		cmd[cmdlen + 1] = '\0';
	}

	MCP_CaptureBegin();
	Cbuf_AddText(cmd);
	Cbuf_Execute();
	const char *output = MCP_CaptureEnd();

	mcp_send_console_result(output, 1);
}

static void
mcp_handle_screenshot(void)
{
	if (!s_screenshot_provider)
	{
		mcp_send_error("q2_screenshot: no provider registered "
		               "(dedicated server or unsupported renderer)");
		return;
	}

	unsigned char *png = NULL;
	size_t png_len = 0;
	int width = 0, height = 0;
	if (!s_screenshot_provider(&png, &png_len, &width, &height))
	{
		mcp_send_error("q2_screenshot: capture failed");
		return;
	}

	size_t b64_len = 0;
	char *b64 = mcp_base64_encode(png, png_len, &b64_len);
	free(png);
	if (!b64)
	{
		mcp_send_error("q2_screenshot: base64 alloc failed");
		return;
	}

	size_t cap = b64_len + 256;
	char *buf = malloc(cap);
	if (!buf)
	{
		free(b64);
		mcp_send_error("q2_screenshot: response alloc failed");
		return;
	}

	int n = snprintf(buf, cap,
	                 "{\"result\":{\"width\":%d,\"height\":%d,"
	                 "\"format\":\"png\",\"png_base64\":\"%s\"}}",
	                 width, height, b64);
	free(b64);
	if (n < 0 || (size_t)n >= cap)
	{
		free(buf);
		mcp_send_error("q2_screenshot: response overflow");
		return;
	}

	mcp_send_line(buf);
	free(buf);
}

static void
mcp_dispatch(const char *request)
{
	char method[64];
	if (!MCP_Json_FindString(request, "method", method, sizeof(method)))
	{
		mcp_send_error("missing string param 'method'");
		return;
	}

	if (strcmp(method, "q2_console") == 0)
	{
		mcp_handle_console(request);
	}
	else if (strcmp(method, "q2_screenshot") == 0)
	{
		mcp_handle_screenshot();
	}
	else if (strcmp(method, "q2_get_state") == 0)
	{
		mcp_send_error("method not implemented yet");
	}
	else
	{
		char err[128];
		snprintf(err, sizeof(err), "unknown method '%s'", method);
		mcp_send_error(err);
	}
}

/* ----- per-frame work --------------------------------------------------- */

static void
mcp_accept_pending(void)
{
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);
	mcp_socket_t s = accept(s_listen, (struct sockaddr *)&from, &fromlen);
	if (s == MCP_INVALID_SOCKET)
	{
		return;
	}

	if (s_client != MCP_INVALID_SOCKET)
	{
		/* Replace the existing client. */
		mcp_close_client();
	}

	mcp_set_nonblocking(s);

	/* Disable Nagle so single-line responses go out immediately. */
	int one = 1;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
	           (const char *)&one, sizeof(one));

	s_client = s;
	s_rx_len = 0;
}

static void
mcp_drain_client(void)
{
	for (;;)
	{
		if (s_rx_len >= MCP_RX_BUFLEN - 1)
		{
			/* line too long; drop the connection */
			mcp_close_client();
			return;
		}

		int n = recv(s_client, s_rx_buf + s_rx_len,
		             (int)(MCP_RX_BUFLEN - 1 - s_rx_len), 0);
		if (n == 0)
		{
			mcp_close_client();
			return;
		}
		if (n == MCP_SOCKET_ERROR)
		{
			int e = mcp_errno();
			if (e == MCP_EWOULDBLOCK || e == MCP_EAGAIN)
			{
				return;
			}
			mcp_close_client();
			return;
		}

		s_rx_len += (size_t)n;
		s_rx_buf[s_rx_len] = '\0';

		/* Process every complete line currently in the buffer. */
		for (;;)
		{
			char *nl = memchr(s_rx_buf, '\n', s_rx_len);
			if (!nl)
			{
				break;
			}
			*nl = '\0';
			mcp_dispatch(s_rx_buf);

			size_t consumed = (size_t)(nl - s_rx_buf) + 1;
			size_t remaining = s_rx_len - consumed;
			if (remaining > 0)
			{
				memmove(s_rx_buf, s_rx_buf + consumed, remaining);
			}
			s_rx_len = remaining;
			s_rx_buf[s_rx_len] = '\0';

			if (s_client == MCP_INVALID_SOCKET)
			{
				/* dispatch closed the connection */
				return;
			}
		}
	}
}

void
MCP_RunFrame(void)
{
	if (s_listen == MCP_INVALID_SOCKET)
	{
		return;
	}

	mcp_accept_pending();

	if (s_client != MCP_INVALID_SOCKET)
	{
		mcp_drain_client();
	}
}

/* ----- init / shutdown -------------------------------------------------- */

void
MCP_Init(void)
{
	mcp_server_port = Cvar_Get("mcp_server_port", "0", CVAR_NOSET);
	mcp_server_bind = Cvar_Get("mcp_server_bind", "127.0.0.1", CVAR_NOSET);

	int port = (int)mcp_server_port->value;
	if (port <= 0 || port > 65535)
	{
		return;
	}

	mcp_socket_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == MCP_INVALID_SOCKET)
	{
		Com_Printf("MCP: socket() failed (err %d), server disabled\n",
		           mcp_errno());
		return;
	}

	int one = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
	           (const char *)&one, sizeof(one));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short)port);
	if (inet_pton(AF_INET, mcp_server_bind->string, &addr.sin_addr) != 1)
	{
		Com_Printf("MCP: invalid bind address '%s', server disabled\n",
		           mcp_server_bind->string);
		mcp_close(s);
		return;
	}

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == MCP_SOCKET_ERROR)
	{
		Com_Printf("MCP: bind(%s:%d) failed (err %d), server disabled\n",
		           mcp_server_bind->string, port, mcp_errno());
		mcp_close(s);
		return;
	}

	if (listen(s, 1) == MCP_SOCKET_ERROR)
	{
		Com_Printf("MCP: listen() failed (err %d), server disabled\n",
		           mcp_errno());
		mcp_close(s);
		return;
	}

	mcp_set_nonblocking(s);
	s_listen = s;
	Com_Printf("MCP: listening on %s:%d\n",
	           mcp_server_bind->string, port);
}

void
MCP_Shutdown(void)
{
	mcp_close_client();
	if (s_listen != MCP_INVALID_SOCKET)
	{
		mcp_close(s_listen);
		s_listen = MCP_INVALID_SOCKET;
	}
}
