# mulib-http

[Manuals](README.md) · [Library index](index.md)

A strict HTTP/1.1 **wire-format utility**, not a socket client/server. Requests,
responses, binary bodies, content-length framing, chunked framing and
close-delimited responses are supported. Validated chunk extensions and separately
owned trailers are available with `MULIB_HTTP_CHUNK_METADATA`. HTTP/1.0, HTTP/2,
upgrade protocols and decompression are outside this focused API.

## Integration

Implementation switches: `MULIB_IMPL_HTTP`, `MULIB_IMPL_URI`,
`MULIB_IMPL_STRBUF`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`. Define all before including the header
in exactly one C11 translation unit. No external dependencies.
Start-line and header extraction use MuLIB parser combinators with captures.
Already-extracted token/value and numeric validation use allocation-free byte
scans; framing, overflow checks and resource policy remain in this module.
Target and Host checks reuse the URI parser, which shares IP-literal parsers.

## Parsing

`mulib_http_parse(bytes,length,response,options,&message,&consumed)` parses
**one** message. `response` must be 0 or 1. Results are `MULIB_HTTP_OK` (1),
`MORE` (0), `INVALID` (-1), `LIMIT` (-2), or `NOMEM` (-3).

- On OK, the output owns all strings, header pairs, and decoded body bytes.
  Release with `mulib_http_message_free`. Do not overwrite a live parsed output.
- Otherwise output is unchanged and consumed is zero. Retain the entire input,
  append bytes, and retry after MORE; this is length-aware, not a stateful feeder.
- Exactly `consumed` bytes belong to the frame. Trailing bytes remain untouched
  for pipelining; an exact-one-message caller must additionally compare consumed
  with length. A request without framing has an empty body, not an EOF body.
- `MULIB_HTTP_EOF` turns incomplete input into INVALID and terminates
  close-delimited responses. Such responses consume everything through EOF and
  cannot be pipelined. Connection management is the caller's responsibility.
- `MULIB_HTTP_HEAD_RESPONSE` supplies HEAD request context. It consumes only the
  response headers even if Content-Length describes a representation.
  `MULIB_HTTP_CONNECT_RESPONSE` similarly ends a successful CONNECT response
  at its headers. Hand subsequent tunnel bytes to another protocol; they are
   not HTTP bodies. Informational responses consume one frame independently.
   HEAD and CONNECT context flags are mutually exclusive and rejected on requests.

Options with zero limits select 32768 total start-line/header bytes, 100 header
fields, and 16 MiB decoded body. Limits apply before body allocation; chunk size
lines additionally have a fixed 128-byte bound. Unknown option flags are invalid.
`mulib_http_header` borrows the first case-insensitive matching value. Header
values have surrounding spaces/tabs removed; names preserve original case.
`mulib_http_header_next(message,name,&cursor)` iterates matching values including
duplicates; initialize cursor to zero. This preserves separate Set-Cookie fields.

## Incremental response decoding

`mulib_http_stream_create(options, callback, user)` creates a response-only
incremental decoder. `feed` accepts arbitrary fragments; `finish` is called at
transport EOF and transfers owned final headers/trailers on success. `destroy`
releases decoder storage. The callback receives final headers once with NULL/0,
then binary decoded body slices. Return 1 to accept or 0 to abort. The decoder
retains no body. It handles up to eight informational responses, fixed-length,
chunked and close-delimited bodies, extensions and trailers under the same flags
and validation rules as the buffered parser. It rejects extra bytes after a
complete frame and truncated fixed/chunked bodies at EOF.

Body limits are cumulative; metadata and chunk lines are bounded independently.
Feed/finish return OK or INVALID/LIMIT/NOMEM; failures are sticky. Outputs remain
unchanged on failure. Callbacks can occur before a later framing/transport error;
applications must not treat intermediate delivery as final verification. Do not
recursively feed, finish or destroy the decoder inside a callback. See
`tests/test-http-stream.c` for exhaustive fragment-boundary and truncation tests.

## Strict framing/security policy

The parser requires CRLF and exactly `HTTP/1.1`. It rejects bare line endings,
obs-fold, whitespace before `:`, invalid token names/methods, control bytes in
fields, duplicate Host, duplicate Content-Length **even when equal**, comma-list
Content-Length, signed/overflowing lengths, duplicate Transfer-Encoding, and
Transfer-Encoding plus Content-Length. The sole supported transfer coding is
exactly `chunked` (case-insensitive, outer whitespace trimmed). Every chunk
requires hexadecimal size, exact data length and CRLF; by default chunk extensions
and nonempty trailers are rejected. This conservative subset
reduces cross-parser ambiguities but does not replace careful proxy policy.

With `MULIB_HTTP_CHUNK_METADATA`, extensions use token names and optional
token/quoted-string values (including quoted-pair escapes); malformed syntax and
controls still fail. Valid extensions are discarded, never interpreted as framing.
The 128-byte chunk-line bound still applies. Trailers populate `message.trailers`
and `trailer_count`, separately from `headers`; `message_free` releases both.
Trailer bytes have their own `max_header_bytes` budget (including final CRLF), and
header+trailer field counts share `max_headers`. Framing, routing, authentication,
cookie, content-processing, cache-control fields and fields nominated by Connection
are forbidden in trailers. The exact prohibited-name list is in the header.
Allowed fields such as Digest can be examined explicitly by applications.
Serialization requires the flag and chunked encoding for nonempty trailers,
validates them before writing and emits canonical trailer lines after the zero chunk.

Every request requires one nonempty Host authority with a numeric port <=65535
if present, without userinfo. Origin form, http/https absolute form, CONNECT
authority form with explicit port, and OPTIONS `*` are supported. Fragments and
userinfo in request targets are rejected. Absolute/CONNECT authorities must
match Host (ASCII case-insensitive host and numeric port, default 80/443 where
appropriate). No DNS, IPv6 textual canonicalization, percent normalization, or
IDNA occurs. Thus equivalent but differently spelled authorities may be rejected.
Paths beginning `//` are rejected rather than confused with an authority.

