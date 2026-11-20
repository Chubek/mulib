# mcp-broadcast — Publish an authenticated remote MCP catalogue

The broadcast adapter serves MCP **JSON-RPC 2.0** requests through the
existing [Streamable HTTP binding](mulib-mcp-http.md), using the envelope
reader shared with the [JSON-RPC kernel](mulib-jsonrpc.md). Broadcasting here
means publishing an endpoint clients can call, not sending unsolicited
messages. The adapter processes one complete, bounded HTTP request at a time.
The host owns the HTTP listener, TLS, connection timeouts, and response
delivery. Use HTTPS at the public address and an authenticated local link
or loopback behind a reverse proxy.

1. Initialize a `mulib_mcp_catalog_t` with an authorization callback that
   checks the authenticated principal, operation and target. Authorize
   discovery and listing explicitly; do not expose tools or data via lists
   that the caller is not permitted to see.
2. Set `resource_url` to the exact public HTTPS endpoint and provide a
   `verify` callback for Bearer credentials. For OAuth, verify token
   audience, issuer, expiry and scopes with your authorization service.
   Return a host-owned principal, or `NULL` to deny. Avoid logging tokens.
3. Pass the trusted authority and original request target selected by the
   TLS frontend, and the complete HTTP/1.1 request, to
   `mulib_mcp_broadcast_http`. Preserve the public `Host` and exact path;
   never derive trusted route context from untrusted forwarded headers.
   Browser `Origin` is denied by default; if enabled, use an explicit
   allowlist and configure CORS separately.
4. On `MULIB_CDD_MCP_OK`, send the returned byte count and free the response.
   The response may be an HTTP rejection. Do not blindly retry after a
   failure: the tool may have completed before response encoding failed.
   Apply request deadlines and rate limits in the host.

Optionally configure trusted OAuth issuer URLs as `authorization_servers`.
For `https://tools.example/mcp`, the adapter then serves RFC 9728 metadata at
`https://tools.example/.well-known/oauth-protected-resource/mcp` and advertises
that URL in `WWW-Authenticate`. Without issuers it sends a plain Bearer
challenge. It does not issue or validate OAuth tokens itself.

An HTTPS connector can use this endpoint if it supports the implemented MCP
and authentication profiles. GPT or Claude configuration and version
negotiation are deployment concerns, not compatibility claims from this
kernel. The standard wire binding does not implement legacy Streamable HTTP
initialization or deprecated HTTP+SSE sessions.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-mcp-broadcast.h`](../include/mulib-mcp-broadcast.h) |
| Implementation switch | `MULIB_IMPL_MCP_BROADCAST` |
| Repository-local includes | [`include/detail/mulib-mcp-wire.h`](../include/detail/mulib-mcp-wire.h), [mulib-mcp-http](mulib-mcp-http.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/mcp-broadcast.c`](../examples/modules/mcp-broadcast.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mcp-broadcast.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
