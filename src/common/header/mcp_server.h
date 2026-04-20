/*
 * =======================================================================
 *
 * Optional MCP (Model Context Protocol) bridge server.
 *
 * When the cvar `mcp_server_port` is non-zero, MCP_Init() opens a TCP
 * listener on `mcp_server_bind:mcp_server_port` (default 127.0.0.1) that
 * speaks line-delimited JSON-RPC. A single external client (typically a
 * Python stdio bridge spawned by Claude Code) connects and sends commands
 * such as console input or screenshot requests. This is a thin remote-
 * control surface; nothing happens unless the cvars are set.
 *
 * =======================================================================
 */

#ifndef SRC_COMMON_HEADER_MCP_SERVER_H_
#define SRC_COMMON_HEADER_MCP_SERVER_H_

#include "shared.h"

void MCP_Init(void);
void MCP_Shutdown(void);
void MCP_RunFrame(void);

/*
 * Output capture: when MCP is dispatching a console command, it wraps the
 * dispatch in MCP_CaptureBegin/End so the text printed by that command can
 * be returned in the JSON response. Com_VPrintf calls MCP_CaptureAppend on
 * every message; that call is a one-comparison no-op when capture is off.
 */
void        MCP_CaptureBegin(void);
const char *MCP_CaptureEnd(void);
void        MCP_CaptureAppend(const char *msg);

/*
 * Screenshot provider: client code registers a callback that captures the
 * current frame and PNG-encodes it. q2_screenshot calls this if registered.
 * Returns 1 on success; *png_bytes is malloc'd and must be free()d by the
 * caller (mcp_server.c handles that).
 */
typedef int (*mcp_screenshot_provider_fn)(unsigned char **png_bytes,
                                          size_t *png_len,
                                          int *width, int *height);
void MCP_RegisterScreenshotProvider(mcp_screenshot_provider_fn fn);

#endif