Statuses must be 100..599. 1xx/204 and successful CONNECT responses reject
framing headers; HEAD/304 may carry Content-Length but no Transfer-Encoding.
These are deliberately stricter than some interoperable HTTP implementations.
HEAD/CONNECT context must be supplied correctly by the caller. Response body
length is never inferred from the status reason or a Connection header.
205 Reset Content responses cannot carry content: nonzero Content-Length,
nonempty chunks and nonempty close-delimited bodies are invalid. Unlike 204,
205 still uses normal message framing: Content-Length: 0, an empty chunked body,
or an empty close-delimited response completed by EOF. The zero chunk is consumed
before returning a chunked 205 frame boundary.

## Serialization

`mulib_http_serialize(&message,options,&bytes,&length)` borrows the message and
returns malloc-owned bytes, with an extra NUL outside the reported binary length.
Only outputs on OK; free bytes with `free`. Supply `method`/`target` for requests,
`status`/`reason` for responses. `chunked` selects canonical one-chunk output;
otherwise missing framing gets Content-Length. Existing framing headers must
agree with the chosen encoding and actual body (HEAD/304 lengths are metadata).
No-body responses cannot contain body bytes. `close_delimited` is informational:
serialization uses explicit framing instead of emitting an ambiguous EOF body.
Original method, target, status/reason and header fields are validated before
interpolation. Request targets must satisfy the same form/Host rules as parsed
requests and cannot contain raw whitespace or controls. Generated wire is then
reparsed with the same limits and compared with the original start-line fields
and body, rejecting request-line/header injection, length mismatch and accidental
trailing-message output. On failure both output arguments remain unchanged.
Header/body source pointers must remain valid until the call returns.
Before any wire/authority allocation, serialization bounds every source string
and preflights the **aggregate** start-line/header bytes, final CRLF and generated
framing header. Generated framing also counts against max_headers. Bounds use
checked subtraction rather than unchecked size sums. Over-limit output leaves
both output arguments untouched without first allocating an oversized buffer.

`mulib-http1.h` exposes matching parse and serialize entry points with identical
contracts. The owned request builder accepts a single `Transfer-Encoding:
chunked` header and encodes the supplied decoded body accordingly; unsupported
codings, duplicate framing and CL/TE conflicts remain invalid.

Regression suites: `tests/test_http_hardening.c` covers fragmentation, EOF,
smuggling-shaped headers, exact limits, contextual responses and binary owned
construction. `tests/test_http_alloc.c` fails each allocation in parsing,
serialization and construction and verifies unchanged outputs and released memory.

## Runnable example

[`examples/modules/http.c`](../examples/modules/http.c) parses and serializes a
chunked request containing an embedded NUL:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/http.c -o ./http
./http
```

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-http.h`](../include/mulib-http.h) |
| Implementation switch | `MULIB_IMPL_HTTP` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-parsec](mulib-parsec.md), [mulib-uri](mulib-uri.md) |
| Complete example | [`examples/modules/http.c`](../examples/modules/http.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/http.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
