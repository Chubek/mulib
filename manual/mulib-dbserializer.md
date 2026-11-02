# Database value serialization

Define `MULIB_IMPL_DBSERIALIZER`, `BSON`, `CBOR`, `MESSAGEPACK`, `BUFFER`, `JSON`,
`PARSEC`, `STRBUF` and `UTF8`. Dispatch selects one of four formats; all linked
codec implementations are required. Writes atomically append and return 1/0.
Profiles are intentionally restricted to shared JSON values.
[Example](../examples/modules/dbserializer.c), [profiles](storage-server.md).
