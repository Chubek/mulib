/* Private, translation-unit-local MCP JSON-RPC helpers. */
#ifndef MULIB_MCP_WIRE_H
#define MULIB_MCP_WIRE_H
#include "../mulib-uri.h"
#include "../mulib-base64.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MULIB_MCP_WIRE_VERSION "io.modelcontextprotocol/protocolVersion"
#define MULIB_MCP_WIRE_CLIENT "io.modelcontextprotocol/clientInfo"
#define MULIB_MCP_WIRE_CAPS "io.modelcontextprotocol/clientCapabilities"
#define MULIB_MCP_WIRE_SERVER "io.modelcontextprotocol/serverInfo"
#define MULIB_MCP_WIRE_LOG "io.modelcontextprotocol/logLevel"
#define MULIB_MCP_WIRE_PAYLOAD (MULIB_CDD_MCP_MAX_FRAME - 11000u)
/* Shared private capability mask (public client storage remains ABI-sized). */
#define MULIB_MCP_WIRE_TOOLS 1
#define MULIB_MCP_WIRE_RESOURCES 2
#define MULIB_MCP_WIRE_PROMPTS 4
#define MULIB_MCP_WIRE_COMPLETIONS 8

static inline int
mulib_mcp_wire_is_ (const mulib_json_t *v, const char *s)
{
  size_t n;
  const char *p = mulib_json_text (v, &n);
  return p && n == strlen (s) && !memcmp (p, s, n);
}

static inline int
mulib_mcp_wire_integer_ (const mulib_json_t *v)
{
  /* Round-trips exactly through MuLIB JSON's double-valued numbers. */
  double d = mulib_json_numeric (v);
  return mulib_json_typeof (v) == MULIB_JSON_NUMBER
         && d >= -9007199254740991.0 && d <= 9007199254740991.0
         && d == (double)(int64_t)d;
}

static inline int
mulib_mcp_wire_id_ (const mulib_json_t *v)
{
  size_t n;
  return mulib_json_text (v, &n) ? n <= 1024
                                 : mulib_mcp_wire_integer_ (v);
}

static inline mulib_json_t *
mulib_mcp_wire_object_ (void)
{
  return mulib_json_new (MULIB_JSON_OBJECT);
}

/* Take ownership even if inserting fails. */
static inline int
mulib_mcp_wire_put_ (mulib_json_t *o, const char *key, mulib_json_t *v)
{
  if (v && mulib_json_set (o, key, v))
    return 1;
  mulib_json_destroy (v);
  return 0;
}

static inline int
mulib_mcp_wire_str_ (mulib_json_t *o, const char *key, const char *value)
{
  return mulib_mcp_wire_put_ (o, key, mulib_json_string (value));
}

static inline int
mulib_mcp_wire_append_ (mulib_json_t *a, mulib_json_t *v)
{
  if (v && mulib_json_append (a, v))
    return 1;
  mulib_json_destroy (v);
  return 0;
}

static inline int
mulib_mcp_wire_info_valid_ (const mulib_json_t *info)
{
  return mulib_json_typeof (info) == MULIB_JSON_OBJECT
         && mulib_json_typeof (mulib_json_get (info, "name")) == MULIB_JSON_STRING
         && mulib_json_typeof (mulib_json_get (info, "version")) == MULIB_JSON_STRING;
}

static inline int
mulib_mcp_wire_cstr_ (const char *s, size_t limit)
{
  size_t n = 0;
  if (!s)
    return 0;
  while (n <= limit && s[n])
    ++n;
  return n <= limit && mulib_utf8_validate ((const unsigned char *)s, n);
}

static inline int
mulib_mcp_wire_string_ (const mulib_json_t *v)
{
  return mulib_json_typeof (v) == MULIB_JSON_STRING;
}

static inline int
mulib_mcp_wire_opt_ (const mulib_json_t *o, const char *k, mulib_json_type t)
{
  const mulib_json_t *v = mulib_json_get (o, k);
  return !v || mulib_json_typeof (v) == t;
}

