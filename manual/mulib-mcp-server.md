# `mulib-mcp-server` — bounded Model Context Protocol server

The [official versioning page](https://modelcontextprotocol.io/specification/versioning)
identifies **2026-07-28** as Current. This kernel targets a tested subset of
that [revision and schema](https://modelcontextprotocol.io/specification/2026-07-28),
not the [draft](https://modelcontextprotocol.io/specification/draft).

| Revision | Implemented operations |
| --- | --- |
| `2026-07-28` | `server/discover`, `tools/list`, `tools/call`, `resources/list`, `resources/templates/list`, `resources/read`, `prompts/list`, `prompts/get`, `completion/complete`. Requests provide version and client capabilities in `params._meta`; client software identity is optional. No initialization or ping. |
| `2025-11-25` (opt-in stdio) | `initialize`, `notifications/initialized`, client-to-server `ping`, and the above feature methods (without modern envelopes, caching, or multi-round-trip state). Earlier offered versions get `2025-11-25` in the handshake. |

## The host supplies all handlers

- The base `mulib_cdd_mcp_server_init` / `mulib_cdd_mcp_server_process` API
  remains a simple text-tool registry. Its text callbacks get a bounded
  8 KiB UTF-8 output buffer; `TOOL_ERROR` becomes a successful wire response
  with `isError: true`. The base tool list fits in one page.
- Use `mulib_cdd_mcp_catalog_init` and `mulib_cdd_mcp_catalog_process` for a
  catalogue of resources, template descriptions, prompts, and structured
  tools. The configuration structs are copied; strings, JSON descriptors,
  schema, arrays, and callback contexts stay borrowed and immutable.
  Allocate one instance per legacy channel. Do not reenter or use concurrently.
- Read handlers get bounded absolute ASCII URIs, never paths resolved by the
  library. A handler decides which URIs (including expanded templates) are
  readable, and can return multiple text or Base64 blob contents. Unknown URIs
  should return `HANDLER_INVALID` (`-32602`). Template descriptions use RFC
  6570 expression syntax with an absolute ASCII URI layout; the host implements
  expansion, matching, variable policy, and URI authorization.
- Prompt calls dispatch only to an exact listed prompt, with declared required
  arguments present and all argument values strings. Completion validates
  the reference, partial value and optional string-map context, checks known
  prompt arguments, and returns up to 100 validated suggestions. Keep sensitive
  suggestions out of unauthorized responses; rate-limit in the host.
- Structured tools require a host-provided schema validator called before
  invocation and, when an output schema is declared, on structured output.
  Input schema roots must have `type: "object"` and legacy-compatible output
  schema roots must describe objects. Supply actual schema evaluation in the
  host. No schema or URI network resolution is performed. The kernel checks
  tool and prompt content blocks (`text`, `image`, `audio`, `resource`,
  `resource_link`) but neither renders them nor executes content.

Callbacks borrow the request only for the call and transfer a newly owned result
DOM. Return `HANDLER_FAILED` for internal errors; invalid or oversize callback
output becomes an internal error. Use `HANDLER_DEFER` with a JSON state string to
end a current `tools/call`, `resources/read`, or `prompts/get` round. The host
must verify the returning state before using it, binding any integrity-protected
state to the authenticated principal, original operation and arguments, and
expiry. If single use matters, enforce replay protection at the host. This
facility does not send or handle `inputRequests`/`inputResponses`.

The optional authorization callback sees the host-supplied authenticated
principal and untrusted request fields. A denial prevents invocation.
An allow decision for a list exposes the entire configured inventory; use
separate catalogues for different visibility policies. Validate arguments
before effects, enforce access control for URI/template resolution, use
explicit host-approved implementations, and isolate callbacks as appropriate.
Neither peer's self-reported identity authenticates it.

## Framing, caching, and limits

On a successful frame call, `free` the compact JSON reply or ignore a NULL
reply for a notification. JSON-RPC errors are reply objects, distinct from
local API statuses. A local failure leaves the output slot unchanged, but a
handler might already have run; do not automatically replay operations with
side effects. The [stdio example](../examples/modules/mcp-server.c) supplies
LF framing. See [mcp-http](mulib-mcp-http.md) for the separate current HTTP binding.

Current discovery and completed list/read replies contain a private TTL
hint (defaults to zero); `resources/read` retry results get TTL zero and
interim `input_required` replies have no cache hints. Catalogue lists return
opaque paginated cursors tied to method, era and the configured catalogue
generation. Tokens are positions, not credentials: recheck authorization
on every page and set a new generation when replacing a catalogue or policy.
Limits include 64 KiB frames, 1,024-byte string IDs or safe integer IDs, no
batches or duplicate JSON keys, 64 tools and 256 descriptors per other
catalogue category. Output depth/size is bounded independently of input.

No subscription streams, list-change notifications, progress emission,
server-side interruption of an active callback, optional extensions, or
legacy reverse RPC. This is not a claim of full MCP conformance.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-mcp-server.h`](../include/mulib-mcp-server.h) |
| Implementation switch | `MULIB_IMPL_CDD_MCP_SERVER` |
| Repository-local includes | [`include/detail/mulib-mcp-common.h`](../include/detail/mulib-mcp-common.h), [`include/detail/mulib-mcp-wire.h`](../include/detail/mulib-mcp-wire.h), [mulib-json](mulib-json.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/mcp-server.c`](../examples/modules/mcp-server.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mcp-server.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
