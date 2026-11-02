# BSON

Define `MULIB_IMPL_BSON`, `BUFFER`, `JSON`, `PARSEC`, `STRBUF` and `UTF8`.
Object-root JSON-profile codec: double, string, nested object/array, bool, null;
int32/int64 also decode within safe double range. No ObjectId/binary/date types.
NUL keys and duplicates fail. Decode owns values, requires exact input, limits
depth to 64 and input to 16 MiB. [Example](../examples/modules/bson.c),
[contracts](storage-server.md#storage-and-serialization).
