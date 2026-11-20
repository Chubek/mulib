/**
 * @file mulib-mcp-broadcast.h
 * @brief Authenticated, socket-free public endpoint adapter for MCP JSON-RPC.
 * Define MULIB_IMPL_MCP_BROADCAST once, alongside the MCP HTTP dependencies.
 */
#ifndef MULIB_MCP_BROADCAST_H
#define MULIB_MCP_BROADCAST_H
#include "mulib-mcp-http.h"
#ifdef __cplusplus
extern "C"
{
#endif
  #define MULIB_MCP_BROADCAST_MAX_URL 2048u
  #define MULIB_MCP_BROADCAST_MAX_ISSUERS 8u
  #define MULIB_MCP_BROADCAST_MAX_TOKEN 8192u

  /** Check a parsed RFC 6750 Bearer credential for this exact resource URL.
   * The token is borrowed only during this call; never log it. For OAuth,
   * verify issuer, token validity (including audience and expiry), and
   * applicable scopes. Return NULL to deny or a host-owned principal that
   * remains valid until the enclosing HTTP call returns. */
  typedef const void *(*mulib_mcp_broadcast_verify_fn) (
      void *user, const char *resource_url, const char *token, size_t token_length);

  typedef struct mulib_mcp_broadcast_config
  {
    const char *resource_url;
    /* Trusted HTTPS issuer identifiers, not authorization endpoint URLs.
     * Count 0 disables RFC 9728 metadata (the challenge is just Bearer). */
    const char *const *authorization_servers;
    size_t authorization_server_count;
    mulib_mcp_broadcast_verify_fn verify; /* required */
    mulib_cdd_mcp_origin_fn origin; /* NULL denies requests carrying Origin */
    void *user;
    int sse; /* use the binding's single final SSE event on success */
  } mulib_mcp_broadcast_config_t;

  /** A caller-owned TLS frontend must select the public HTTPS authority
   * and exact origin-form target, then supply them as trusted route context.
   * Never derive this context from untrusted headers (including forwarded
   * headers). Preserve the original public Host (or validated :authority
   * in an HTTP/1.1 translation) and raw target in the complete bounded request.
   * There is no TLS, listener, proxy trust, or network discovery here.
   *
   * The configured URL must be an absolute lowercase-scheme HTTPS URL with
   * explicit path, DNS/IPv4 or bracketed IPv6 host, optional nonzero decimal
   * port without leading zeroes, and no userinfo, query or fragment. Use
   * literal unreserved path bytes and slash separators; empty, dot and
   * dot-dot segments are rejected (the root path "/" is supported).
   * Host compares ASCII case-insensitively,
   * with no implicit port; the path compares byte-for-byte. Reachability and
   * certificate checks are deployment concerns. Issuer URLs may omit the path.
   *
   * An initialized catalogue with a nonnull authorize callback is mandatory.
   * It receives the verifier's principal, never a client-supplied identity.
   * Authorize methods and targets, including list/inventory exposure; don't
   * assume that all authenticated principals may invoke every tool. Tool
   * callbacks still validate argument semantics before performing effects.
   * Authentication is required for all MCP dispatch. With configured issuers
   * the metadata GET route is public, subject to the same Host/Origin checks;
   * publish "/.well-known/oauth-protected-resource" followed by the resource
   * path (omit the suffix for "/"). This namespace is reserved when metadata
   * is enabled. This adapter does not implement OAuth issuance, signature
   * verification, CORS, legacy MCP or connector guarantees.
   *
   * OK returns a malloc-owned HTTP response, including status rejections.
   * Invalid config, unrecognized trusted route context, or framing failure
   * returns INVALID/LIMIT (or NOMEM) without changing outputs. A callback
   * may already have run if response encoding fails; don't blindly retry.
   * Borrow config and catalogue for the call. Observe the HTTP binding's
   * no-concurrent-dispatch constraint for a single catalogue. */
  mulib_cdd_mcp_status_t mulib_mcp_broadcast_http (
      const mulib_cdd_mcp_catalog_t *catalog, const mulib_mcp_broadcast_config_t *config,
      const char *https_authority, const char *target, const void *request,
      size_t request_length, unsigned char **out, size_t *length);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_MCP_BROADCAST) && !defined(MULIB_MCP_BROADCAST_IMPLEMENTED)
