/*
 * Client-side glue for the MCP bridge: provides a screenshot capture
 * function that pulls raw pixels from the active renderer (re.Retrieve-
 * Screenshot), PNG-encodes them in-memory via stb_image_write, and hands
 * the bytes back to the engine-side MCP server. The MCP server then base64-
 * encodes and ships them to the connected MCP client over TCP.
 *
 * This file is client-only; q2ded leaves the screenshot provider unset
 * and q2_screenshot returns an error there.
 */

#include "header/client.h"
#include "vid/header/ref.h"
#include "vid/header/stb_image_write.h"
#include "../common/header/mcp_server.h"

typedef struct
{
	unsigned char *data;
	size_t         len;
	size_t         cap;
	int            ok;
} membuf_t;

static void
membuf_write(void *context, void *data, int size)
{
	membuf_t *mb = (membuf_t *)context;
	if (!mb->ok)
	{
		return;
	}

	size_t need = mb->len + (size_t)size;
	if (need > mb->cap)
	{
		size_t new_cap = mb->cap ? mb->cap * 2 : 65536;
		while (new_cap < need)
		{
			new_cap *= 2;
		}
		unsigned char *grown = realloc(mb->data, new_cap);
		if (!grown)
		{
			free(mb->data);
			mb->data = NULL;
			mb->len = 0;
			mb->cap = 0;
			mb->ok = 0;
			return;
		}
		mb->data = grown;
		mb->cap = new_cap;
	}
	memcpy(mb->data + mb->len, data, (size_t)size);
	mb->len += (size_t)size;
}

static int
mcp_client_screenshot(unsigned char **png_bytes, size_t *png_len,
                      int *width, int *height)
{
	if (!re.RetrieveScreenshot)
	{
		return 0;
	}

	int w = 0, h = 0, c = 0;
	unsigned char *pixels = NULL;
	if (!re.RetrieveScreenshot(&w, &h, &c, &pixels))
	{
		return 0;
	}
	if (!pixels || w <= 0 || h <= 0 || (c != 3 && c != 4))
	{
		free(pixels);
		return 0;
	}

	membuf_t mb = { NULL, 0, 0, 1 };
	int rc = stbi_write_png_to_func(membuf_write, &mb, w, h, c,
	                                pixels, w * c);
	free(pixels);

	if (!rc || !mb.ok || !mb.data || mb.len == 0)
	{
		free(mb.data);
		return 0;
	}

	*png_bytes = mb.data;
	*png_len = mb.len;
	*width = w;
	*height = h;
	return 1;
}

void
MCP_ClientInit(void)
{
	MCP_RegisterScreenshotProvider(mcp_client_screenshot);
}
