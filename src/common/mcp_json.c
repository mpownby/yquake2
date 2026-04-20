/*
 * Tiny JSON helpers for the MCP bridge. See header/mcp_json.h.
 */

#include "header/mcp_json.h"

#include <ctype.h>
#include <string.h>

static const char *
skip_ws(const char *p)
{
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
	{
		++p;
	}
	return p;
}

/*
 * Find a "key" token in json (matching the entire quoted key), then expect
 * `: "..."` and return a pointer to the opening quote of the value. Returns
 * NULL if not found or value is not a string. Skips over string contents
 * (honoring backslash escapes) when scanning past values that are themselves
 * strings, so it doesn't confuse keys-inside-values with real keys.
 */
static const char *
find_key_value_start(const char *json, const char *key)
{
	size_t keylen = strlen(key);
	const char *p = json;

	while (*p)
	{
		if (*p == '"')
		{
			const char *start = p + 1;
			const char *q = start;
			while (*q && *q != '"')
			{
				if (*q == '\\' && q[1])
				{
					q += 2;
				}
				else
				{
					++q;
				}
			}
			if (!*q)
			{
				return NULL;
			}
			if ((size_t)(q - start) == keylen
			    && memcmp(start, key, keylen) == 0)
			{
				const char *after = skip_ws(q + 1);
				if (*after != ':')
				{
					p = q + 1;
					continue;
				}
				after = skip_ws(after + 1);
				if (*after != '"')
				{
					return NULL;
				}
				return after;
			}
			p = q + 1;
		}
		else
		{
			++p;
		}
	}
	return NULL;
}

int
MCP_Json_FindString(const char *json, const char *key,
                    char *out, size_t maxlen)
{
	if (!json || !key || !out || maxlen == 0)
	{
		return 0;
	}

	const char *q = find_key_value_start(json, key);
	if (!q)
	{
		out[0] = '\0';
		return 0;
	}

	++q; /* step over opening quote */

	size_t i = 0;
	while (*q && *q != '"' && i < maxlen - 1)
	{
		if (*q == '\\' && q[1])
		{
			char c = q[1];
			switch (c)
			{
				case '"':  out[i++] = '"';  break;
				case '\\': out[i++] = '\\'; break;
				case '/':  out[i++] = '/';  break;
				case 'n':  out[i++] = '\n'; break;
				case 'r':  out[i++] = '\r'; break;
				case 't':  out[i++] = '\t'; break;
				case 'b':  out[i++] = '\b'; break;
				case 'f':  out[i++] = '\f'; break;
				default:   out[i++] = c;    break;
			}
			q += 2;
		}
		else
		{
			out[i++] = *q++;
		}
	}
	out[i] = '\0';
	return (*q == '"') ? 1 : 0;
}

int
MCP_Json_AppendEscapedString(char **p, char *end, const char *s)
{
	char *w = *p;

	if (w >= end)
	{
		return 0;
	}
	*w++ = '"';

	for (const unsigned char *r = (const unsigned char *)s; *r; ++r)
	{
		const char *esc = NULL;
		char buf[8];
		size_t n = 0;

		switch (*r)
		{
			case '"':  esc = "\\\""; n = 2; break;
			case '\\': esc = "\\\\"; n = 2; break;
			case '\n': esc = "\\n";  n = 2; break;
			case '\r': esc = "\\r";  n = 2; break;
			case '\t': esc = "\\t";  n = 2; break;
			case '\b': esc = "\\b";  n = 2; break;
			case '\f': esc = "\\f";  n = 2; break;
			default:
				if (*r < 0x20)
				{
					n = (size_t)snprintf(buf, sizeof(buf),
					                     "\\u%04x", *r);
					esc = buf;
				}
				else
				{
					buf[0] = (char)*r;
					n = 1;
					esc = buf;
				}
				break;
		}

		if (w + n > end)
		{
			*p = end;
			return 0;
		}
		memcpy(w, esc, n);
		w += n;
	}

	if (w >= end)
	{
		*p = end;
		return 0;
	}
	*w++ = '"';
	*p = w;
	return 1;
}
