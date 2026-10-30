# wal

Write-ahead-log **byte codec**, with no file or durability operations. `append`
adds a complete record atomically to an initialized buffer. Lengths are uint32;
nonzero sequences must increase when read. Tombstones require empty values.
Reader borrows input and starts zeroed with data/size assigned.

Wire record: `MWAL`, LE32 key/value lengths, LE64 sequence, deletion byte,
four reserved zero bytes, key, value, LE32 CRC32 of preceding frame bytes.
`next` returns RECORD and advances; OK is clean EOF; TORN is an incomplete final
frame; negative errors mean invalid/corrupt data. Output/cursor change only on a
complete record. Callers decide whether to truncate a torn tail; never skip CRC
errors. Persistence, fsync, serialization of concurrent writers and recovery
publication are caller policy. Append inputs must not alias log storage.

Switches: `MULIB_IMPL_WAL`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`,
`MULIB_IMPL_CRC32`. Example: `examples/modules/wal.c`.
