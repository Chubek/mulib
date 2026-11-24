# `mulib-mcp-client` — bounded Model Context Protocol client

The [official versioning page](https://modelcontextprotocol.io/specification/versioning)
identifies **2026-07-28** as Current. This kernel implements a subset of that
[revision and schema](https://modelcontextprotocol.io/specification/2026-07-28),
not the [draft](https://modelcontextprotocol.io/specification/draft).
The initialization handshake and `ping` are only for legacy `2025-11-25` stdio.

Use one caller-owned client state per peer/channel. Initialize it with
`mulib_mcp_client_init` (or the compatible `mulib_cdd_mcp_client_init`),
borrowing immutable UTF-8 name/version strings. The `mulib_mcp_client_*`
API uses the same bounded framing and single-in-flight state machine.
Define `MULIB_IMPL_MCP_CLIENT` or its legacy equivalent
`MULIB_IMPL_CDD_MCP_CLIENT` in the implementation translation unit.
Then choose one opening:

- For dual-era stdio, build `MULIB_CDD_MCP_DISCOVER`. Discovery records the
  capabilities of the peer. If it returns `LEGACY_PEER`, or the host times
  out and abandons the probe, host policy may choose `INITIALIZE_LEGACY`
  followed by `INITIALIZED_LEGACY`. Recognized current errors never trigger
  downgrade. `NO_VERSION` means no compatible version for the selected era.
- For a known legacy peer, start with `INITIALIZE_LEGACY`.
- For a known current peer, `mulib_cdd_mcp_client_request_ex` with `direct = 1`
  skips discovery and capability gating for feature RPCs. It does not select
  a legacy peer or automate fallback.

## Request and response shapes

| Operation | `target` | `object` |
| --- | --- | --- |
| `TOOLS_LIST`, `RESOURCES_LIST`, `RESOURCE_TEMPLATES_LIST`, `PROMPTS_LIST` | `NULL` or the exact returned cursor | `NULL` |
| `TOOLS_CALL` | Host-approved tool name | Optional arguments object |
| `RESOURCES_READ` | Absolute ASCII URI | `NULL` |
| `PROMPTS_GET` | Prompt name | Optional string map |
| `COMPLETE` | `NULL` | MCP `ref`, `argument`, optional `context` object |
| Discovery and legacy lifecycle/ping | `NULL` | `NULL` |

Current requests carry the version, software identity, and an empty client
capabilities object in `params._meta`. The extended builder can add validated
progress tokens and vendor metadata, but cannot override those three fields.
Server identity is advisory, never proof of authentication. Returned resources,
schemas, descriptions, images, and other content remain untrusted even after
wire shape validation. A tool result with `isError: true` is not a JSON-RPC
error. Current `structuredContent` can be any JSON value; legacy allows an
object only. The host decides whether to call a tool, validates its schema and
semantics, and handles consent. There is no automatic invocation, resource
resolution, or content rendering.

The host owns each successfully built frame (`free`). For stdio, append LF and
remove the incoming delimiter before `mulib_cdd_mcp_client_receive`. For the
current HTTP binding see [mcp-http](mulib-mcp-http.md). The client correlates one
request at a time using monotonically increasing string IDs. Do not reinitialize
while old replies can still arrive. A completed request gives an owned response
DOM even for `REMOTE_ERROR`, `LEGACY_PEER`, `NO_VERSION`, `INPUT_REQUIRED`, or
`UNSUPPORTED`; release it with `mulib_json_destroy`. A malformed correlated
response leaves the pending request intact.

## Cache, round trips, and cancellation

- Modern discovery and cacheable list/read results carry validated `ttlMs` and
  `cacheScope` hints. The client stores no results. Hosts that cache must key by
  peer, method, relevant parameters (including cursor), and authorization context
  for private entries; never cache a round-trip retry.
- A state-only `input_required` produces `INPUT_REQUIRED`. If safe, the host may
  explicitly build the same request with the exact `requestState` JSON value
  in `mulib_cdd_mcp_request_options_t`. It gets a new ID. No automatic retry or
  replay, and no support for fulfilling `inputRequests` (reported `UNSUPPORTED`).
- `mulib_cdd_mcp_client_abandon` stops waiting locally. For stdio,
  `mulib_cdd_mcp_client_cancel` builds `notifications/cancelled` and abandons
  local correlation; the host must send the frame. This does not prove that a
  remote callback stopped or that it never had effects. For current HTTP do not
  send this notification; the host handles disconnects.
- No subscription/list-change handling, progress callbacks, extensions,
  concurrency, or legacy reverse RPC. Unsolicited notifications are ignored.
  Frame limit: 64 KiB. CR/LF delimiters, batches and duplicate JSON keys are
  rejected. The URI template subset checks expressions but does not expand them.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-mcp-client.h`](../kernels/mulib-mcp-client.h) |
| Implementation switch | `MULIB_IMPL_CDD_MCP_CLIENT` |
| Repository-local includes | [`kernels/detail/mulib-mcp-common.h`](../kernels/detail/mulib-mcp-common.h), [`kernels/detail/mulib-mcp-wire.h`](../kernels/detail/mulib-mcp-wire.h), [mulib-json](mulib-json.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/mcp-client.c`](../examples/modules/mcp-client.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/mcp-client.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
