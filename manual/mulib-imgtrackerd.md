# imgtrackerd

In-memory reference counts keyed by syntactically valid OCI sha256 digests.
Zero initialize. `retain` increments with overflow checking; `release` decrements
or returns 1 when unretained; `count` returns zero absent/invalid. Changes return
0 success, negative invalid/allocation errors. Zero-count keys remain stored.
`free` releases/reset records. No image/blob ownership is transferred.

Not a daemon, persistent tracker, authenticated content store or blob garbage
collector. Syntax checks do not verify a digest against content.
Switches: `MULIB_IMPL_IMGTRACKERD`, `MULIB_IMPL_MEMTABLE`, `MULIB_IMPL_BYCUTILS`,
`MULIB_IMPL_OCIIMAGE`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`,
`MULIB_IMPL_UTF8`. Example: `examples/modules/imgtrackerd.c`.
