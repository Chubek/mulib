/* Transport-independent part of the JSON-RPC kernel, shared with MCP.
 * Profile-specific method, ID and params validation belongs to the caller. */
#ifndef MULIB_JSONRPC_ENVELOPE_H
#define MULIB_JSONRPC_ENVELOPE_H
#include "../mulib-json.h"
#include <string.h>

typedef struct {
  const mulib_json_t *method, *params, *id;
} mulib_jsonrpc_envelope_;

static inline int
mulib_jsonrpc_version_(const mulib_json_t *v)
{
  size_t n = 0;
  const char *s = mulib_json_text(v, &n);
  return s && n == 3 && !memcmp(s, "2.0", 3);
}

static inline int
mulib_jsonrpc_envelope_view_(const mulib_json_t *root, mulib_jsonrpc_envelope_ *view)
{
  if (!view || mulib_json_typeof(root) != MULIB_JSON_OBJECT
      || !mulib_jsonrpc_version_(mulib_json_get(root, "jsonrpc"))) return 0;
  view->method = mulib_json_get(root, "method");
  view->params = mulib_json_get(root, "params");
  view->id = mulib_json_get(root, "id");
  return 1;
}
#endif
