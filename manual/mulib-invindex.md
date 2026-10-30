# invindex

Binary terms map to unique ascending uint64 document-ID postings. Zero initialize.
`add` copies a posting idempotently; `at` decodes by sorted position; `remove`
removes a single posting. Empty/NUL terms and document ID zero are supported.
No tokenization, normalization, ranking, position lists or automatic intersection.

Returns 0 success, 1 absent/out-of-range, negative invalid/allocation errors.
Edits copy posting bytes before publication, so failure preserves the index.
Empty posting lists remain interned. `free` releases/reset all owned data.
Switches: `MULIB_IMPL_INVINDEX`, `MULIB_IMPL_MEMTABLE`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_BYCUTILS`. Example: `examples/modules/invindex.c`.
