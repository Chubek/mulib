/**
 * @file mulib-mcp-http.h
 * @brief Bounded, socket-free MCP 2026-07-28 Streamable HTTP/1.1 binding.
 * Define MULIB_IMPL_CDD_MCP_HTTP once. See the manual for dependencies.
 */
#ifndef MULIB_MCP_HTTP_H
#define MULIB_MCP_HTTP_H
#include "mulib-http.h"
#include "mulib-mcp-client.h"
#include "mulib-mcp-server.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Accept or deny the supplied Origin (called only when present). NULL
   * policy rejects all browser Origins. Match an explicit allowlist. */
  typedef int (*mulib_cdd_mcp_origin_fn) (void *user, const char *origin);
  /** Must return 200 to allow, 401 if unauthenticated, or 403 to deny.
   * Set a borrowed principal for the duration of server_http. Return 200
   * with NULL principal only for an intentionally anonymous endpoint.
   * Do not authenticate via clientInfo, Host, or mirrored MCP headers. */
  typedef unsigned (*mulib_cdd_mcp_authenticate_fn) (
      void *user, const mulib_http_message_t *request, const void **principal);
  typedef struct mulib_cdd_mcp_http_config
  {
    const char *authority, *path; /* exact Host and origin-form path */
    const char *challenge; /* required WWW-Authenticate value if returning 401 */
    mulib_cdd_mcp_origin_fn origin;
    mulib_cdd_mcp_authenticate_fn authenticate; /* required */
    void *user;
    int sse; /* encode the final successful response as one SSE message */
  } mulib_cdd_mcp_http_config_t;

  /** Parse exactly one full HTTP request. On OK write a malloc-owned HTTP
   * response (including HTTP rejection statuses). Framing failure returns
   * INVALID or LIMIT and leaves output unchanged; host should close socket.
   * Current revision only; HTTP sessions and GET are not supported. The
   * catalogue is not mutated; never dispatch concurrently on one catalogue. */
  mulib_cdd_mcp_status_t mulib_cdd_mcp_server_http (
      const mulib_cdd_mcp_catalog_t *catalog, const mulib_cdd_mcp_http_config_t *config,
      const void *request, size_t request_length, unsigned char **out, size_t *length);

  /** Bind a previously built current revision client request to HTTP.
   * Pass the advertised input schema for tools/call to mirror its annotated
   * properties; NULL means no annotations. No URL discovery or I/O.
   * Both output arguments are unchanged on failure. */
  mulib_cdd_mcp_status_t mulib_cdd_mcp_client_http_request (
      const char *authority, const char *path, const char *authorization,
      const char *frame, size_t frame_length, const mulib_json_t *input_schema,
      unsigned char **out, size_t *length);

  /** Consume an entire bounded HTTP response (including final SSE event).
   * See client_receive for result ownership/status, except this current-only
   * binding never converts a remote error into a legacy-peer hint. Errors without a
   * correlated JSON-RPC response are INVALID; the host interprets HTTP
   * challenges, initiates authentication, and chooses whether to retry. */
  mulib_cdd_mcp_status_t mulib_cdd_mcp_client_http_receive (
      mulib_cdd_mcp_client_t *client, const void *response, size_t length,
      mulib_json_t **out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_CDD_MCP_HTTP) && !defined(MULIB_MCP_HTTP_IMPLEMENTED)