/* A label is passed to host callbacks as a C string, never a truncated ID. */
static inline const char *
mulib_mcp_wire_label_ (const mulib_json_t *v, size_t limit)
{
  size_t n;
  const char *s = mulib_json_text (v, &n);
  return s && n && n <= limit && !memchr (s, 0, n) ? s : NULL;
}

static inline int
mulib_mcp_wire_uri_ (const mulib_json_t *v)
{
  size_t n;
  const char *s = mulib_json_text (v, &n);
  mulib_uri_t parsed;
  return s && n && n <= 4096 && mulib_uri_parse (s, n, &parsed)
         && parsed.scheme.length != 0;
}

static inline int
mulib_mcp_wire_hexdigit_ (char c)
{
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* Validate RFC 6570 expression syntax; validate the remaining ASCII
   literal layout as an absolute URI with placeholder expression values.
   Expansion and inverse matching belong to the host. */
static inline int
mulib_mcp_wire_template_ (const mulib_json_t *v)
{
  const char *s = mulib_mcp_wire_label_ (v, 4096);
  char literal[4097];
  size_t used = 0;
  mulib_uri_t uri;
  if (!s)
    return 0;
  while (*s)
    {
      if (*s != '{')
        {
          literal[used++] = *s++;
          continue;
        }
      ++s;
      if (*s && strchr ("+#./;?&", *s))
        ++s;
      for (;;)
        {
          int component = 0;
          while (*s)
            {
              char c = *s;
              if (c == '.' && component)
                component = 0;
              else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                       || (c >= '0' && c <= '9') || c == '_')
                component = 1;
              else if (c == '%' && mulib_mcp_wire_hexdigit_ (s[1])
                       && mulib_mcp_wire_hexdigit_ (s[2]))
                { s += 2; component = 1; }
              else
                break;
              ++s;
            }
          if (!component)
            return 0;
          if (*s == '*')
            ++s;
          else if (*s == ':')
            {
              unsigned digits = 0;
              ++s;
              if (*s < '1' || *s > '9')
                return 0;
              while (*s >= '0' && *s <= '9' && digits < 4)
                { ++s; ++digits; }
            }
          if (*s == '}')
            break;
          if (*s++ != ',')
            return 0;
        }
      ++s;
      literal[used++] = 'x';
    }
  return mulib_uri_parse (literal, used, &uri) && uri.scheme.length;
}

static inline int
mulib_mcp_wire_blob_ (const mulib_json_t *v)
{
  size_t n, decoded;
  const char *s = mulib_json_text (v, &n);
  unsigned char bytes[3];
  if (!s || n % 4)
    return 0;
  for (size_t i = 0; i < n; i += 4)
    if (!mulib_base64_decode (s + i, 4, bytes, sizeof bytes, &decoded)
        || (decoded != 3 && n - i != 4))
      return 0;
  return 1;
}

static inline int
mulib_mcp_wire_resource_ (const mulib_json_t *v, int templated)
{
  return mulib_mcp_wire_label_ (mulib_json_get (v, "name"), 4096)
         && (templated ? mulib_mcp_wire_template_ (mulib_json_get (v, "uriTemplate"))
                       : mulib_mcp_wire_uri_ (mulib_json_get (v, "uri")))
         && mulib_mcp_wire_opt_ (v, "title", MULIB_JSON_STRING)
         && mulib_mcp_wire_opt_ (v, "description", MULIB_JSON_STRING)
         && mulib_mcp_wire_opt_ (v, "mimeType", MULIB_JSON_STRING)
         && mulib_mcp_wire_opt_ (v, "_meta", MULIB_JSON_OBJECT);
}

