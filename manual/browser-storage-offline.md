# Offline browser storage kernels

This is the storage-specific update to the browser additions staging ledger.
These mechanisms are for controlled offline inputs, not a storage broker,
authorization policy, OS isolation, or a browser conformance claim. They do not
enable script exposure or untrusted content. CacheStorage below is separate from
the HTTP transport cache and cookie jar.

| Kernel | Offline profile | Not implemented | Direct MuLIB staging dependency |
| --- | --- | --- | --- |
| `indexeddb-key` | Numbers except NaN (including infinities, with ±0 equal), followed by byte strings in lexicographic order. | Date, binary and array keys, UTF-16 ordering, key paths, serialization. | — |
| `indexeddb-store` | Caller-bounded sorted records, add/upsert/lookup/delete, and all-or-nothing batches via caller scratch storage. | Persistence, structured clone, key generators, concurrent or durable transactions. | `indexeddb-key` |
| `indexeddb-index` | Uniqueness predicate over caller-provided keys. | Primary-key ordering, multiEntry, maintained indexes and transactions. | `indexeddb-key` |
| `indexeddb-transaction` | Explicit commit/rollback callback dispatch, separate from the store batch API. | ACID database transactions, scope locking, scheduling and asynchronous events. | — |
| `indexeddb-cursor` | Iteration over a caller-maintained immutable record snapshot in either direction. | Snapshot creation, live transaction cursors, key ranges, seek and mutation. | `indexeddb-store` |
| `indexeddb-request` | One-shot result/error settlement. | Event dispatch, request reuse, transaction association and JS bindings. | — |
| `web-cachestorage` | Caller-bounded insertion, replacement, exact matching and deletion using literal `GET` and caller-provided URL bytes. | URL parsing/canonicalization, fragment/ignore options, Vary, named cache registry, response cloning and persistence. | — |
| `web-storagequota` | Checked byte-count reservation, release and replacement arithmetic. | Charge identity, persistence, eviction, shared origin quotas or storage-manager policy. | — |
| `web-opfs` | Lexical component and slash-delimited relative path checks with caller byte/component limits. | Filesystem brokers, symlink-safe resolution, filesystem-specific name rules, handles and synchronous access. | — |
| `web-storageevent` | Existing routing predicate for distinct nonzero document IDs with equal caller-supplied storage keys; unchanged by this slice. | Namespace policy, queued delivery and value snapshots. | `web-storagekey` |
| [Existing `webstorage`](mulib-webstorage.md) | Owned NUL-terminated strings, insertion-order key access, remove/clear, and opt-in per-write logical limits. | Persistent/session/origin-scoped storage, cross-store quotas and event dispatch. | — |

The staging headers use `kernels/detail/mulib-browser-additions-common.h`
and do not emit external symbols. `webstorage` uses its existing
`MULIB_IMPL_WEBSTORAGE` switch and ISO C allocation. No pending P0 interface or
third-party library is required. Do not treat declaration scanning as a callable
FFI adapter.

## Ownership and failure

- **IndexedDB store:** Start with `{records, 0, capacity}` where `records`
  is an actual array of at least `capacity` records (NULL allowed at zero
  capacity). Key spans are borrowed and must remain alive and immutable until
  removed; values are caller-owned opaque `uintptr_t`s, and zero is valid.
  `put` respects its overwrite flag and preserves the original stored key
  bytes on overwrite. `get` writes its output only on hit. Deleting a missing
  valid key succeeds. `apply` takes `{kind, key, value}` operations using the
  `ADD`, `PUT`, and `DELETE` enumerators. Operations apply in order; a duplicate
  add, invalid operation/key, or insertion beyond capacity rejects the whole
  batch. An empty batch needs no scratch. Otherwise provide a disjoint scratch
  array of at least `capacity` records. Keep the store struct, both arrays, the
  operation list, and the key bytes disjoint; the API does not check partial
  overlaps. Failure leaves the store untouched but scratch may have changed.
  This is synchronous rollback of record changes only, with no callbacks,
  allocation, locking or side-effect rollback. Mutations can invalidate record
  indices and borrowed views; no mutation may race a cursor over those records.
- **CacheStorage table:** Start with `{entries, 0, capacity}` using a real
  caller-owned array. New entries borrow immutable nonempty URL byte spans and
  nonzero response handles. Duplicate bytes replace the handle and retain the
  original URL span, even at capacity. `match(entries, len, method, url)`
  returns a borrowed handle or zero on miss; there is no URL transformation.
  `delete` returns 1 only for a removed entry. The host owns the old handles
  and their release. The caller is responsible for keeping unrelated namespaces
  apart and for excluding responses that need Vary-aware matching.
- **Quota:** `replace` subtracts the old charge and adds the new charge only
  after all checks; the caller is responsible for supplying genuine old
  charges. If a limit was lowered below current usage, replacement and release
  allow usage to stay the same or shrink; growth must fit the limit.
- **OPFS:** `relative_path` requires nonzero length within `max_bytes`, and
  one to `max_components` components. It rejects empty components, leading
  and trailing slashes, `.`, `..`, NUL, backslashes, and colons. This is a byte check,
  not a URL decoder, native filename validator or path resolver. A passing
  span must not be interpreted as authorization or proof of confinement.

No mutable object here is synchronized. These APIs assume valid caller-owned
memory and real array sizes, not forged pointers or mutable public state.

## Regression test

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels \
  tests/test_browser_storage_offline.c -o test-browser-storage
./test-browser-storage
```

The test exercises sorted lookup and rollback, bounded Web Storage writes and
allocation failure, CacheStorage exact matching and capacity, quota arithmetic
and path rejection. Full browser integration and web-platform tests are outside
this slice.
