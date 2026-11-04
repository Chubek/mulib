# HTTP/3 frames

Define `MULIB_IMPL_HTTP3`, `MULIB_IMPL_QUIC` and `MULIB_IMPL_BUFFER`. The codec
handles QUIC-varint type/length envelopes and validates frame-local HTTP/3 payload
structure. It does not implement QUIC transport, QPACK or connection state.

`mulib_http3_frame_read(data, length, max_payload, &frame, &consumed)` returns
1 OK, 0 MORE, -1 invalid, -2 limit, or **-3 allocation failure**. Both output
arguments are unchanged on every non-success. The payload borrows input.
`max_payload` is an explicit cap; zero permits only empty payloads. Limits are
checked as soon as the complete envelope is available, before waiting for the
advertised payload. Only a complete outer frame is semantically validated: a
truncated identifier *inside* that frame is INVALID, not MORE.

`mulib_http3_frame_write(&buffer, &frame)` appends transactionally and returns
1 success, 0 invalid/allocation failure. Initialize the buffer first. Payload
may alias its existing bytes. Type/length must fit QUIC's 62-bit integer range.
The envelope is written using minimal varints; payload bytes are retained.

## Payload validation

* HTTP/2-reserved frame types 0x2, 0x6, 0x8 and 0x9 are invalid in HTTP/3.
* CANCEL_PUSH (0x3), GOAWAY (0x7) and MAX_PUSH_ID (0xd) contain exactly one
  complete varint, without trailing bytes. PUSH_PROMISE (0x5) begins with one
  complete push-ID varint; the remaining QPACK bytes are opaque.
* SETTINGS (0x4) contains complete identifier/value varint pairs. Empty SETTINGS
  is valid. Duplicate identifiers are rejected, including unknown identifiers
  and identical integers represented with different varint widths.
* HTTP/2-reserved SETTINGS identifiers 0x2 through 0x5 are rejected.
  SETTINGS_ENABLE_CONNECT_PROTOCOL (0x8) and H3_DATAGRAM (0x33) must be 0 or 1.
  Unknown setting identifiers and their values remain valid.
* DATA, HEADERS and unknown extension/grease frame payloads remain opaque.
  Nonminimal QUIC varints are accepted on input, including in SETTINGS.

SETTINGS is first checked for well-formed pairs without allocating. Up to 32
identifiers fit in a stack array; larger frames allocate exactly eight bytes
per identifier, with overflow checks. Sorting detects duplicates in O(n log n)
work rather than a quadratic scan. Allocation failure is reported as -3 by the
reader and 0 by the writer, with no published output or leaked temporary array.

These are stricter checks than the earlier envelope-only codec. Stream roles,
the requirement that SETTINGS be first on the control stream, duplicate SETTINGS
*frames*, role-dependent GOAWAY IDs, push-ID limits, extension negotiation and
QPACK semantics still require the surrounding connection implementation.

[Example](../examples/modules/http3.c). Regression and allocation suites:
`tests/test_http_hardening.c`, `tests/test_http_alloc.c`. The shared fuzz target
also checks parse/write/read semantics and failure output preservation.
