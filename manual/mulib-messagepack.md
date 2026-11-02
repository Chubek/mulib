# MessagePack

Define `MULIB_IMPL_MESSAGEPACK`, `BUFFER`, `JSON`, `PARSEC`, `STRBUF` and `UTF8`.
JSON-profile codec with text-keyed maps, no bin/ext, duplicate keys or unsafe
integer rounding. Float32/64 decode; numbers emit float64. Exact owned decode
is limited to 16 MiB/depth 64; encode atomically appends.
[Example](../examples/modules/messagepack.c), [contracts](storage-server.md).
