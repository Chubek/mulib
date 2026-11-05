# SQLite snapshot pager

`mulib-sqlite-pager.h` owns bounded main-file snapshots and implements native
POSIX file locking, SQLite DELETE rollback-journal commits and journal recovery.
It is independent of SQLite, is not an SQL engine, and does not use MuLIB's
incompatible `MWAL` or key/value snapshot formats.

## Integration and ownership

Define `MULIB_IMPL_BUFFER`, `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_SQLITE_FORMAT` and
`MULIB_IMPL_SQLITE_PAGER` in exactly one C11 implementation translation unit.
See [`examples/modules/sqlite-pager.c`](../examples/modules/sqlite-pager.c).
C++ clients may include declarations and link the C implementation.

`pager_open(path, flags, max_bytes, &pager)` publishes an owned handle only on
success. NULL/`:memory:` paths select memory storage. Flags are READONLY or
CREATE (not both); zero requires an existing writable file. CREATE may create a
zero-byte file, which remains empty until the first successful commit. Memory
storage works on other platforms; native non-POSIX file opens return UNSUPPORTED.
The default file bound is 64 MiB.

`pager_data` borrows the current snapshot until commit/close. `pager_commit`
copies and validates a complete replacement format-3 image before persistent
mutation. Existing page size must remain unchanged. `pager_close` releases all
memory/locks/handles and never implicitly commits. Individual handles are not
thread-safe. The registry used to detect duplicate native handles is protected
with a C11 atomic flag.

## Locking policy

Writable handles hold an exclusive POSIX advisory lock over SQLite's native
512-byte locking range starting at `0x40000000`. Read-only handles hold a shared
lock over that range. Locks last for the **entire handle lifetime**, not one
transaction. This is deliberately more restrictive than SQLite's ordinary
reader/reserved/pending/exclusive transitions: it prevents concurrent native
writers and stale cached snapshots, but does not implement SQLite's usual
concurrency levels or busy retries.

A second same-file handle in this implementation/process is rejected with BUSY
before opening another descriptor, including ordinary hard-link aliases. Do not
mix this pager with other database libraries or independently opened descriptors
for the same inode in the same process: POSIX closes can invalidate process-wide
locks. Paths and their parent directories must remain trusted and stable during
open/use. Symlink-replacement races, hostile directory mutation, network
filesystem locking and Windows VFS semantics are not qualified. Native locks
interoperate with separate SQLite processes, as tested by the differential suite.

## Commit and recovery

For a native commit the pager:

1. Allocates the new snapshot and constructs a rollback journal containing every
   original physical page, with compatible page numbers and sparse checksums.
2. Exclusively creates `DATABASE-journal`; an existing journal is never replaced.
3. Writes a cold journal header and page records, syncs, writes/syncs its valid
   magic, and syncs the parent directory before modifying the main file.
4. Writes/truncates/syncs the main file, deletes the journal (the commit point),
   and syncs the directory.
5. Publishes the already allocated in-memory snapshot without a post-commit
   allocation that could misreport an allocation failure as a rollback.

OK means committed. UNCERTAIN means journal deletion committed the change but
directory durability was not confirmed. Do not retry such an operation as if it
had rolled back. An I/O failure after journal creation poisons the pager; close
and reopen for recovery. It retains potentially needed recovery information
instead of deleting it on the error path. A failure before the commit point can
leave a hot journal and a partially written main file; no other operation should
use that poisoned handle.

On open, recovery runs under the native lock before interpreting the main
header. It loads and validates the bounded journal, its segments, page numbers
and checksums **before** writing restored pages, truncates to original size,
syncs, removes the journal and syncs the directory. Repeating recovery after an
interruption is intended to be idempotent. Read-only opens refuse a hot journal
instead of returning unrecovered data. Cold journals can be removed by writable
opens. Unsupported journal trailers/super-journals are rejected, not ignored.

The initial journal writer uses 512-byte sectors and `/dev/urandom` for checksum
nonces. The recovery decoder accepts sector/page sizes through 65536 and
multi-segment journals with finite or all-remaining record counts. This is not
a device-capability-aware SQLite VFS: do not infer power-loss guarantees for
arbitrary filesystems, sectors or atomic-write devices from the existence of a
journal. No WAL/wal-index, checkpoints, multi-database super-journal commit,
shared-cache protocol, memory-mapping, journal-mode switching or arbitrary
page-size conversion is implemented. Nonempty `-wal` files or WAL main-file
headers are rejected even when the main file alone looks readable.

Tests: `tests/test_sqlite_pager.c` injects write/sync/truncate/unlink failures at
every operation in a small commit, reopens and verifies old/new committed state,
and checks duplicate-handle locking. `tools/check_sqlite.py` additionally tests
mixed-engine native locks and recovery fixtures with real SQLite. These tests
are not exhaustive power-loss certification; a capability-aware VFS and broader
crash/reordering tests remain on the full-compatibility roadmap.
