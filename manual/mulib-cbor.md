# CBOR

Define `MULIB_IMPL_CBOR`, `BUFFER`, `JSON`, `PARSEC`, `STRBUF` and `UTF8`.
RFC 8949 JSON-profile codec: text/arrays/text maps/null/bool/finite numbers.
No tags, bytes, indefinite lengths, half floats, duplicate keys or unsafe integer
rounding. Encode emits binary64 numbers. Exact owned decode is limited to
16 MiB/depth 64. [Example](../examples/modules/cbor.c), [contracts](storage-server.md).
