# OpenCode V2 client for C and Mune

Include [`mune-opencode-client.h`](../kernels/mune-opencode-client.h) or its
MuLIB catalog entry [`mulib-opencode-client.h`](../kernels/mulib-opencode-client.h).
Define `MULIB_IMPL_CDD_MUNE_OPENCODE_CLIENT` in exactly one implementation
translation unit. The declaration header can also be included by C++17.
The client uses MuW3 for transport, JSON for bodies, and SSE for event
framing; the optional-to-use Mune entry point binds a host-configured client
to an existing Mune state. Link the existing MuW3 and Mune implementations
and JSON, SSE, URL and their dependencies. The
[example](../examples/modules/opencode-client.c) gives the full one-TU
recipe via `tests/fixtures/opencode-client-setup.h`.

## Based on the pinned OpenAPI document

[`standards/opencode-v2/openapi.json`](../standards/opencode-v2/openapi.json)
is the source of truth for the compiled operation IDs, verbs, routes,
parameter names, body requirements, success statuses, and streaming formats.
Run `python3 tools/generate_opencode_client.py --check` to check the generated
catalog. The client implements 37 selected operations, not all of the
experimental API. The compiled table is inspectable through
`mulib_cdd_opencode_operation_count`, `mulib_cdd_opencode_operation_at`,
and `mulib_cdd_opencode_operation`.

Supported areas include server info, location, agent/model/provider
discovery, sessions/messages/context, logs and live events, projects,
integration/MCP discovery, references/worktrees/VCS, and usage limits.
There is no arbitrary method/path, shell or PTY execution, permission or
credential management, plugin RPC, service discovery, or process management.

## C requests

Configure an explicit, trusted origin with `mulib_cdd_opencode_client`.
HTTPS is the default; `allow_http=1` explicitly permits cleartext connections.
`authorization` is an optional borrowed complete field value (for example,
`Bearer ...`) and must outlive the call. No automatic authentication, retries,
redirects, cross-origin dispatch, or proxy configuration is performed. HTTPS
uses MuW3's certificate-verifying native transport by default; custom
transport/TLS callbacks must enforce authentication and timeouts.

Populate a zero-initialized `mulib_cdd_opencode_request_t` with an operation
ID and its path/query pairs. Values are UTF-8 and percent-encoded as components;
the client rejects unknown, missing, or duplicate path fields and rejects
empty/dot/slash/backslash/percent path values that could alter routing.
Only catalogued query names are accepted. Use the literal key
`location[directory]` for OpenAPI's `deepObject` location, and repeat `types`
for the list and event routes that declare it. Query order follows input order.
Opaque pagination cursors can be passed as declared query pairs; page
traversal remains under caller control.

The C caller may explicitly enable `allow_session_writes` to create,
update, prompt, or interrupt a session. It submits a single request per
call. JSON request bodies must be objects with the pinned required top-level
fields; `session.prompt` requires string `text`. `permissions` and `parentID`
are not admitted on writes. The client does not implement full nested JSON
Schema validation; the server validates endpoint-specific payload details.

On success (`1`) or HTTP error (`-8`), the caller owns the resulting HTTP
message/headers/body and optional parsed JSON and releases them with
`mulib_cdd_opencode_response_free`. Invalid requests, transport failures,
unexpected success statuses, wrong success media type, and malformed success
JSON do not publish a response. `204` has no JSON. JSON error decoding is
best effort; the status and raw body remain available. Start with a
zero-initialized response and free before reuse.

The client caps URLs at 8 KiB, parameter lists at 64 pairs each, HTTP body
data at 8 MiB and SSE events at 1 MiB by default. Smaller transport limits
are respected. `event.subscribe` and `experimental.session.log` require an
event callback; JSON and SSE metadata are borrowed during it. Return `1` to
continue; any other value stops delivery (`-7`). Stream failure events
produce `-11` and invalid event data
produces `-9`. Already delivered events cannot be rolled back. EOF ends the
stream; the client does not reconnect or automatically replay events.

## Mune binding

`mulib_cdd_opencode_bind(state, &client)` installs this read-only function:

```lua
local body, http_status, code = opencode_request("agent.get",
    {agentID = "example"}, {["location[directory]"] = "/workspace"})
```

Use string-keyed/string-valued tables for path and query parameters (or `nil`).
Mune receives the raw response body as a string, the HTTP status, and a client
result code. Local failures return `nil, nil, code`. Mutation and live SSE
operations are unavailable through this binding even when the C client is
configured for writes. A table cannot express repeated query keys; use C
when submitting multiple values for one declared field. The binding borrows
the trusted client configuration: keep it alive and unchanged until the
state and all closures using it are destroyed. It grants access to
information the selected server and identity can read; it is not a sandbox
or a service authorization mechanism.

## Tests

Run the OpenCode tests with `ctest --test-dir <build> -R opencode`. They
check catalog drift, mocked HTTP transactions (including non-followed
redirects, response bounds and SSE cancellation), real native loopback HTTP
and chunked SSE, and the Mune capability boundary. They do not connect to a
live OpenCode service or use any service credentials.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-opencode-client.h`](../kernels/mulib-opencode-client.h) |
| Implementation switch | `MULIB_IMPL_CDD_MUNE_OPENCODE_CLIENT` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-json](mulib-json.md), [mulib-mune](mulib-mune.md), [mulib-muw3](mulib-muw3.md), [mulib-sse](mulib-sse.md), [mulib-uri](mulib-uri.md), [mulib-url](mulib-url.md), [mulib-utf8](mulib-utf8.md), [`kernels/mune-opencode-client.h`](../kernels/mune-opencode-client.h) |
| Complete example | [`examples/modules/opencode-client.c`](../examples/modules/opencode-client.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/opencode-client.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
