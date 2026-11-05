# mulib-cache

Explicit binary key/value disk caches with selectable indexes. Define
`MULIB_IMPL_CACHE`, `MULIB_IMPL_KVDB`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_VARINT`,
`MULIB_IMPL_CRC32`, `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_HATTRIE`, `MULIB_IMPL_HASH`,
and `MULIB_IMPL_TRIE` before including `mulib-cache.h` in one C11 translation
unit. No dependency implementation is enabled implicitly.

`mulib_cache_create(backend)` creates a memory-only cache;
`mulib_cache_open(path, create, backend, &cache)` opens/creates a bound snapshot.
Backends are `MULIB_CACHE_SORTED` (KVDB's binary-search sorted table),
`MULIB_CACHE_HATTRIE` (compressed/hash trie) and `MULIB_CACHE_TRIE` (byte trie).
Trie indexes contain stable slot IDs resolving directly to value views, not an
extra binary search per hit. KVDB remains the canonical ordered snapshot store.
Index choice changes lookup/storage overhead, not the portable file format.

`set` copies binary keys/values, including empty spans/NULs. `get` borrows a
value until mutation/destruction. `remove` never allocates. `size` and `at`
provide ordered enumeration. Inputs may be borrowed from the same cache.
Mutations preserve mappings on failure; internal capacities may grow.

All integer return codes are `mulib_kvdb_result`: zero success, negative error,
and positive `MULIB_KVDB_UNCERTAIN` only after disk replacement when directory
durability is uncertain. Output pointers/views are unchanged on failure.
`destroy` accepts NULL and **never implicitly writes**.

## Persistence and use

- `serialize(cache, buffer)` atomically appends a complete portable KVDB snapshot.
- `deserialize(bytes, size, backend, &cache)` owns all decoded bytes and rebuilds
  the requested index. Trailing bytes, unordered/duplicate keys and bad CRCs fail.
- `commit` writes the bound path; memory-only caches return EINVAL.
- `save(cache, path)` writes an explicit path without rebinding or clearing dirty
  state. POSIX saves use an exclusive mode-0600 sibling file, fsync, rename and
  parent-directory fsync. Other platforms return ENOTSUP for disk commits/saves.

Use trusted, stable parent directories. CRC32 detects accidental corruption,
not malicious modification. There are no locks: concurrent writers are
last-commit-wins. Callers supply versioned/namespaced keys and invalidation
policy. This is not an automatic TTL/LRU service, filesystem sandbox, or database
transaction manager. It does not automatically cache arbitrary function calls.
Serialized DFA/FST/hyphen/spelling models can be stored as ordinary cache values.

See [`examples/modules/cache.c`](../examples/modules/cache.c). Disk roundtrips,
binary/empty keys, cross-index decoding, corruption, aliasing and injected
allocation failures are checked in the focused language tests.
