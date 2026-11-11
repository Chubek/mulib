# Server-sent events

**Header:** `include/mulib-sse.h` · **Switch:** `MULIB_IMPL_SSE`

A bounded incremental UTF-8 SSE parser/encoder. Also define `MULIB_IMPL_STRBUF`
and `MULIB_IMPL_UTF8` in the implementation TU. No network transport is implicit.

Create a parser with `mulib_sse_create(limit, callback, user)`, feed arbitrary byte
fragments with `feed`, call `finish` at EOF and `destroy` to release it. Zero
limit selects 1 MiB per line and accumulated event fields. Data is buffered only
until a blank-line event delimiter. UTF-8 may be split across feed boundaries.
LF, CR, CRLF, initial BOM, comments, multiline data, event names and persistent
IDs are supported. Unknown fields (including retry) are ignored: reconnect timing
is application policy. Default event name is `message`. ID fields containing NUL
are ignored. Unterminated events are discarded at EOF.

Callbacks borrow `event`, `id`, and `data` until they return. Read data using
`data_length`; it may contain NUL. Return 1 to continue or 0 to cancel. Results:
1 success, -1 invalid input/UTF-8, -2 resource limit, -3 allocation failure,
-7 callback cancellation. Feed errors are sticky. Do not recursively feed,
destroy or mutate the same parser inside its callback.

`mulib_sse_encode` copies one event into malloc-owned bytes and returns its explicit
length. It splits multiline UTF-8 data into data fields; event/ID must not contain
CR/LF. This encoder rejects embedded NUL and protects against field injection.
Zero limit selects 1 MiB. Outputs remain unchanged on failure; free successful
output with `free`. An empty data field still generates an event.

See [the example](../examples/modules/sse.c), `tests/test-sse.c`, and the
[OpenAI client](mulib-openai-client.md) for streaming JSON usage. MuLL exposes
`G:MulibWrap("sse")` with the same callback and ownership contracts.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sse.h`](../include/mulib-sse.h) |
| Implementation switch | `MULIB_IMPL_SSE` |
| Repository-local includes | [mulib-strbuf](mulib-strbuf.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/sse.c`](../examples/modules/sse.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sse.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
