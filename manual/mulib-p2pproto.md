# p2pproto — multistream-select records

Define `MULIB_IMPL_VARINT` and `MULIB_IMPL_P2PPROTO`.
See [`examples/modules/p2pproto.c`](../examples/modules/p2pproto.c).

This module implements libp2p multistream-select token records: a canonical
unsigned LEB128 byte length, followed by a printable token and LF. Length
includes LF. Tokens are 1..1023 bytes, start `/`, or equal `na`/`ls`; controls,
spaces, embedded newlines and non-ASCII bytes are rejected. The version greeting
is `/multistream/1.0.0`. Encode returns 1/0 and writes to disjoint caller storage.

Decode returns 1 complete, 0 incomplete, -1 malformed/capacity. It copies a
NUL-terminated token without LF, requires minimal varints, and reports exact
consumed bytes. Outputs are unchanged except used=0 on failure. Retain the
entire partial frame for retry. `select` returns the index of an exact supported
protocol proposal or SIZE_MAX; only slash-prefixed proposals can be selected.
The application controls greeting/order/retry state and sends encoded `na` for
an unsupported proposal. `ls` is recognized as a token; list responses are
not generated automatically.

No transport, authentication, peer discovery, stream multiplexing or specific
libp2p application protocol is implicit. All storage is borrowed, no allocation
occurs. Tests: `tests/test_protocol_extensions.c`.