#define MULIB_MCP_BROADCAST_IMPLEMENTED
#include "mulib-utf8.h"
#include "detail/mulib-mcp-wire.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C"
{
#endif
  #define MULIB_MCP_BROADCAST_METADATA_ "/.well-known/oauth-protected-resource"
  #define MULIB_MCP_BROADCAST_CHALLENGE_ "Bearer resource_metadata=\""

  static int
  mulib_mcp_broadcast_alnum_ (char c)
  {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
           || (c >= '0' && c <= '9');
  }

  static int
  mulib_mcp_broadcast_ieq_ (const char *a, const char *b)
  {
    if (!a || !b)
      return 0;
    while (*a && *b)
      {
        char x = *a++, y = *b++;
        if (x >= 'A' && x <= 'Z') x = (char)(x + 'a' - 'A');
        if (y >= 'A' && y <= 'Z') y = (char)(y + 'a' - 'A');
        if (x != y)
          return 0;
      }
    return *a == *b;
  }

  static int
  mulib_mcp_broadcast_authority_ (const mulib_uri_t *uri)
  {
    const char *s = uri->host.data;
    size_t n = uri->host.length;
    if (!uri->has_authority || uri->has_userinfo || !n || n > 253)
      return 0;
    if (uri->has_port)
      {
        unsigned long port = 0;
        if (!uri->port.length || uri->port.length > 5 || uri->port.data[0] == '0')
          return 0;
        for (size_t i = 0; i < uri->port.length; ++i)
          port = port * 10 + (unsigned)(uri->port.data[i] - '0');
        if (port > 65535)
          return 0;
      }
    /* URI parsing has already checked literal syntax. IPvFuture is not
       supported by this endpoint. */
    if (s[0] == '[')
      return s[1] != 'v' && s[1] != 'V';
    for (size_t start = 0, i = 0; i <= n; ++i)
      {
        if (i != n && s[i] != '.')
          {
            if (!mulib_mcp_broadcast_alnum_ (s[i]) && s[i] != '-')
              return 0;
            continue;
          }
        if (i == start || i - start > 63 || s[start] == '-' || s[i - 1] == '-')
          return 0;
        if (i == n)
          {
            /* In browsers a numeric last label can turn the whole authority
               into IPv4. Only permit canonical dotted decimal in that case. */
            size_t j = start;
            int hex = i - start >= 2 && s[start] == '0'
                      && (s[start + 1] == 'x' || s[start + 1] == 'X');
            if (hex) j += 2;
            while (j < i && ((s[j] >= '0' && s[j] <= '9')
                              || (hex && ((s[j] >= 'a' && s[j] <= 'f')
                                          || (s[j] >= 'A' && s[j] <= 'F')))))
              ++j;
            if (j == i)
              {
                mulib_inet4_t address;
                return mulib_inet4_parse (s, n, &address);
              }
          }
        start = i + 1;
      }
    return 1;
  }

  static int
  mulib_mcp_broadcast_url_ (const char *url, mulib_uri_t *uri)
  {
    const size_t limit = MULIB_MCP_BROADCAST_MAX_URL;
    return mulib_mcp_wire_cstr_ (url, limit) && !strncmp (url, "https://", 8)
           && mulib_uri_parse (url, strlen (url), uri)
           && mulib_mcp_broadcast_authority_ (uri)
           && !uri->has_query && !uri->has_fragment;
  }

  static int
  mulib_mcp_broadcast_path_ (const char *path, size_t n)
  {
    if (!n || path[0] != '/')
      return 0;
    if (n == 1)
      return 1;
    for (size_t start = 1, i = 1; i <= n; ++i)
      {
        if (i == n || path[i] == '/')
          {
            size_t z = i - start;
            if (!z || (z == 1 && path[start] == '.')
                || (z == 2 && path[start] == '.' && path[start + 1] == '.'))
              return 0;
            start = i + 1;
          }
        else if (!mulib_mcp_broadcast_alnum_ (path[i]) && !strchr ("-._~", path[i]))
          return 0;
      }
    return 1;
  }

  static int
  mulib_mcp_broadcast_origin_ (void *user, const char *origin)
  {
    const mulib_mcp_broadcast_config_t *config = (const mulib_mcp_broadcast_config_t *)user;
    mulib_uri_t uri;
    size_t n = strlen (origin);
    return config->origin && n <= 1024
           && (!strncmp (origin, "https://", 8) || !strncmp (origin, "http://", 7))
           && mulib_uri_parse (origin, n, &uri) && mulib_mcp_broadcast_authority_ (&uri)
           && !uri.path.length && !uri.has_query && !uri.has_fragment
           && config->origin (config->user, origin);
  }

  static unsigned
  mulib_mcp_broadcast_authenticate_ (void *user, const mulib_http_message_t *message,
                                     const void **principal)
  {
    const mulib_mcp_broadcast_config_t *config = (const mulib_mcp_broadcast_config_t *)user;
    const char *value = mulib_http_header (message, "Authorization");
    char scheme[7];
    size_t n = 0;
    /* The binding has rejected duplicates. Only RFC 6750 header credentials
       are accepted, not query parameters, form contents or proxy headers. */
    if (!value || strlen (value) < sizeof scheme || value[6] != ' ')
      return 401;
    memcpy (scheme, value, 6);
    scheme[6] = 0;
    if (!mulib_mcp_broadcast_ieq_ (scheme, "Bearer"))
      return 401;
    value += 7;
    while (*value == ' ') ++value;
    while (value[n] && (mulib_mcp_broadcast_alnum_ (value[n]) || strchr ("-._~+/", value[n])))
      ++n;
    if (!n)
      return 401;
    while (value[n] == '=') ++n;
    if (value[n] || n > MULIB_MCP_BROADCAST_MAX_TOKEN)
      return 401;
    *principal = config->verify (config->user, config->resource_url, value, n);
    return *principal ? 200 : 401;
  }

  static char *
  mulib_mcp_broadcast_document_ (const mulib_mcp_broadcast_config_t *config)
  {
    mulib_json_t *doc = mulib_json_new (MULIB_JSON_OBJECT);
    mulib_json_t *methods = mulib_json_new (MULIB_JSON_ARRAY);
    mulib_json_t *issuers = mulib_json_new (MULIB_JSON_ARRAY);
    char *json = NULL;
    if (!doc || !methods || !issuers
        || !mulib_mcp_wire_str_ (doc, "resource", config->resource_url)
        || !mulib_mcp_wire_append_ (methods, mulib_json_string ("header"))
        || !mulib_json_set (doc, "bearer_methods_supported", methods))
      goto done;
    methods = NULL;
    for (size_t i = 0; i < config->authorization_server_count; ++i)
      if (!mulib_mcp_wire_append_ (issuers, mulib_json_string (config->authorization_servers[i])))
        goto done;
    if (mulib_json_set (doc, "authorization_servers", issuers))
      {
        issuers = NULL;
        json = mulib_json_stringify (doc, 0);
      }
  done:
    mulib_json_destroy (issuers);
    mulib_json_destroy (methods);
    mulib_json_destroy (doc);
    return json;
  }

  static mulib_cdd_mcp_status_t
  mulib_mcp_broadcast_status_ (mulib_http_result status)
  {
    switch (status)
      {
      case MULIB_HTTP_OK: return MULIB_CDD_MCP_OK;
      case MULIB_HTTP_LIMIT: return MULIB_CDD_MCP_LIMIT;
      case MULIB_HTTP_NOMEM: return MULIB_CDD_MCP_NOMEM;
      default: return MULIB_CDD_MCP_INVALID;
      }
  }

  static mulib_cdd_mcp_status_t
  mulib_mcp_broadcast_metadata_ (const mulib_mcp_broadcast_config_t *config,
                                 const char *authority, const char *target,
                                 const void *request, size_t n,
                                 unsigned char **out, size_t *length)
  {
    const mulib_http_options_t options = { 0, 0, MULIB_CDD_MCP_MAX_FRAME, MULIB_HTTP_EOF };
    mulib_http_message_t message, response;
    mulib_http_header_t headers[2];
    const char *origin;
    char *json = NULL;
    size_t used, cursor = 0;
    mulib_cdd_mcp_status_t status = mulib_mcp_broadcast_status_ (
        mulib_http_parse (request, n, 0, &options, &message, &used));
    if (status != MULIB_CDD_MCP_OK)
      return status;
    if (used != n)
      {
        mulib_http_message_free (&message);
        return MULIB_CDD_MCP_INVALID;
      }
    memset (&response, 0, sizeof response);
    response.is_response = 1;
    response.headers = headers;
    response.header_count = 1;
    headers[0].name = (char *)"Cache-Control";
    headers[0].value = (char *)"no-store";
    origin = mulib_http_header_next (&message, "Origin", &cursor);
    /* The HTTP request parser already requires exactly one Host field. */
    if (!mulib_mcp_broadcast_ieq_ (mulib_http_header (&message, "Host"), authority))
      response.status = 400, response.reason = (char *)"Bad Request";
    else if (origin && (mulib_http_header_next (&message, "Origin", &cursor)
                        || !mulib_mcp_broadcast_origin_ ((void *)config, origin)))
      response.status = 403, response.reason = (char *)"Forbidden";
    else if (strcmp (message.target, target))
      response.status = 404, response.reason = (char *)"Not Found";
    else if (strcmp (message.method, "GET"))
      {
        response.status = 405, response.reason = (char *)"Method Not Allowed";
        headers[1].name = (char *)"Allow";
        headers[1].value = (char *)"GET";
        ++response.header_count;
      }
    else if (message.body_length || message.chunked)
      response.status = 400, response.reason = (char *)"Bad Request";
    else
      {
        json = mulib_mcp_broadcast_document_ (config);
        if (!json)
          {
            mulib_http_message_free (&message);
            return MULIB_CDD_MCP_NOMEM;
          }
        response.status = 200, response.reason = (char *)"OK";
        response.body = (unsigned char *)json;
        response.body_length = strlen (json);
        headers[1].name = (char *)"Content-Type";
        headers[1].value = (char *)"application/json";
        ++response.header_count;
      }
    mulib_http_message_free (&message);
    status = mulib_mcp_broadcast_status_ (mulib_http_serialize (&response, NULL, out, length));
    free (json);
    return status;
  }

  mulib_cdd_mcp_status_t
  mulib_mcp_broadcast_http (const mulib_cdd_mcp_catalog_t *catalog,
                           const mulib_mcp_broadcast_config_t *config,
                           const char *https_authority, const char *target,
                           const void *request, size_t request_length,
                           unsigned char **out, size_t *length)
  {
    mulib_uri_t uri;
    mulib_cdd_mcp_http_config_t http;
    char authority[253 + 1 + 5 + 1];
    char metadata[sizeof MULIB_MCP_BROADCAST_METADATA_ + MULIB_MCP_BROADCAST_MAX_URL];
    char challenge[sizeof metadata + sizeof authority + sizeof MULIB_MCP_BROADCAST_CHALLENGE_ + 9];
    size_t na;
    if (!catalog || !catalog->server.config.name || !catalog->features.authorize
        || !config || !config->verify || !target || !out || !length
        || (config->authorization_server_count && !config->authorization_servers)
        || config->authorization_server_count > MULIB_MCP_BROADCAST_MAX_ISSUERS
        || !mulib_mcp_broadcast_url_ (config->resource_url, &uri)
        || !mulib_mcp_broadcast_path_ (uri.path.data, uri.path.length))
      return MULIB_CDD_MCP_INVALID;
    na = uri.host.length + (uri.has_port ? uri.port.length + 1 : 0);
    memcpy (authority, uri.host.data, na);
    authority[na] = 0;
    if (!mulib_mcp_broadcast_ieq_ (https_authority, authority))
      return MULIB_CDD_MCP_INVALID;
    http.authority = authority;
    http.path = uri.path.data;
    http.challenge = "Bearer";
    http.origin = mulib_mcp_broadcast_origin_;
    http.authenticate = mulib_mcp_broadcast_authenticate_;
    http.user = (void *)config;
    http.sse = config->sse;
    if (config->authorization_server_count)
      {
        for (size_t i = 0; i < config->authorization_server_count; ++i)
          {
            mulib_uri_t issuer;
            if (!mulib_mcp_broadcast_url_ (config->authorization_servers[i], &issuer))
              return MULIB_CDD_MCP_INVALID;
            for (size_t j = 0; j < i; ++j)
              if (!strcmp (config->authorization_servers[i], config->authorization_servers[j]))
                return MULIB_CDD_MCP_INVALID;
          }
        (void)snprintf (metadata, sizeof metadata, "%s%s", MULIB_MCP_BROADCAST_METADATA_,
                        uri.path.length == 1 ? "" : http.path);
        /* Avoid a resource serving its own metadata on the protected route.
           Reserve the discovery namespace when metadata is enabled. */
        if (!strcmp (http.path, MULIB_MCP_BROADCAST_METADATA_)
            || !strncmp (http.path, MULIB_MCP_BROADCAST_METADATA_ "/",
                         sizeof MULIB_MCP_BROADCAST_METADATA_))
          return MULIB_CDD_MCP_INVALID;
        if (!strcmp (target, metadata))
          return mulib_mcp_broadcast_metadata_ (config, authority, metadata,
                                                request, request_length, out, length);
        (void)snprintf (challenge, sizeof challenge, MULIB_MCP_BROADCAST_CHALLENGE_
                        "https://%s%s\"", authority, metadata);
        http.challenge = challenge;
      }
    if (strcmp (target, http.path))
      return MULIB_CDD_MCP_INVALID;
    return mulib_cdd_mcp_server_http (catalog, &http, request, request_length, out, length);
  }
  #undef MULIB_MCP_BROADCAST_METADATA_
  #undef MULIB_MCP_BROADCAST_CHALLENGE_
#ifdef __cplusplus
}
#endif
#endif /* implementation */
