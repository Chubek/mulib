# OpenAI client over MuW3

**Header:** `include/mulib-openai-client.h` · **Switch:** `MULIB_IMPL_OPENAI_CLIENT`

The synchronous client composes [MuW3](mulib-muw3.md), [OpenAI utilities](mulib-openai.md),
[SSE](mulib-sse.md) and [multipart](mulib-multipart.md). Enable these modules and
their dependency closures in one C11 implementation translation unit; dependency
switches are never enabled implicitly. The complete standalone
[example](../examples/modules/openai-client.c) includes the TLS implementation
closure for native verified HTTPS. Other translation units include declarations
only; C++ consumers can link the C implementation.

## Quick start

```c
mulib_openai_client_t c = mulib_openai_client_default(getenv("OPENAI_API_KEY"));
mulib_json_t *body = mulib_openai_responses_request("your-model", "Hello");
mulib_openai_response_t r = {0};
if (body) {
    int rc = mulib_openai_client_json(&c, "createResponse", body, &r);
    if (rc == MULIB_OPENAI_OK) {
        mulib_json_t *text = mulib_openai_output_text(r.json);
        /* Read text with mulib_json_text(text, &length). */
        mulib_json_destroy(text);
    }
    /* API_ERROR also owns r: inspect status, r.json and response headers. */
    mulib_openai_response_free(&r);
    mulib_json_destroy(body);
}
```

`client_default` borrows the key; NULL permits unauthenticated compatible servers.
The default base URL is `https://api.openai.com/v1`. Configuration and request
pointers must live through the synchronous call. Independent clients have no
shared mutable state. `base_url` may include a path prefix, but not credentials,
query or fragment. A trailing slash is accepted. Set `organization`, `project`
and `beta` for their OpenAI headers; `transport` exposes MuW3 CA stores, socket
timeouts, limits and injected HTTP/TLS transports.

Calls never retry or follow redirects, preserving authentication scope and
non-idempotent POST semantics. Use per-request extra headers for idempotency keys,
client request IDs or application extensions; managed auth, framing, content-type
and accept headers cannot be overridden there. HTTP status, request IDs,
rate-limit fields and Retry-After are available through `r.http.message` and
`mulib_http_header`. Retry/backoff and cancellation between calls belong to the
application.

## Primitive and catalog calls

`mulib_openai_client_request` accepts either:

* `operation_id` plus `path`/`query` name/value arrays; or
* a raw `method` and origin-form `target` relative to the base URL, with optional
  extra `query` pairs. This allows future or server-specific endpoints.

Set `json` for serialized JSON, or `body`/`body_length` plus `content_type` for
binary, SDP or pre-encoded input. These are mutually exclusive. Catalog calls
check required-body and accepted MIME metadata; arbitrary JSON properties remain
available through MuLIB JSON. Request-specific schema semantics are application
responsibilities. `responses_request`, `chat_request` and `embeddings_request`
create owned minimal request objects; add tool definitions, messages, structured
output schemas, multimodal input, parameters and other fields with `mulib_json_set`.

`client_multipart` encodes borrowed binary/text parts and uses the same request
options, including path/query parameters. Supply a collision-free boundary;
collisions and header injection are rejected. For files, include `purpose` and a
`file` part with filename/media type. Repeated fields support multiple images.
Download operations and audio/SDP responses preserve exact binary bytes.

On `OK` **or `API_ERROR`**, `r` owns the MuW3 response and optional parsed JSON.
JSON is parsed when the response advertises application/json; binary remains in
`r.http.message.body` with explicit length. Invalid success JSON returns
`PROTOCOL` and leaves the caller's output untouched. Other failures likewise
leave output unchanged. Free before reuse; `response_free` accepts NULL or a
zeroed/already-freed record. Malformed JSON error bodies still retain raw HTTP
details. HTTP API errors include redirects and all non-2xx statuses.

## Live streaming

Set the protocol's `stream: true` JSON field and `request.event`. MuW3's incremental
HTTP decoder delivers decoded body slices as they arrive over HTTP or verified
HTTPS. No successful SSE body is retained. Each event borrows its event name,
ID, raw data and parsed JSON for the callback. `[DONE]` has NULL JSON; it and
terminal `response.completed`, `response.failed`, `response.incomplete` events set
`done`. Inspect event JSON to distinguish successful completion from an API-level
failure inside a 200 stream. Callback return 0 stops immediately with `CANCELLED`.

Callbacks run before final HTTP framing/TLS EOF verification and may already have
produced side effects when a later error occurs. The final response owns only
headers/trailers and `stream_done`. SSE EOF discards an unterminated event;
applications requiring terminal completion must check `stream_done`. Other event
families can have different terminal conventions. Non-2xx streaming responses
are buffered as normal API errors without invoking the event callback.

Default limits: 1 MiB per SSE event/line, MuW3's 8192-byte URL, 32 KiB headers,
100 HTTP fields, 16 MiB decoded body and 32 MiB raw response. Streaming body and
wire limits are **cumulative**, while memory is bounded by headers and one event.
Increase `transport.options.web` limits for large downloads/long streams. Socket
timeouts are per I/O wait, not whole-workflow deadlines. Content compression is
not decoded; the client requests identity encoding.

## Higher-level workflows

`client_pages` repeatedly requests a `data[]`/`has_more` cursor list and invokes
the item callback. It advances `after` using `last_id` (or the last item's ID),
rejects a stalled cursor and bounds page count. Set `max_pages > 0`; callback 0
cancels. The original request is unchanged. This helper applies to this pagination
envelope; endpoints using other cursor formats remain available through ordinary
requests. The latest received response is transferred to `final`, even when a
later page, callback, protocol check or page limit fails; free it on every path.

`client_run` implements a bounded Responses function-tool loop. Start with a
nonstreaming request containing your `tools`. Each `function_call` is passed to
the tool callback with parsed arguments; return an **owned JSON string** holding
the result. The next request supplies `function_call_output` items and
`previous_response_id`, preserving the initial model, tools and instructions.
Server-side response storage must be enabled. The maximum round count includes
the first request, and the helper checks the bound before executing more tools.
Tool execution is sequential and is never retried. The application selects and
validates tool functions and their arguments. `final` owns the latest response
whenever one was received; free it even when the result is an error. More elaborate
parallel tool, polling, stateless history or Chat tool workflows can be composed
from the primitive calls and JSON values.

## Examples, tests and MuLL

```sh
cc -std=c11 -Iinclude examples/modules/openai-client.c -lm -o /tmp/opencode/openai-client
/tmp/opencode/openai-client                         # offline fixture
OPENAI_BASE_URL=http://127.0.0.1:8000/v1 /tmp/opencode/openai-client synthetic-model Hello
make check-openai
```

For a remote endpoint, set `OPENAI_API_KEY` in the environment and pass a model
and prompt. No example embeds credentials. MuLL exposes the complete API as
`G:MulibWrap("openai-client")`; records, callbacks and ownership follow the
[shared MuLL bridge contract](../mudsl/mull/bridge/Mulib/README.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-openai-client.h`](../include/mulib-openai-client.h) |
| Implementation switch | `MULIB_IMPL_OPENAI_CLIENT` |
| Repository-local includes | [mulib-multipart](mulib-multipart.md), [mulib-muw3](mulib-muw3.md), [mulib-openai](mulib-openai.md), [mulib-sse](mulib-sse.md) |
| Complete example | [`examples/modules/openai-client.c`](../examples/modules/openai-client.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/openai-client.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
