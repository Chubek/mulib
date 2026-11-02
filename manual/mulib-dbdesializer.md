# Database value deserialization

Historical filename retained; API spelling is `mulib_dbdeserializer_read`.
Define `MULIB_IMPL_DBDESIALIZER` and the complete [dbserializer](mulib-dbserializer.md)
closure. Returns an owned JSON value or NULL. Input must contain exactly one
supported value and is limited to 16 MiB. No automatic format detection.
[Example](../examples/modules/dbdesializer.c), [profiles](storage-server.md).