static inline int
mulib_mcp_wire_contents_ (const mulib_json_t *v)
{
  const mulib_json_t *text = mulib_json_get (v, "text");
  const mulib_json_t *blob = mulib_json_get (v, "blob");
  return mulib_mcp_wire_uri_ (mulib_json_get (v, "uri"))
         && mulib_mcp_wire_opt_ (v, "mimeType", MULIB_JSON_STRING)
         && mulib_mcp_wire_opt_ (v, "_meta", MULIB_JSON_OBJECT)
         && (text ? !blob && mulib_mcp_wire_string_ (text)
                  : mulib_mcp_wire_blob_ (blob));
}

static inline int
mulib_mcp_wire_content_ (const mulib_json_t *v)
{
  const mulib_json_t *type = mulib_json_get (v, "type");
  if (!mulib_mcp_wire_opt_ (v, "_meta", MULIB_JSON_OBJECT)
      || !mulib_mcp_wire_opt_ (v, "annotations", MULIB_JSON_OBJECT))
    return 0;
  if (mulib_mcp_wire_is_ (type, "text"))
    return mulib_mcp_wire_string_ (mulib_json_get (v, "text"));
  if (mulib_mcp_wire_is_ (type, "image") || mulib_mcp_wire_is_ (type, "audio"))
    return mulib_mcp_wire_blob_ (mulib_json_get (v, "data"))
           && mulib_mcp_wire_string_ (mulib_json_get (v, "mimeType"));
  if (mulib_mcp_wire_is_ (type, "resource"))
    return mulib_mcp_wire_contents_ (mulib_json_get (v, "resource"));
  return mulib_mcp_wire_is_ (type, "resource_link") && mulib_mcp_wire_resource_ (v, 0);
}

static inline int
mulib_mcp_wire_prompt_ (const mulib_json_t *v)
{
  const mulib_json_t *args = mulib_json_get (v, "arguments");
  if (!mulib_mcp_wire_label_ (mulib_json_get (v, "name"), 4096)
      || !mulib_mcp_wire_opt_ (v, "title", MULIB_JSON_STRING)
      || !mulib_mcp_wire_opt_ (v, "description", MULIB_JSON_STRING)
      || !mulib_mcp_wire_opt_ (v, "_meta", MULIB_JSON_OBJECT)
      || (args && (mulib_json_typeof (args) != MULIB_JSON_ARRAY
                   || mulib_json_size (args) > MULIB_CDD_MCP_MAX_ENTRIES)))
    return 0;
  for (size_t i = 0; i < mulib_json_size (args); ++i)
    {
      const mulib_json_t *a = mulib_json_at (args, i);
      const char *name = mulib_mcp_wire_label_ (mulib_json_get (a, "name"), 4096);
      if (!name || !mulib_mcp_wire_opt_ (a, "required", MULIB_JSON_BOOL)
          || !mulib_mcp_wire_opt_ (a, "description", MULIB_JSON_STRING)
          || !mulib_mcp_wire_opt_ (a, "title", MULIB_JSON_STRING))
        return 0;
      for (size_t j = 0; j < i; ++j)
        if (mulib_mcp_wire_is_ (mulib_json_get (mulib_json_at (args, j), "name"), name))
          return 0;
    }
  return 1;
}

static inline int
mulib_mcp_wire_string_map_ (const mulib_json_t *map)
{
  if (map && mulib_json_typeof (map) != MULIB_JSON_OBJECT)
    return 0;
  for (size_t i = 0; i < mulib_json_size (map); ++i)
    {
      size_t n;
      const char *key = mulib_json_key (map, i, &n);
      if (!n || n > 4096 || memchr (key, 0, n)
          || !mulib_mcp_wire_string_ (mulib_json_at (map, i)))
        return 0;
    }
  return 1;
}

