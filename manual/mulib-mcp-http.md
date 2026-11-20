# `mulib-mcp-http` — bounded Streamable HTTP binding

This module connects the [MCP client](mulib-mcp-client.md) and
[server](mulib-mcp-server.md) to the HTTP/1.1 and SSE kernels for the
[current `2026-07-28` transport](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/streamable-http).
It takes and returns whole framed messages. It does not own a socket, spawn
a process, select a URL, handle TLS, or follow redirects.

1. Build a current JSON-RPC request using the client. Pass the frame, a trusted
   authority and endpoint path, and optionally an authorization value to
   `mulib_cdd_mcp_client_http_request`. For a tool call, pass the approved
   tool's advertised input schema for `x-mcp-header` mirroring. Send the
   returned HTTP bytes and `free` them.
2. On the server, pass exactly one HTTP request to `mulib_cdd_mcp_server_http`.
   It checks the exact configured path and case-insensitive Host, rejects
   disallowed or malformed `Origin` values, requires a host authentication
   callback, and checks required media and mirrored headers. Duplicated
   singleton and recognized parameter headers, mismatches, malformed
   Base64 sentinel values, and duplicate JSON keys are rejected. The
   catalogue's authorization callback receives the authenticated principal
   separately from the untrusted JSON.
3. The server returns one malloc-owned HTTP response, optionally carrying
   the final JSON-RPC response as an SSE event. The client accepts JSON and
   bounded SSE response bodies, correlates the final ID, and rejects multiple
   final results or reverse RPCs. It ignores request-related notifications.
   No state is committed for a malformed HTTP/SSE response. A transport-only
   error (for example, a bodyless `401`) is for the caller to interpret.

The HTTP binding mirrors `MCP-Protocol-Version`, `Mcp-Method`, and `Mcp-Name`
as required. It supports primitive `x-mcp-header` annotations at statically
reachable `properties` paths (up to 32 annotations and 64 bytes per annotation
name). It excludes malformed annotations when consuming `tools/list` pages.
The kernel checks header/body agreement, not JSON Schema semantics. Header
encoding never resolves `$ref` or fetches schemas. An HTTP version mismatch
or unsupported method follows the current 400/404 JSON-RPC error rules.
HTTP responses carry `Cache-Control: no-store`; protocol-level TTL hints are
separate and private.

## Host responsibilities

For an authenticated public endpoint with optional OAuth resource metadata,
use the [MCP broadcast adapter](mulib-mcp-broadcast.md).

- Configure a fixed Host and path; bind local-only services to loopback.
  Omit the Origin policy to deny all requests with an Origin. If you allow a
  browser origin, use an explicit allowlist and implement any required CORS
  policy outside this binding. Origin and Host are not authentication.
- Require actual authentication for a network service. The authentication
  callback returns 200 with a host principal, 401 with a configured
  `WWW-Authenticate` challenge, or 403. Do not trust mirrored routing values,
  forwarded headers, or client software identity as credentials. For deployed
  services supply TLS, credential verification, OAuth/discovery when needed,
  connection/read deadlines, limits, and rate limiting in the host.
- Use a complete bounded HTTP message, with no pipelined bytes. The server
  caps the body at 64 KiB; the client caps the accumulated SSE/HTTP body at
  256 KiB and each JSON-RPC frame at 64 KiB. The HTTP codec applies separate
  header/framing limits. This binding accepts identity encoding and the
  expected media type with an optional UTF-8 charset.

Only the current POST request path is supported. Legacy Streamable HTTP,
deprecated HTTP+SSE, sessions, browser preflight, SSE resumption, incremental
response delivery, live subscriptions, and server-side cancellation are not
implemented. Closing the external connection stops the host from receiving a
response but cannot interrupt this synchronous library call. No full MCP
transport or authorization conformance is claimed.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-mcp-http.h`](../include/mulib-mcp-http.h) |
| Implementation switch | `MULIB_IMPL_CDD_MCP_HTTP` |
| Repository-local includes | [`include/detail/mulib-jsonrpc-envelope.h`](../include/detail/mulib-jsonrpc-envelope.h), [`include/detail/mulib-mcp-wire.h`](../include/detail/mulib-mcp-wire.h), [mulib-http](mulib-http.md), [mulib-mcp-client](mulib-mcp-client.md), [mulib-mcp-server](mulib-mcp-server.md), [mulib-sse](mulib-sse.md) |
| Complete example | [`examples/modules/mcp-http.c`](../examples/modules/mcp-http.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mcp-http.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
