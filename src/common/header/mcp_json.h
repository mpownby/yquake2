/*
 * =======================================================================
 *
 * Tiny JSON helpers used by the MCP server. Not a full JSON library:
 * just enough to extract one string value by key from a flat or nested
 * request object, and to write escaped string literals into a response
 * buffer. The protocol is internal and the peer is trusted (loopback
 * Python bridge), so robustness only needs to cover well-formed input.
 *
 * =======================================================================
 */

#ifndef SRC_COMMON_HEADER_MCP_JSON_H_
#define SRC_COMMON_HEADER_MCP_JSON_H_

#include <stddef.h>

/*
 * Locate the first occurrence of "key": "value" in `json` and copy the
 * unescaped value into out (truncated to maxlen-1 + NUL). Recognized
 * escapes: \" \\ \/ \n \r \t \b \f. Returns 1 on success, 0 if the key
 * is absent or the value is not a string.
 */
int MCP_Json_FindString(const char *json, const char *key,
                        char *out, size_t maxlen);

/*
 * Append `s` to *p as an escaped JSON string literal (surrounding quotes
 * included), without overrunning `end`. Advances *p. Returns 1 on success,
 * 0 if the buffer was too small (and *p is left at end).
 */
int MCP_Json_AppendEscapedString(char **p, char *end, const char *s);

#endif