static inline int
mulib_mcp_wire_complete_args_ (const mulib_json_t *params)
{
  const mulib_json_t *ref = mulib_json_get (params, "ref");
  const mulib_json_t *type = mulib_json_get (ref, "type");
  const mulib_json_t *arg = mulib_json_get (params, "argument");
  const mulib_json_t *ctx = mulib_json_get (params, "context");
  return ((mulib_mcp_wire_is_ (type, "ref/prompt")
           && mulib_mcp_wire_label_ (mulib_json_get (ref, "name"), 4096))
          || (mulib_mcp_wire_is_ (type, "ref/resource")
              && mulib_mcp_wire_template_ (mulib_json_get (ref, "uri"))))
         && mulib_mcp_wire_label_ (mulib_json_get (arg, "name"), 4096)
         && mulib_mcp_wire_string_ (mulib_json_get (arg, "value"))
         && (!ctx || (mulib_json_typeof (ctx) == MULIB_JSON_OBJECT
                      && mulib_mcp_wire_string_map_ (mulib_json_get (ctx, "arguments"))));
}

static inline int
mulib_mcp_wire_cache_ (const mulib_json_t *result)
{
  const mulib_json_t *ttl = mulib_json_get (result, "ttlMs");
  const mulib_json_t *scope = mulib_json_get (result, "cacheScope");
  return mulib_mcp_wire_integer_ (ttl) && mulib_json_numeric (ttl) >= 0
         && (mulib_mcp_wire_is_ (scope, "public") || mulib_mcp_wire_is_ (scope, "private"));
}

