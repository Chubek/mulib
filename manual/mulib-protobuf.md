# Protobuf wire fields

Define `MULIB_IMPL_PROTOBUF`, `VARINT` and `BUFFER`. Wire types 0,1,2,5 are
supported; no groups or generated schema semantics. Bytes views borrow input.
Read returns 1 success, 0 truncation, -1 invalid, leaving outputs unchanged on
failure. Write atomically appends. ZigZag handles the full int64 range.
[Example](../examples/modules/protobuf.c), [contracts](storage-server.md).
