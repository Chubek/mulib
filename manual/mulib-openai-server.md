# OpenAI protocol server

**Header:** `include/mulib-openai-server.h` · **Switch:** `MULIB_IMPL_OPENAI_SERVER`

This transport-independent library parses and dispatches all REST operations in
the [shared OpenAI catalog](mulib-openai.md), then serializes ordinary HTTP or live
chunked/SSE responses. It is suitable for inference backends, mock services,
gateways and application-specific OpenAI-compatible servers.

Enable `OPENAI_SERVER`, `OPENAI`, `SSE`, `MULTIPART`, `JSON`, `UTF8`, `HTTP`,
`URI`, `INET4`, `INET6`, `PARSEC`, `BUFFER` and `STRBUF` implementation switches
in one TU (each prefixed `MULIB_IMPL_`). No third-party library or network runtime
is required. The [example](../examples/modules/openai-server.c) additionally
uses MuLIB SOCKET to offer a sequential loopback listener. Socket acceptance,
concurrency, TLS termination and service lifetime remain host responsibilities.

## Host and handler contract

```c
static int handle(void *user, const mulib_openai_server_request_t *q,
                  mulib_openai_writer_t *w) {
    /* Select implementation using q->operation->id. */
    return 0; /* Unimplemented operation: library returns an OpenAI 501 error. */
}
mulib_openai_server_t s = mulib_openai_server_default(handle, context);
/* Feed complete/incomplete HTTP bytes to server_process with a write sink. */
```

`server_process` uses MuLIB's bounded HTTP parser. On MORE=0, retain the bytes
and retry after receiving more. On a complete valid frame, `consumed` identifies
its boundary and the handler is invoked once. Parse errors leave consumed=0 and
emit no bytes; the host should close that connection. The example sends one
response per connection. `server_dispatch` is available when a host already has
a **validated parsed HTTP request**. It borrows that message for the call.

The write sink consumes **all** bytes synchronously, returning 1, or returns 0 on
failure. A socket adapter must loop over short writes and handle timeouts. Failure
may happen after some bytes have been sent: close the connection. There is no
rollback after a write. Handler/request/writer pointers and parsed JSON are
borrowed only during dispatch. Do not retain them or recursively mutate the writer.

`base_path` defaults to `/v1`; set another slash-prefixed path without a trailing
slash, or an empty string for root mounting. Exact segment boundaries are checked.
Static routes take precedence over parameter routes. Use `mulib_openai_path_parameter`
with `q->operation`/`q->target`, and `mulib_openai_query`, to decode owned parameter
strings. `q->http` exposes the original headers and binary body. `q->json` holds
a parsed JSON object for JSON requests. For multipart, repeatedly call
`server_part` with cursor=0, freeing each decoded part's metadata; its data borrows
the HTTP body. The entire multipart envelope is checked before handler invocation.

## Authentication and validation

`api_key` optionally requires an exact Bearer credential. NULL permits requests
without a key. Key values are never included in generated error messages.
`authorize` optionally enforces per-operation policy using the parsed HTTP request
and matched operation; it runs **before body decoding**, so its `q->json` is NULL.
Return 1 to authorize, otherwise the library emits 403. The host can implement
multi-key/project policies here instead of setting a single fixed key.

The dispatcher detects missing required bodies, unsupported media/encoding,
duplicate Authorization/Content-Type, malformed JSON and multipart, unknown
paths and unsupported methods. It emits structured OpenAI JSON errors for
400/401/403/404/405/413/415/501. 401 includes a Bearer challenge; 405 includes Allow.
Schema-specific property requirements, model availability, tool execution,
pagination semantics, resource persistence and endpoint responses are implemented
by the handler. This is a protocol server toolkit, not an inference engine or
a full generated JSON Schema validator.

If a handler returns 0 before writing, dispatch emits 501; a negative result
before writing emits 500. Returning 1 after a reply completes normally. A
successfully started chunked response is terminated automatically if the handler
returns 1 without calling `done`. Errors after writing propagate to the host.
Dispatch returning 1 means a complete HTTP response, including generated HTTP
error responses, was sent.

## Responses and live events

* `server_reply`: buffered binary response with explicit status/media type and
  optional extra headers (request IDs, rate limits, Retry-After, etc.).
* `server_json`: serialize a borrowed JSON value.
* `server_error`: standard OpenAI error envelope with nullable param/code.
* `server_begin`: send headers immediately for a chunked response.
* `server_chunk`: send a binary chunk, respecting cumulative output limits.
* `server_event`: encode a borrowed JSON value as one SSE event. Requires a
  response begun with `text/event-stream`; event name and ID are optional.
* `server_done(w, 1)`: emit Chat/Completions-style `data: [DONE]` then terminate
  HTTP chunking. For Responses, emit the appropriate terminal named JSON event
  and call `server_done(w, 0)`.

Headers and framing are validated with MuLIB HTTP. The writer manages Connection,
Content-Type, Content-Length and Transfer-Encoding; extra headers cannot override
them. Each write is immediate. There is no whole-stream response buffer. Default
HTTP limits are 32 KiB metadata, 100 headers and 16 MiB cumulative body; per-event
default is 1 MiB. The parser also enforces incoming message bounds. Increase
limits explicitly for larger media requests or long event streams.

## Running the example

```sh
cc -std=c11 -Iinclude examples/modules/openai-server.c -lm -o /tmp/opencode/openai-server
/tmp/opencode/openai-server                # offline HTTP example
/tmp/opencode/openai-server --listen 8000  # loopback example service
```

`OPENAI_API_KEY` optionally protects the loopback example. It offers a model list
and synthetic Responses replies/events; substitute your own handler/backend.
The example is sequential and uses Connection: close. Tests exercise this service
with both the MuLIB client and Python's independent HTTP parser. Run
`make check-openai`. MuLL registration is `G:MulibWrap("openai-server")`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-openai-server.h`](../include/mulib-openai-server.h) |
| Implementation switch | `MULIB_IMPL_OPENAI_SERVER` |
| Repository-local includes | [mulib-multipart](mulib-multipart.md), [mulib-openai](mulib-openai.md), [mulib-sse](mulib-sse.md) |
| Complete example | [`examples/modules/openai-server.c`](../examples/modules/openai-server.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/openai-server.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