static inline int
mulib_mcp_wire_alpha_ (char c)
{
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static inline int
mulib_mcp_wire_alnum_ (char c)
{
  return mulib_mcp_wire_alpha_ (c) || (c >= '0' && c <= '9');
}

static inline int
mulib_mcp_wire_meta_ (const mulib_json_t *meta)
{
  if (!meta)
    return 1;
  if (mulib_json_typeof (meta) != MULIB_JSON_OBJECT)
    return 0;
  for (size_t i = 0; i < mulib_json_size (meta); ++i)
    {
      size_t n;
      const char *key = mulib_json_key (meta, i, &n);
      const char *end = key + n;
      const char *slash = (const char *)memchr (key, '/', n);
      if (slash)
        {
          const char *p = key;
          for (;;)
            {
              if (p == slash || !mulib_mcp_wire_alpha_ (*p++))
                return 0;
              while (p < slash && (mulib_mcp_wire_alnum_ (*p) || *p == '-'))
                ++p;
              if (!mulib_mcp_wire_alnum_ (p[-1]))
                return 0;
              if (p == slash)
                break;
              if (*p++ != '.')
                return 0;
            }
          key = slash + 1;
        }
      if (key == end)
        continue;
      if (!mulib_mcp_wire_alnum_ (key[0]) || !mulib_mcp_wire_alnum_ (end[-1]))
        return 0;
      for (; key != end; ++key)
        if (!mulib_mcp_wire_alnum_ (*key) && *key != '-' && *key != '_' && *key != '.')
          return 0;
    }
  return 1;
}

static inline int
mulib_mcp_wire_request_meta_ (const mulib_json_t *meta)
{
  const mulib_json_t *token = mulib_json_get (meta, "progressToken");
  const mulib_json_t *log = mulib_json_get (meta, MULIB_MCP_WIRE_LOG);
  return mulib_mcp_wire_meta_ (meta)
         && (!token || mulib_mcp_wire_string_ (token)
             || mulib_json_typeof (token) == MULIB_JSON_NUMBER)
         && (!log || mulib_mcp_wire_is_ (log, "debug")
             || mulib_mcp_wire_is_ (log, "info") || mulib_mcp_wire_is_ (log, "notice")
             || mulib_mcp_wire_is_ (log, "warning") || mulib_mcp_wire_is_ (log, "error")
             || mulib_mcp_wire_is_ (log, "critical") || mulib_mcp_wire_is_ (log, "alert")
             || mulib_mcp_wire_is_ (log, "emergency"));
}

static inline int
mulib_mcp_wire_payload_ (const mulib_json_t *result, const char *key, int legacy)
{
  const mulib_json_t *list = mulib_json_get (result, key);
  if (mulib_json_typeof (result) != MULIB_JSON_OBJECT)
    return 0;
  if (!strcmp (key, "completion"))
    {
      const mulib_json_t *total = mulib_json_get (list, "total");
      if (!mulib_mcp_wire_opt_ (list, "hasMore", MULIB_JSON_BOOL))
        return 0;
      list = mulib_json_get (list, "values");
      if (mulib_json_size (list) > 100
          || (total && (!mulib_mcp_wire_integer_ (total)
                        || mulib_json_numeric (total) < (double)mulib_json_size (list))))
        return 0;
    }
  else if (!strcmp (key, "content"))
    {
      const mulib_json_t *structured = mulib_json_get (result, "structuredContent");
      if (!mulib_mcp_wire_opt_ (result, "isError", MULIB_JSON_BOOL)
          || (legacy && structured && mulib_json_typeof (structured) != MULIB_JSON_OBJECT))
        return 0;
    }
  else if (!strcmp (key, "messages")
           && !mulib_mcp_wire_opt_ (result, "description", MULIB_JSON_STRING))
    return 0;
  if (mulib_json_typeof (list) != MULIB_JSON_ARRAY)
    return 0;
  for (size_t i = 0; i < mulib_json_size (list); ++i)
    {
      const mulib_json_t *v = mulib_json_at (list, i);
      if (!strcmp (key, "contents") && !mulib_mcp_wire_contents_ (v))
        return 0;
      if (!strcmp (key, "content") && !mulib_mcp_wire_content_ (v))
        return 0;
      if (!strcmp (key, "completion") && !mulib_mcp_wire_string_ (v))
        return 0;
      if (!strcmp (key, "messages"))
        {
          const mulib_json_t *role = mulib_json_get (v, "role");
          if ((!mulib_mcp_wire_is_ (role, "user")
               && !mulib_mcp_wire_is_ (role, "assistant"))
              || !mulib_mcp_wire_content_ (mulib_json_get (v, "content")))
            return 0;
        }
    }
  return 1;
}

static inline mulib_json_t *
mulib_mcp_wire_info_ (const char *name, const char *version)
{
  mulib_json_t *info = mulib_mcp_wire_object_ ();
  if (info && mulib_mcp_wire_str_ (info, "name", name)
      && mulib_mcp_wire_str_ (info, "version", version))
    return info;
  mulib_json_destroy (info);
  return NULL;
}

/* Conservative compact JSON byte bound (numbers, quoted escaped keys and
   values, punctuation), also bounding recursion and node count. */
static inline int
mulib_mcp_wire_charge_ (size_t *budget, size_t length)
{
  if (length > *budget)
    return 0;
  *budget -= length;
  return 1;
}

static inline int
mulib_mcp_wire_charge_text_ (size_t *budget, size_t n)
{
  return n <= *budget / 6 && mulib_mcp_wire_charge_ (budget, n * 6)
         && mulib_mcp_wire_charge_ (budget, 4);
}

static inline int
mulib_mcp_wire_measure_ (const mulib_json_t *v, size_t *budget, unsigned depth)
{
  mulib_json_type type = mulib_json_typeof (v);
  size_t n;
  if (depth == 32 || !mulib_mcp_wire_charge_ (budget, 4))
    return 0;
  if (type == MULIB_JSON_STRING)
    {
      (void)mulib_json_text (v, &n);
      return mulib_mcp_wire_charge_text_ (budget, n);
    }
  if (type == MULIB_JSON_NUMBER)
    return mulib_mcp_wire_charge_ (budget, 32);
  if (type == MULIB_JSON_ARRAY || type == MULIB_JSON_OBJECT)
    for (size_t i = 0; i < mulib_json_size (v); ++i)
      {
        if (type == MULIB_JSON_OBJECT)
          {
            (void)mulib_json_key (v, i, &n);
            if (!mulib_mcp_wire_charge_text_ (budget, n))
              return 0;
          }
        if (!mulib_mcp_wire_measure_ (mulib_json_at (v, i), budget, depth + 1))
          return 0;
      }
  return 1;
}

/* Only for a measured value or bounded scalar. */
static inline mulib_json_t *
mulib_mcp_wire_clone_ (const mulib_json_t *v)
{
  const char *text;
  size_t n;
  mulib_json_type type = mulib_json_typeof (v);
  mulib_json_t *copy;
  switch (type)
    {
    case MULIB_JSON_STRING:
      text = mulib_json_text (v, &n);
      return mulib_json_string_n (text, n);
    case MULIB_JSON_BOOL:
      return mulib_json_bool (mulib_json_boolean (v));
    case MULIB_JSON_NUMBER:
      return mulib_json_number (mulib_json_numeric (v));
    default:
      break;
    }
  copy = mulib_json_new (type);
  if (copy)
    for (size_t i = 0; i < mulib_json_size (v); ++i)
      {
        mulib_json_t *child = mulib_mcp_wire_clone_ (mulib_json_at (v, i));
        int ok;
        if (type == MULIB_JSON_OBJECT)
          {
            text = mulib_json_key (v, i, &n);
            ok = child && mulib_json_set_n (copy, text, n, child);
          }
        else
          ok = child && mulib_json_append (copy, child);
        if (!ok)
          {
            mulib_json_destroy (child);
            mulib_json_destroy (copy);
            return NULL;
          }
      }
  return copy;
}

static inline mulib_cdd_mcp_status_t
mulib_mcp_wire_frame_ (const char *frame, size_t length)
{
  if (!frame)
    return MULIB_CDD_MCP_INVALID;
  if (length > MULIB_CDD_MCP_MAX_FRAME)
    return MULIB_CDD_MCP_LIMIT;
  if (memchr (frame, '\r', length) || memchr (frame, '\n', length))
    return MULIB_CDD_MCP_INVALID;
  return MULIB_CDD_MCP_OK;
}

/* Borrow the object; publish malloc-owned compact text only on success. */
static inline mulib_cdd_mcp_status_t
mulib_mcp_wire_format_ (const mulib_json_t *value, char **out)
{
  char *frame = mulib_json_stringify (value, 0);
  if (!frame)
    return MULIB_CDD_MCP_NOMEM;
  if (strlen (frame) > MULIB_CDD_MCP_MAX_FRAME)
    {
      free (frame);
      return MULIB_CDD_MCP_LIMIT;
    }
  *out = frame;
  return MULIB_CDD_MCP_OK;
}

static inline int
mulib_mcp_wire_parse_nomem_ (const mulib_json_error_t *error)
{
  return error->message && !strcmp (error->message, "out of memory");
}

/* MuLIB JSON has a general-purpose last-write-wins object builder. For MCP,
   reject repeated keys including escaped aliases. For well-formed JSON each
   colon outside a string corresponds to one object member. A duplicate can
   only shrink the number of DOM members compared with that wire count. */
static inline size_t
mulib_mcp_wire_members_ (const mulib_json_t *v)
{
  mulib_json_type t = mulib_json_typeof (v);
  size_t members = t == MULIB_JSON_OBJECT ? mulib_json_size (v) : 0;
  if (t == MULIB_JSON_OBJECT || t == MULIB_JSON_ARRAY)
    for (size_t i = 0; i < mulib_json_size (v); ++i)
      members += mulib_mcp_wire_members_ (mulib_json_at (v, i));
  return members;
}

static inline mulib_json_t *
mulib_mcp_wire_parse_ (const char *s, size_t n, mulib_json_error_t *error)
{
  mulib_json_t *v = mulib_json_parse (s, n, error);
  size_t members = 0;
  int quoted = 0;
  if (!v)
    return NULL;
  for (size_t i = 0; i < n; ++i)
    {
      if (s[i] == '\\' && quoted)
        ++i;
      else if (s[i] == '"')
        quoted = !quoted;
      else if (!quoted && s[i] == ':')
        ++members;
    }
  if (members == mulib_mcp_wire_members_ (v))
    return v;
  error->message = "duplicate JSON key";
  mulib_json_destroy (v);
  return NULL;
}

#endif