#define MULIB_MCP_HTTP_IMPLEMENTED
#include "mulib-sse.h"
#include "detail/mulib-jsonrpc-envelope.h"
#include "detail/mulib-mcp-wire.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C"
{
#endif
  #define MULIB_MCP_HTTP_MAX_BODY (MULIB_CDD_MCP_MAX_FRAME * 4u)
  #define MULIB_MCP_HTTP_MAX_PARAMS 32u

  static mulib_cdd_mcp_status_t
  mulib_mcp_http_status_ (int status)
  {
    switch (status)
      {
      case 1: return MULIB_CDD_MCP_OK;
      case -2: return MULIB_CDD_MCP_LIMIT;
      case -3: return MULIB_CDD_MCP_NOMEM;
      default: return MULIB_CDD_MCP_INVALID;
      }
  }

  static int
  mulib_mcp_http_ieq_n_ (const char *s, size_t n, const char *p)
  {
    if (n != strlen (p))
      return 0;
    for (size_t i = 0; i < n; ++i)
      {
        unsigned char a = (unsigned char)s[i], b = (unsigned char)p[i];
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + 'a' - 'A');
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + 'a' - 'A');
        if (a != b) return 0;
      }
    return 1;
  }

  static int
  mulib_mcp_http_ieq_ (const char *s, const char *p)
  {
    return s && mulib_mcp_http_ieq_n_ (s, strlen (s), p);
  }

  static int
  mulib_mcp_http_ows_ (char c)
  {
    return c == ' ' || c == '\t';
  }

  static int
  mulib_mcp_http_token_ (unsigned char c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
           || (c >= '0' && c <= '9') || (c && strchr ("!#$%&'*+-.^_`|~", c));
  }

  static const char *
  mulib_mcp_http_one_ (const mulib_http_message_t *m, const char *name, int *ok)
  {
    size_t index = 0;
    const char *value = mulib_http_header_next (m, name, &index);
    if (value && mulib_http_header_next (m, name, &index))
      *ok = 0;
    return value;
  }

  static int
  mulib_mcp_http_type_ (const char *s, const char *type)
  {
    const char *semi, *end;
    if (!s)
      return 0;
    semi = strchr (s, ';');
    end = semi ? semi : s + strlen (s);
    while (end > s && mulib_mcp_http_ows_ (end[-1])) --end;
    if (!mulib_mcp_http_ieq_n_ (s, (size_t)(end - s), type))
      return 0;
    if (!semi)
      return 1;
    s = semi + 1;
    while (mulib_mcp_http_ows_ (*s)) ++s;
    return mulib_mcp_http_ieq_ (s, "charset=utf-8")
           || mulib_mcp_http_ieq_ (s, "charset=\"utf-8\"");
  }

  /* Explicit acceptance of both transport formats; a q=0 item never counts. */
  static int
  mulib_mcp_http_accept_ (const mulib_http_message_t *m)
  {
    int mask = 0;
    size_t cursor = 0;
    const char *s;
    while ((s = mulib_http_header_next (m, "Accept", &cursor)) != NULL)
      while (*s)
        {
          const char *end = strchr (s, ',');
          const char *semi, *z;
          int item = 0, q = 1;
          if (!end) end = s + strlen (s);
          while (s < end && mulib_mcp_http_ows_ (*s)) ++s;
          semi = (const char *)memchr (s, ';', (size_t)(end - s));
          z = semi ? semi : end;
          while (z > s && mulib_mcp_http_ows_ (z[-1])) --z;
          if (mulib_mcp_http_ieq_n_ (s, (size_t)(z - s), "application/json")) item = 1;
          if (mulib_mcp_http_ieq_n_ (s, (size_t)(z - s), "text/event-stream")) item = 2;
          if (item && semi)
            {
              const char *p = semi + 1, *last = end;
              while (p < last && mulib_mcp_http_ows_ (*p)) ++p;
              while (last > p && mulib_mcp_http_ows_ (last[-1])) --last;
              if (last - p < 3 || (p[0] != 'q' && p[0] != 'Q') || p[1] != '='
                  || (p[2] != '0' && p[2] != '1') || last - p > 7)
                return 0;
              q = p[2] == '1';
              if (last - p > 3)
                {
                  if (p[3] != '.') return 0;
                  for (const char *d = p + 4; d < last; ++d)
                    {
                      if (*d < '0' || *d > '9' || (p[2] == '1' && *d != '0'))
                        return 0;
                      if (*d != '0') q = 1;
                    }
                }
            }
          if (item && q)
            mask |= item;
          s = *end ? end + 1 : end;
        }
    return mask == 3;
  }

  static int
  mulib_mcp_http_sentinel_ (const char *s, size_t n)
  {
    return n >= 11 && !memcmp (s, "=?base64?", 9) && !memcmp (s + n - 2, "?=", 2);
  }

  static int
  mulib_mcp_http_plain_ (const char *s, size_t n)
  {
    if ((n && (mulib_mcp_http_ows_ (s[0]) || mulib_mcp_http_ows_ (s[n - 1])))
        || mulib_mcp_http_sentinel_ (s, n))
      return 0;
    for (size_t i = 0; i < n; ++i)
      if (s[i] != '\t' && ((unsigned char)s[i] < 32 || (unsigned char)s[i] > 126))
        return 0;
    return 1;
  }

  static char *
  mulib_mcp_http_encoded_ (const char *s, size_t n)
  {
    char *v;
    if (mulib_mcp_http_plain_ (s, n))
      {
        v = (char *)malloc (n + 1);
        if (v) { memcpy (v, s, n); v[n] = 0; }
        return v;
      }
    size_t len = ((n + 2) / 3) * 4;
    v = (char *)malloc (len + 12);
    if (v)
      {
        memcpy (v, "=?base64?", 9);
        (void)mulib_base64_encode (s, n, v + 9, len + 1);
        memcpy (v + 9 + len, "?=", 3);
      }
    return v;
  }

  static int
  mulib_mcp_http_matches_ (const char *wire, const char *value, size_t n)
  {
    size_t len;
    if (!wire)
      return 0;
    len = strlen (wire);
    if (!mulib_mcp_http_sentinel_ (wire, len))
      return len == n && mulib_mcp_http_plain_ (wire, len) && !memcmp (wire, value, n);
    len -= 11;
    if (len != ((n + 2) / 3) * 4)
      return 0;
    for (size_t i = 0, pos = 0; i < len; i += 4)
      {
        unsigned char bytes[3];
        size_t got, want = n - pos < 3 ? n - pos : 3;
        if (!mulib_base64_decode (wire + 9 + i, 4, bytes, sizeof bytes, &got)
            || got != want || memcmp (value + pos, bytes, got))
          return 0;
        pos += got;
      }
    return 1;
  }

  typedef struct mulib_mcp_http_mirror_
  {
    char name[80], number[32];
    const char *value;
    size_t length;
  } mulib_mcp_http_mirror_;

  /* Walk all schema locations for misplaced annotations. Carry the instance
     path only across properties, never through refs or branch keywords. */
  static int
  mulib_mcp_http_schema_ (const mulib_json_t *node, const mulib_json_t *value,
                          int reachable, unsigned depth, mulib_mcp_http_mirror_ *params,
                          size_t *count)
  {
    const mulib_json_t *annot = mulib_json_get (node, "x-mcp-header");
    if (depth >= 48)
      return 0;
    if (annot)
      {
        const mulib_json_t *type = mulib_json_get (node, "type");
        const char *label = mulib_mcp_wire_label_ (annot, 64);
        int str = mulib_mcp_wire_is_ (type, "string");
        int boolean = mulib_mcp_wire_is_ (type, "boolean");
        int integer = mulib_mcp_wire_is_ (type, "integer");
        if (reachable != 1 || !label || *count == MULIB_MCP_HTTP_MAX_PARAMS
            || (!str && !boolean && !integer))
          return 0;
        for (size_t i = 0; label[i]; ++i)
          if (!mulib_mcp_http_token_ ((unsigned char)label[i]))
            return 0;
        mulib_mcp_http_mirror_ *p = &params[*count];
        (void)snprintf (p->name, sizeof p->name, "Mcp-Param-%s", label);
        for (size_t i = 0; i < *count; ++i)
          if (mulib_mcp_http_ieq_ (params[i].name, p->name))
            return 0;
        ++*count;
        p->value = NULL;
        p->length = 0;
        if (mulib_json_typeof (value) != MULIB_JSON_NULL)
          {
            if (str)
              p->value = mulib_json_text (value, &p->length);
            else if (boolean && mulib_json_typeof (value) == MULIB_JSON_BOOL)
              {
                p->value = mulib_json_boolean (value) ? "true" : "false";
                p->length = strlen (p->value);
              }
            else if (integer && mulib_mcp_wire_integer_ (value))
              {
                (void)snprintf (p->number, sizeof p->number, "%lld",
                                (long long)mulib_json_numeric (value));
                p->value = p->number;
                p->length = strlen (p->value);
              }
            if (!p->value)
              return 0;
          }
      }
    for (size_t i = 0; i < mulib_json_size (node); ++i)
      {
        size_t len = 0;
        const char *key = mulib_json_key (node, i, &len);
        const mulib_json_t *child = mulib_json_at (node, i);
        /* A properties map's keys are argument names, not schema keywords. */
        if (key && len == 10 && !memcmp (key, "properties", 10) && reachable >= 0
            && mulib_json_typeof (child) == MULIB_JSON_OBJECT)
          {
            for (size_t j = 0; j < mulib_json_size (child); ++j)
              {
                const char *k = mulib_json_key (child, j, &len);
                if (!mulib_mcp_http_schema_ (
                        mulib_json_at (child, j), mulib_json_get_n (value, k, len),
                        1, depth + 1, params, count))
                  return 0;
              }
          }
        else if (!mulib_mcp_http_schema_ (child, NULL, -1, depth + 1, params, count))
          return 0;
      }
    return 1;
  }

  static const mulib_json_t *
  mulib_mcp_http_target_ (const mulib_json_t *rq, const char *method)
  {
    return mulib_json_get (mulib_json_get (rq, "params"),
                           !strcmp (method, "resources/read") ? "uri" : "name");
  }

  static int
  mulib_mcp_http_named_ (const char *method)
  {
    return !strcmp (method, "tools/call") || !strcmp (method, "resources/read")
           || !strcmp (method, "prompts/get");
  }

  static int
  mulib_mcp_http_method_ (const char *method)
  {
    if (!method || !*method)
      return 0;
    for (size_t i = 0; method[i]; ++i)
      if (method[i] != '/' && !mulib_mcp_http_token_ ((unsigned char)method[i]))
        return 0;
    return 1;
  }

  static int
  mulib_mcp_http_endpoint_ (const char *authority, const char *path)
  {
    return mulib_mcp_wire_cstr_ (authority, 512) && *authority
           && !strpbrk (authority, "/\\?#@, \t\r\n")
           && mulib_mcp_wire_cstr_ (path, 4096) && path[0] == '/' && path[1] != '/'
           && !strpbrk (path, "?# \t\r\n");
  }

  static void
  mulib_mcp_http_header_ (mulib_http_header_t *h, const char *name, const char *value)
  {
    /* The encoder only borrows its inputs; no ownership/mutation here. */
    h->name = (char *)name;
    h->value = (char *)value;
  }

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_client_http_request (
      const char *authority, const char *path, const char *authorization,
      const char *frame, size_t n, const mulib_json_t *schema,
      unsigned char **out, size_t *length)
  {
    mulib_json_error_t error = { 0, 0, 0, NULL };
    mulib_json_t *rq;
    mulib_jsonrpc_envelope_ envelope;
    const mulib_json_t *params;
    const char *method;
    mulib_mcp_http_mirror_ mirror[MULIB_MCP_HTTP_MAX_PARAMS];
    size_t count = 0, headers = 0, frees = 0;
    mulib_http_header_t h[7 + MULIB_MCP_HTTP_MAX_PARAMS];
    char *allocated[1 + MULIB_MCP_HTTP_MAX_PARAMS];
    mulib_http_message_t m;
    mulib_cdd_mcp_status_t rc;
    if (!out || !length || !mulib_mcp_http_endpoint_ (authority, path))
      return MULIB_CDD_MCP_INVALID;
    rc = mulib_mcp_wire_frame_ (frame, n);
    if (rc != MULIB_CDD_MCP_OK)
      return rc;
    rq = mulib_mcp_wire_parse_ (frame, n, &error);
    if (!rq)
      return mulib_mcp_wire_parse_nomem_ (&error) ? MULIB_CDD_MCP_NOMEM
                                                 : MULIB_CDD_MCP_INVALID;
    rc = MULIB_CDD_MCP_INVALID;
    if (!mulib_jsonrpc_envelope_view_ (rq, &envelope))
      goto done;
    params = envelope.params;
    method = mulib_mcp_wire_label_ (envelope.method, 256);
    if (!mulib_mcp_http_method_ (method) || !mulib_mcp_wire_id_ (envelope.id)
        || !mulib_mcp_wire_is_ (mulib_json_get (mulib_json_get (params, "_meta"),
                                              MULIB_MCP_WIRE_VERSION), MULIB_CDD_MCP_VERSION)
        || mulib_json_get (rq, "error") || mulib_json_get (rq, "result"))
      goto done;
    if (schema && (strcmp (method, "tools/call")
                   || !mulib_mcp_wire_is_ (mulib_json_get (schema, "type"), "object")))
      goto done;
    if (schema && !mulib_mcp_http_schema_ (schema, mulib_json_get (params, "arguments"),
                                          0, 0, mirror, &count))
      goto done;
    mulib_mcp_http_header_ (&h[headers++], "Host", authority);
    mulib_mcp_http_header_ (&h[headers++], "Content-Type", "application/json");
    mulib_mcp_http_header_ (&h[headers++], "Accept", "application/json, text/event-stream");
    mulib_mcp_http_header_ (&h[headers++], "MCP-Protocol-Version", MULIB_CDD_MCP_VERSION);
    mulib_mcp_http_header_ (&h[headers++], "Mcp-Method", method);
    if (authorization)
      {
        if (!*authorization || !mulib_mcp_wire_cstr_ (authorization, 8192)
            || !mulib_mcp_http_plain_ (authorization, strlen (authorization)))
          goto done;
        mulib_mcp_http_header_ (&h[headers++], "Authorization", authorization);
      }
    if (mulib_mcp_http_named_ (method))
      {
        size_t z;
        const char *name = mulib_json_text (mulib_mcp_http_target_ (rq, method), &z);
        if (!name || !z || z > 4096)
          goto done;
        allocated[frees] = mulib_mcp_http_encoded_ (name, z);
        if (!allocated[frees]) { rc = MULIB_CDD_MCP_NOMEM; goto done; }
        mulib_mcp_http_header_ (&h[headers++], "Mcp-Name", allocated[frees++]);
      }
    for (size_t i = 0; i < count; ++i)
      if (mirror[i].value)
        {
          allocated[frees] = mulib_mcp_http_encoded_ (mirror[i].value, mirror[i].length);
          if (!allocated[frees]) { rc = MULIB_CDD_MCP_NOMEM; goto done; }
          mulib_mcp_http_header_ (&h[headers++], mirror[i].name, allocated[frees++]);
        }
    memset (&m, 0, sizeof m);
    m.method = (char *)"POST";
    m.target = (char *)path;
    m.headers = h;
    m.header_count = headers;
    m.body = (unsigned char *)frame;
    m.body_length = n;
    rc = mulib_mcp_http_status_ (mulib_http_serialize (&m, NULL, out, length));
  done:
    for (size_t i = 0; i < frees; ++i)
      free (allocated[i]);
    mulib_json_destroy (rq);
    return rc;
  }

  static int
  mulib_mcp_http_origin_ (const char *s)
  {
    mulib_uri_t uri;
    size_t n;
    if (!s)
      return 1;
    n = strlen (s);
    return n && n <= 1024 && mulib_uri_parse (s, n, &uri)
           && ((uri.scheme.length == 4 && !memcmp (s, "http", 4))
               || (uri.scheme.length == 5 && !memcmp (s, "https", 5)))
           && uri.has_authority && uri.host.length && !uri.has_userinfo
           && (!uri.has_port || uri.port.length)
           && !uri.path.length && !uri.has_query && !uri.has_fragment;
  }

  static mulib_json_t *
  mulib_mcp_http_error_ (const mulib_json_t *id, int code, const char *message,
                         const char *version)
  {
    mulib_json_t *r = mulib_mcp_wire_object_ (), *error = mulib_mcp_wire_object_ ();
    if (!r || !error || !mulib_mcp_wire_str_ (r, "jsonrpc", "2.0")
        || (mulib_mcp_wire_id_ (id)
            && !mulib_mcp_wire_put_ (r, "id", mulib_mcp_wire_clone_ (id)))
        || !mulib_mcp_wire_put_ (error, "code", mulib_json_number ((double)code))
        || !mulib_mcp_wire_str_ (error, "message", message))
      goto fail;
    if (version)
      {
        mulib_json_t *data = mulib_mcp_wire_object_ ();
        mulib_json_t *list = mulib_json_new (MULIB_JSON_ARRAY);
        if (!data || !list
            || !mulib_mcp_wire_append_ (list, mulib_json_string (MULIB_CDD_MCP_VERSION))
            || !mulib_mcp_wire_str_ (data, "requested", version)
            || !mulib_json_set (data, "supported", list))
          {
            mulib_json_destroy (list);
            mulib_json_destroy (data);
            goto fail;
          }
        if (!mulib_mcp_wire_put_ (error, "data", data))
          goto fail;
      }
    if (mulib_json_set (r, "error", error))
      return r;
  fail:
    mulib_json_destroy (error);
    mulib_json_destroy (r);
    return NULL;
  }

  static const mulib_json_t *
  mulib_mcp_http_tool_schema_ (const mulib_cdd_mcp_catalog_t *c, const char *name)
  {
    for (size_t i = 0; i < c->server.config.tool_count; ++i)
      if (!strcmp (c->server.config.tools[i].name, name))
        return c->server.config.tools[i].input_schema;
    for (size_t i = 0; i < c->features.tool_count; ++i)
      if (!strcmp (c->features.tools[i].name, name))
        return c->features.tools[i].input_schema;
    return NULL;
  }

  static unsigned
  mulib_mcp_http_code_ (double code)
  {
    if (code == -32601) return 404;
    if (code == -32000) return 403; /* local explicit authorization denial */
    if (code == -32020 || code == -32021 || code == -32022
        || code == -32700 || code == -32600) return 400;
    return 200;
  }

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_server_http (
      const mulib_cdd_mcp_catalog_t *catalog, const mulib_cdd_mcp_http_config_t *config,
      const void *request, size_t request_length, unsigned char **out, size_t *length)
  {
    mulib_http_message_t m, response;
    const mulib_http_options_t opts = { 0, 0, MULIB_CDD_MCP_MAX_FRAME, MULIB_HTTP_EOF };
    const mulib_json_t *id = NULL, *params, *meta;
    mulib_json_t *rq = NULL, *result = NULL;
    mulib_jsonrpc_envelope_ envelope;
    mulib_json_error_t error = { 0, 0, 0, NULL };
    mulib_cdd_mcp_status_t rc = MULIB_CDD_MCP_OK;
    const void *principal = NULL;
    const char *host, *origin, *method, *wire_version, *wire_method, *wire_name;
    const char *version, *encoding, *content;
    char *frame = NULL, *body = NULL, *sse_body = NULL;
    size_t used, hcount = 0, body_len = 0;
    mulib_http_header_t headers[4];
    unsigned status;
    int one = 1;
    if (!catalog || !catalog->server.config.name || !config || !config->authenticate
        || !out || !length || !mulib_mcp_http_endpoint_ (config->authority, config->path)
        || (config->challenge && (!*config->challenge
            || !mulib_mcp_wire_cstr_ (config->challenge, 4096)
            || !mulib_mcp_http_plain_ (config->challenge, strlen (config->challenge)))))
      return MULIB_CDD_MCP_INVALID;
    rc = mulib_mcp_http_status_ (
        mulib_http_parse (request, request_length, 0, &opts, &m, &used));
    if (rc != MULIB_CDD_MCP_OK)
      return rc;
    if (used != request_length) { rc = MULIB_CDD_MCP_INVALID; goto done; }
    status = 400;
    host = mulib_mcp_http_one_ (&m, "Host", &one);
    if (!one || !mulib_mcp_http_ieq_ (host, config->authority))
      goto respond;
    origin = mulib_mcp_http_one_ (&m, "Origin", &one);
    if (!one || (origin && (!mulib_mcp_http_origin_ (origin) || !config->origin
                            || !config->origin (config->user, origin))))
      { status = 403; goto respond; }
    if (strcmp (m.target, config->path)) { status = 404; goto respond; }
    if (strcmp (m.method, "POST")) { status = 405; goto respond; }
    (void)mulib_mcp_http_one_ (&m, "Authorization", &one);
    if (!one)
      goto respond;
    status = config->authenticate (config->user, &m, &principal);
    if (status != 200)
      {
        if ((status != 401 && status != 403) || (status == 401 && !config->challenge))
          { rc = MULIB_CDD_MCP_INVALID; goto done; }
        goto respond;
      }
    status = 415;
    encoding = mulib_mcp_http_one_ (&m, "Content-Encoding", &one);
    content = mulib_mcp_http_one_ (&m, "Content-Type", &one);
    if (!one || (encoding && !mulib_mcp_http_ieq_ (encoding, "identity"))
        || !mulib_mcp_http_type_ (content, "application/json"))
      goto respond;
    if (!mulib_mcp_http_accept_ (&m)) { status = 406; goto respond; }
    status = 400;
    rq = mulib_mcp_wire_parse_ (m.body ? (const char *)m.body : "", m.body_length, &error);
    if (!rq)
      {
        if (mulib_mcp_wire_parse_nomem_ (&error)) { rc = MULIB_CDD_MCP_NOMEM; goto done; }
        result = mulib_mcp_http_error_ (NULL, -32700, "Parse error", NULL);
        goto format_error;
      }
    id = mulib_json_get (rq, "id");
    if (!mulib_jsonrpc_envelope_view_ (rq, &envelope) || !mulib_mcp_wire_id_ (id)
        || !mulib_mcp_http_method_ (method = mulib_mcp_wire_label_ (envelope.method, 256))
        || mulib_json_get (rq, "result") || mulib_json_get (rq, "error"))
      {
        result = mulib_mcp_http_error_ (id, -32600, "Invalid HTTP JSON-RPC request", NULL);
        goto format_error;
      }
    params = envelope.params;
    meta = mulib_json_get (params, "_meta");
    version = mulib_mcp_wire_label_ (mulib_json_get (meta, MULIB_MCP_WIRE_VERSION), 128);
    wire_version = mulib_mcp_http_one_ (&m, "MCP-Protocol-Version", &one);
    wire_method = mulib_mcp_http_one_ (&m, "Mcp-Method", &one);
    wire_name = mulib_mcp_http_one_ (&m, "Mcp-Name", &one);
    if (!one || !wire_version || !version || strcmp (wire_version, version)
        || !wire_method || strcmp (wire_method, method)
        || !mulib_mcp_http_plain_ (wire_version, strlen (wire_version)))
      goto mismatch;
    if (strcmp (version, MULIB_CDD_MCP_VERSION))
      {
        result = mulib_mcp_http_error_ (id, -32022, "Unsupported protocol version", version);
        goto format_error;
      }
    if (mulib_mcp_http_named_ (method))
      {
        size_t n;
        const char *name = mulib_json_text (mulib_mcp_http_target_ (rq, method), &n);
        if (!name || !n || n > 4096 || !mulib_mcp_http_matches_ (wire_name, name, n))
          goto mismatch;
        if (!strcmp (method, "tools/call"))
          {
            const mulib_json_t *schema = mulib_mcp_http_tool_schema_ (catalog, name);
            mulib_mcp_http_mirror_ mirror[MULIB_MCP_HTTP_MAX_PARAMS];
            size_t count = 0;
            if (schema && !mulib_mcp_http_schema_ (
                              schema, mulib_json_get (params, "arguments"), 0, 0, mirror, &count))
              goto mismatch;
            for (size_t i = 0; i < count; ++i)
              {
                const char *actual = mulib_mcp_http_one_ (&m, mirror[i].name, &one);
                if (!one || (mirror[i].value
                                 ? !mulib_mcp_http_matches_ (
                                       actual, mirror[i].value, mirror[i].length)
                                 : actual != NULL))
                  goto mismatch;
              }
          }
      }
    else if (wire_name)
      goto mismatch;
    rc = mulib_mcp_wire_format_ (rq, &frame);
    if (rc != MULIB_CDD_MCP_OK)
      goto done;
    {
      mulib_cdd_mcp_catalog_t copy = *catalog;
      copy.server.config.allow_legacy = 0;
      copy.server.legacy_state = 0;
      rc = mulib_cdd_mcp_catalog_process (&copy, principal, frame, strlen (frame), &body);
    }
    if (rc != MULIB_CDD_MCP_OK)
      goto done;
    if (!body) { rc = MULIB_CDD_MCP_INVALID; goto done; }
    result = mulib_json_parse (body, strlen (body), &error);
    if (!result)
      {
        rc = mulib_mcp_wire_parse_nomem_ (&error) ? MULIB_CDD_MCP_NOMEM : MULIB_CDD_MCP_INVALID;
        goto done;
      }
    status = mulib_mcp_http_code_ (mulib_json_numeric (
        mulib_json_get (mulib_json_get (result, "error"), "code")));
    goto respond;
  mismatch:
    result = mulib_mcp_http_error_ (id, -32020, "Missing or mismatched MCP header", NULL);
  format_error:
    if (!result) { rc = MULIB_CDD_MCP_NOMEM; goto done; }
    rc = mulib_mcp_wire_format_ (result, &body);
    if (rc != MULIB_CDD_MCP_OK)
      goto done;
  respond:
    if (status == 405)
      mulib_mcp_http_header_ (&headers[hcount++], "Allow", "POST");
    if (status == 401)
      mulib_mcp_http_header_ (&headers[hcount++], "WWW-Authenticate", config->challenge);
    mulib_mcp_http_header_ (&headers[hcount++], "Cache-Control", "no-store");
    if (body)
      {
        body_len = strlen (body);
        if (status == 200 && config->sse)
          {
            const mulib_sse_event_t e = { NULL, NULL, body, body_len };
            rc = mulib_mcp_http_status_ (
                mulib_sse_encode (&e, MULIB_CDD_MCP_MAX_FRAME + 32u, &sse_body, &body_len));
            if (rc != MULIB_CDD_MCP_OK)
              goto done;
            mulib_mcp_http_header_ (&headers[hcount++], "X-Accel-Buffering", "no");
          }
        mulib_mcp_http_header_ (&headers[hcount++], "Content-Type",
                                sse_body ? "text/event-stream" : "application/json");
      }
    memset (&response, 0, sizeof response);
    response.is_response = 1;
    response.status = status;
    switch (status)
      {
      case 200: response.reason = (char *)"OK"; break;
      case 401: response.reason = (char *)"Unauthorized"; break;
      case 403: response.reason = (char *)"Forbidden"; break;
      case 404: response.reason = (char *)"Not Found"; break;
      case 405: response.reason = (char *)"Method Not Allowed"; break;
      case 406: response.reason = (char *)"Not Acceptable"; break;
      case 415: response.reason = (char *)"Unsupported Media Type"; break;
      default: response.reason = (char *)"Bad Request"; break;
      }
    response.headers = headers;
    response.header_count = hcount;
    response.body = (unsigned char *)(sse_body ? sse_body : body);
    response.body_length = body_len;
    rc = mulib_mcp_http_status_ (mulib_http_serialize (&response, NULL, out, length));
  done:
    free (sse_body);
    free (body);
    free (frame);
    mulib_json_destroy (result);
    mulib_json_destroy (rq);
    mulib_http_message_free (&m);
    return rc;
  }

  typedef struct mulib_mcp_http_reception_
  {
    mulib_cdd_mcp_client_t client;
    mulib_json_t *response;
    mulib_cdd_mcp_status_t status;
    unsigned http_status;
  } mulib_mcp_http_reception_;

  static int
  mulib_mcp_http_receive_event_ (mulib_mcp_http_reception_ *s, const char *text,
                                 size_t n, int sse)
  {
    mulib_json_error_t error = { 0, 0, 0, NULL };
    mulib_json_t *msg;
    const mulib_json_t *method;
    char *frame = NULL;
    s->status = MULIB_CDD_MCP_INVALID;
    if (s->response || !text)
      return 0;
    if (n > MULIB_CDD_MCP_MAX_FRAME)
      { s->status = MULIB_CDD_MCP_LIMIT; return 0; }
    msg = mulib_mcp_wire_parse_ (text, n, &error);
    if (!msg)
      {
        if (mulib_mcp_wire_parse_nomem_ (&error)) s->status = MULIB_CDD_MCP_NOMEM;
        return 0;
      }
    method = mulib_json_get (msg, "method");
    if (method)
      {
        int ok = sse && mulib_mcp_wire_label_ (method, 256)
                 && mulib_mcp_wire_is_ (mulib_json_get (msg, "jsonrpc"), "2.0")
                 && !mulib_json_get (msg, "id") && !mulib_json_get (msg, "result")
                 && !mulib_json_get (msg, "error")
                 && mulib_mcp_wire_opt_ (msg, "params", MULIB_JSON_OBJECT);
        mulib_json_destroy (msg);
        return ok;
      }
    {
      const mulib_json_t *e = mulib_json_get (msg, "error");
      const double code = mulib_json_numeric (mulib_json_get (e, "code"));
      if ((s->http_status != 200 && (!e || s->http_status != mulib_mcp_http_code_ (code)))
          || (s->http_status == 200
              && (code == -32601 || code == -32020 || code == -32021 || code == -32022)))
        { mulib_json_destroy (msg); return 0; }
    }
    s->status = mulib_mcp_wire_format_ (msg, &frame);
    mulib_json_destroy (msg);
    if (s->status == MULIB_CDD_MCP_OK)
      s->status = mulib_cdd_mcp_client_receive (&s->client, frame, strlen (frame), &s->response);
    free (frame);
    return s->response != NULL;
  }

  static int
  mulib_mcp_http_event_ (void *data, const mulib_sse_event_t *e)
  {
    mulib_mcp_http_reception_ *s = (mulib_mcp_http_reception_ *)data;
    if (strcmp (e->event, "message") || *e->id)
      { s->status = MULIB_CDD_MCP_INVALID; return 0; }
    return mulib_mcp_http_receive_event_ (s, e->data, e->data_length, 1);
  }

  static int
  mulib_mcp_http_filter_tools_ (mulib_json_t *r)
  {
    mulib_json_t *result = mulib_json_get (r, "result");
    const mulib_json_t *list = mulib_json_get (result, "tools");
    mulib_json_t *filtered = NULL;
    if (mulib_json_typeof (list) != MULIB_JSON_ARRAY)
      return 1;
    for (size_t i = 0; i < mulib_json_size (list); ++i)
      {
        const mulib_json_t *tool = mulib_json_at (list, i);
        const mulib_json_t *schema = mulib_json_get (tool, "inputSchema");
        mulib_mcp_http_mirror_ mirror[MULIB_MCP_HTTP_MAX_PARAMS];
        size_t count = 0;
        if (!mulib_mcp_http_schema_ (schema, NULL, 0, 0, mirror, &count))
          {
            if (!filtered)
              {
                filtered = mulib_json_new (MULIB_JSON_ARRAY);
                if (!filtered) return 0;
                for (size_t j = 0; j < i; ++j)
                  if (!mulib_mcp_wire_append_ (filtered, mulib_mcp_wire_clone_ (mulib_json_at (list, j))))
                    { mulib_json_destroy (filtered); return 0; }
              }
          }
        else if (filtered && !mulib_mcp_wire_append_ (filtered, mulib_mcp_wire_clone_ (tool)))
          { mulib_json_destroy (filtered); return 0; }
      }
    return !filtered || mulib_mcp_wire_put_ (result, "tools", filtered);
  }

  mulib_cdd_mcp_status_t
  mulib_cdd_mcp_client_http_receive (mulib_cdd_mcp_client_t *client, const void *response,
                                     size_t n, mulib_json_t **out)
  {
    mulib_http_message_t m;
    const mulib_http_options_t opts = { 0, 0, MULIB_MCP_HTTP_MAX_BODY, MULIB_HTTP_EOF };
    size_t used;
    const char *content, *encoding;
    int one = 1;
    mulib_mcp_http_reception_ ref;
    mulib_cdd_mcp_status_t rc;
    if (!client || !client->name || !client->pending || !out)
      return MULIB_CDD_MCP_INVALID;
    rc = mulib_mcp_http_status_ (mulib_http_parse (response, n, 1, &opts, &m, &used));
    if (rc != MULIB_CDD_MCP_OK)
      return rc;
    ref.client = *client;
    ref.response = NULL;
    ref.http_status = m.status;
    ref.status = MULIB_CDD_MCP_INVALID;
    content = mulib_mcp_http_one_ (&m, "Content-Type", &one);
    encoding = mulib_mcp_http_one_ (&m, "Content-Encoding", &one);
    if (!one || used != n || (encoding && !mulib_mcp_http_ieq_ (encoding, "identity")))
      goto done;
    if (mulib_mcp_http_type_ (content, "application/json"))
      (void)mulib_mcp_http_receive_event_ (&ref, (const char *)m.body, m.body_length, 0);
    else if (m.status == 200 && mulib_mcp_http_type_ (content, "text/event-stream"))
      {
        mulib_sse_parser_t *s = mulib_sse_create (MULIB_CDD_MCP_MAX_FRAME + 32u,
                                                  mulib_mcp_http_event_, &ref);
        int status;
        if (!s) { ref.status = MULIB_CDD_MCP_NOMEM; goto done; }
        status = mulib_sse_feed (s, m.body, m.body_length);
        if (status == 1)
          status = mulib_sse_finish (s);
        mulib_sse_destroy (s);
        if (status != 1)
          {
            if (status != -7)
              ref.status = mulib_mcp_http_status_ (status);
            mulib_json_destroy (ref.response);
            ref.response = NULL;
          }
      }
    if (ref.response && (client->pending & 255u) == MULIB_CDD_MCP_TOOLS_LIST + 1
        && !mulib_mcp_http_filter_tools_ (ref.response))
      {
        mulib_json_destroy (ref.response);
        ref.response = NULL;
        ref.status = MULIB_CDD_MCP_NOMEM;
      }
    if (ref.response)
      {
        if (ref.status == MULIB_CDD_MCP_LEGACY_PEER)
          {
            ref.status = MULIB_CDD_MCP_REMOTE_ERROR;
            ref.client.phase = client->phase;
          }
        *client = ref.client;
        *out = ref.response;
      }
  done:
    mulib_http_message_free (&m);
    return ref.status == MULIB_CDD_MCP_IGNORED ? MULIB_CDD_MCP_INVALID : ref.status;
  }
  #undef MULIB_MCP_HTTP_MAX_BODY
  #undef MULIB_MCP_HTTP_MAX_PARAMS
#ifdef __cplusplus
}
#endif
#endif /* implementation */
