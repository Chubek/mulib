# Independent SQLite implementation plan

Status: **an independent, bounded engine is implemented; full compatibility is
not yet implemented**. See [the actual SQL/storage profile](mulib-sqlite.md),
[format codecs](mulib-sqlite-format.md) and [pager](mulib-sqlite-pager.md).

The requested endpoint is `include/mulib-sqlite.h`: an independent C11 database
engine that opens, queries and updates SQLite databases, with SQL parsing built
on `include/mulib-parsec.h`. A system SQLite wrapper, subprocess backend,
renamed SQLite amalgamation, or proprietary replacement file format does not
satisfy this request. Intermediate milestones below are not the final scope.

Do not create an API full of stubs, advertise an implemented SQLite module, or
mark this plan complete when only a SQL subset works.

## 1. Compatibility target

Pin the initial reference to **SQLite 3.54.1**, released 2026-10-16, rather than
an unspecified moving version. The official release history gives source ID:

```text
2026-10-16 14:45:20 555e07ef33f69a0957d24d8e555b702464a131d6f1607f4c3c1e4551e8e6ca76
```

Reference: <https://www.sqlite.org/releaselog/3_54_1.html>.

Before implementation, record a reference build's compiler, platform,
`sqlite_version()`, `sqlite_source_id()`, `PRAGMA compile_options`, default
connection settings and runtime limits. Archive its build recipe and test
fixture hashes. Older system/Python SQLite installations may provide additional
oracles but must not silently replace the pinned reference.

Keep these compatibility surfaces separate:

| Surface | Required endpoint | Evidence |
|---|---|---|
| Database storage | SQLite format 3, existing databases and newly written databases | Bidirectional open/query/update, independent integrity checks |
| Recovery and concurrency | Rollback journals, WAL, locking, snapshots and checkpoints | Crash simulation and mixed-engine multi-process schedules |
| SQL | Core SQL grammar and semantics of the reference configuration | Typed differential query and error corpus |
| SQL functions and collations | Built-ins enabled by that configuration | Per-function argument, result, encoding and error tests |
| Connection behavior | Transactions, prepared statements, binding, schema invalidation and cancellation | Lifecycle and failure-injection suites |
| Platform behavior | Explicitly qualified OS/VFS implementations | Per-platform locking and durability evidence |
| Optional extensions | Individually enumerated, independently implemented modules | Separate feature matrix; no blanket claim |

The MuLIB API uses `mulib_sqlite_` names. This does **not** imply binary
compatibility with `sqlite3_*`, native SQLite loadable extensions, its internal
opcodes, or CLI dot commands. Those are separate surfaces to inventory, not
excuses to omit core SQL behavior. A compatibility claim must identify the
version, build configuration, APIs, optional features and platforms it covers.
An unqualified claim of full compatibility is blocked while any required item
remains missing. Query plans and physical page layouts may differ when the
observable behavior remains valid; matching planner performance is not required
for initial correctness.

No third-party SQLite code is linked into or executed by the implementation.
An external reference SQLite is permitted **only in development tests**. Keep
specification links, fixture provenance and licensing with any imported test
material. Proprietary TH3/dbsqlfuzz tests are not assumed available.

## 2. Repository reuse decisions

These decisions are based on the current public headers, not module names:

| Existing module | Use or limitation |
|---|---|
| `mulib-parsec.h` | Required recognition engine for SQL tokens and SQL grammar; use bounded runs and transactional captures |
| `mulib-peg.h` | Optional grammar authoring layer compiling to parsec; not a second SQL parser |
| `mulib-arena.h`, `mulib-buffer.h`, `mulib-strbuf.h` | Candidate statement storage and temporary buffers, with checked sizes and explicit ownership |
| `mulib-endian.h` | Candidate bounded fixed-width integer codecs |
| `mulib-utf8.h` | Candidate UTF-8 operations; do not impose stricter text validity than the selected SQLite behavior |
| `mulib-varint.h` | **Not SQLite varints**: it implements unsigned LEB128; implement SQLite's 1–9 byte big-endian codec separately |
| `mulib-wal.h` | **Not SQLite WAL**: its `MWAL` CRC-framed key/value records are incompatible |
| `mulib-kvdb.h` | **Not a SQLite pager**: custom sorted snapshots, whole-file rename commits and no writer locking |
| `mulib-vfs.h` | Path routing/read/write interface lacks durable sync, truncate, native file handles, locking and shared memory; cannot directly provide SQLite durability |
| `mulib-sql.h` | Key/value memtable SQL subset, with different parsing and semantics; not the SQLite engine |
| `mulib-row-rdbms.h`, `mulib-documentdb.h` | Different storage/schema model; cannot substitute for SQLite tables or indexes |
| `mulib-json.h` | Not a general SQL value representation: binary64 numbers cannot preserve all signed 64-bit integers, text and blobs are distinct, and SQL NULL/coercion rules differ |

Do not modify these independent APIs to pretend that their formats are SQLite
formats. Reuse appropriate primitives without silently enabling implementation
switches. Register new dependencies only when actual code uses them.

## 3. Intended architecture

The engine's public entry point is `include/mulib-sqlite.h`, with a public guard,
C linkage and a `MULIB_IMPL_SQLITE` implementation section. Per the clarified
requirement, reusable support libraries are separate `include/mulib-*.h` modules:
`sqlite-format` and `sqlite-pager` currently implement native storage layers,
while existing parsec/PEG/arena/buffer/endian/RNG/strbuf modules are reused.
Each module has its own explicit implementation switch, manual, example and
tests. Do not force consumers to ship unlisted implementation fragments.

```text
application
    connection / statement / typed-value API
    parsec SQL recognition -> AST -> resolver -> logical plan -> execution VM
    schema, affinities, collations, constraints, functions and triggers
    table / index cursors, sorter and temporary storage
    SQLite records and B-trees
    pager / page cache / statement undo / savepoints
    rollback journal or WAL / snapshot and lock manager
    durable file VFS / OS or deterministic test backend
```

### 3.1 API and ownership

Design and test the public ABI before freezing it. Required object families:

* Owned database connections and statements. Closing with live statements must
  have a documented busy/deferred-close policy, not dangling pointers.
* Length-delimited SQL preparation with a precise consumed/tail offset. Prepare
  one complete statement; an execution helper may intentionally iterate a
  script. Semicolons inside strings, comments and triggers are not split points.
* Positional/named parameter discovery, bind, clear-bindings, step, reset and
  finalize. Distinguish row, done and error. Reset and binding retention must
  follow the documented compatibility policy.
* SQL values with separate NULL, signed `int64_t`, binary64, TEXT and BLOB
  representations. Copy/destructor-based binding ownership must be explicit.
  Document column-result lifetime and conversions, including empty BLOB vs NULL.
* Owned diagnostics with primary/extended result codes, SQL byte offset, and
  optional file/page context. Keep resource-limit, corruption, unsupported
  capability, I/O, busy, constraint and misuse errors distinguishable.
* Changes, total changes, last insert rowid, autocommit/transaction state,
  interruption, busy handling, limits and progress callbacks.
* Registration of scalar/aggregate/window functions, collations and virtual
  table modules, with callback ownership, determinism and reentrancy rules.

Use independent handles with no accidental global connection state. Provide an
explicit allocator interface and VFS interface early, so both transient and
persistent failures can be injected. Statement arenas must never own live pager
pages. Separate row-view lifetimes from pinned-page lifetimes.

### 3.2 SQL parsing must genuinely use parsec

The grammar must recognize SQL structure, not merely accept a string or tokenize
it before passing it to another engine. No permissive fallback parser.

1. Specify the pinned SQLite token rules, keyword fallback and quoting behavior:
   whitespace/comments, identifiers, strings, BLOB/numeric literals, parameters,
   operators, UTF encodings and termination. Keep original spans and spelling.
2. Implement token recognition using parsec combinators. A token-stream grammar
   may recognize encoded token kinds with parsec while mapping every capture
   back to the original byte span. Contextual identifier handling must preserve
   keyword information, rather than globally rewriting all keywords to names.
3. Express statement and expression structure through parsec sequences, choices,
   references and captures. Eliminate left recursion explicitly; left-fold
   operator chains where SQLite is left-associative. Apply the reference
   precedence table, including COLLATE, NOT, IS, BETWEEN, IN and JSON operators.
4. Construct an owned AST only after successful recognition. Grammar actions
   must not perform DDL, mutate connection state or invoke user SQL functions.
5. Separate parsing, binding/name resolution, semantic validation and execution.
   A syntax error or failed preparation must not start a write transaction.
6. Apply parsec depth, step, capture and work budgets. Bound tokens/AST bytes as
   well. Diagnose limits instead of using unbounded native recursion.
7. Audit ambiguous grammar branches for SQLite rather than PEG semantics:
   contextual keywords, UPSERT/SELECT ambiguity, qualified names, trigger bodies,
   aliases, compound queries and trailing statement boundaries need dedicated
   fixtures. PEG ordered choice is not itself proof of SQL compatibility.

The grammar inventory must cover SELECT/VALUES, joins, compounds, CTEs and
recursive CTEs, subqueries, windows, all DML/RETURNING/UPSERT, DDL, triggers,
views, transaction/savepoint statements, ATTACH/DETACH, PRAGMA, VACUUM, ANALYZE,
REINDEX and EXPLAIN. Maintain individual feature rows rather than one broad
"parser complete" checkbox. SQLite compatibility is not generic SQL compliance.

### 3.3 Values, schema and execution

Implement storage class, declared affinity, expression affinity, comparison and
CAST rules separately. Preserve 64-bit integer precision, integer-to-real
overflow behavior, numeric text conversion and locale independence. SQL NULL
uses three-valued logic; C truthiness is not its semantics. Handle collations,
text encodings, embedded NULs, NaN/infinity behavior and subtype propagation
according to reference fixtures, not convenient JSON rules.

Read `sqlite_schema` from root page 1 and compile stored SQL with the same
parser. Resolve `main`, `temp` and attached databases. Model rowid aliases and
shadowing, INTEGER PRIMARY KEY quirks, AUTOINCREMENT/`sqlite_sequence`, missing
trailing record fields/defaults, generated columns and STRICT tables.

Use a small verified execution VM with explicit cursors, registers, value
ownership and transaction boundaries. Start with correct scans; introduce index
selection and optimization only behind semantic equivalence tests. Implement
constraints, immediate/deferred foreign keys, conflict policies, triggers and
statement rollback before claiming safe general writes. An INSERT must never
silently become an upsert. Savepoints require independent undo boundaries, not
just SQL syntax recognition.

Sorting, DISTINCT, grouping, aggregates, windows, compound SELECT and recursive
queries need bounded temporary storage and interruption checks. Without ORDER
BY, compare permitted result multisets in differential tests; do not require one
incidental scan order. Respect query cases whose result/error choice is
explicitly unspecified by SQLite.

### 3.4 File format and B-trees

Implement and independently test:

* The 100-byte header, page sizes 512 through 65536 (including the encoded value
  1), usable/reserved bytes, header version fields and valid-for/change-counter
  rules. Distinguish invalid, unknown read format, and future write-only format;
  the latter may permit read-only access under the published format rules.
* SQLite 1–9 byte varints and record serial types, including signed 24/48-bit
  integers, 0/1 serial types, large text/BLOB lengths and exact endian handling.
* All four B-tree page types, page-1's 100-byte prefix, sorted cell pointers,
  freeblocks/fragments, rightmost children, local payload thresholds and
  overflow chains. Validate offsets before reads and detect cycles/duplicates.
* Table and index cursors, index record ordering, descending columns, compound
  keys, partial/expression indexes and WITHOUT ROWID storage/reordered columns.
* Page allocation/freeing, splits, merges, root changes, freelists, pointer maps,
  auto-vacuum/incremental vacuum, lock-byte page and large-file offsets.
* UTF-8/UTF-16LE/UTF-16BE storage and schema formats 1–4. Reserved extension bytes
  must never be overwritten as if they were free space. Unknown encryption or
  custom page transformations require a codec, not guessed decoding.

A reader cannot assume every root is a table leaf or that overflow only occurs
in table data. A writer must maintain every affected index, constraint and
schema dependency. Never rebuild an existing database from only recognized
tables and discard the rest.

### 3.5 Pager, durable I/O and locking

Define a SQLite-specific VFS contract covering open/close, exact offset I/O,
short reads/writes, truncate, size, sync, access/delete, directory durability,
file identity, lock/unlock/reserved-lock checks, randomness/time and temporary
files. WAL additionally needs shared-memory mapping, locks and memory barriers.
Device sector size and atomic-write guarantees must be explicit capabilities.
Do not infer them from a successful `fwrite` or `rename`.

First OS backend: POSIX, with a deterministic in-memory fault backend developed
alongside it. Other platforms remain individually unqualified until their
backends pass the same tests. Use 64-bit file offsets and checked conversions.

Rollback mode needs compatible journal records/checksums/sector padding,
hot-journal detection/recovery, journal-before-database ordering, commit
invalidation, database sync and lock transitions. Cover DELETE, TRUNCATE,
PERSIST and explicitly selected weaker modes. Multi-database transactions need
the compatible super-journal protocol; ATTACH cannot promise atomic commits by
committing files independently.

WAL mode needs compatible headers, frame checksums/salts, commit markers,
recovery, wal-index shared memory, read marks, writer/checkpoint locks, snapshot
visibility, checkpoint modes and WAL reset/reuse ordering. Include the upstream
WAL-reset corruption regression in the compatibility investigation. A WAL
database is not represented by the main file alone.

Maintain a tested pager state machine. A failed sync/commit must not simply
report success or assert that nothing changed: it may require rollback,
connection invalidation or an explicitly uncertain outcome. Recovery itself must
tolerate allocation/I/O failure without destroying recovery information.

Interoperate with SQLite's real lock ranges and protocols, not an unrelated
sidecar mutex. Account for POSIX process-wide locks, descriptor lifetime and
same-inode aliases. Mixed engines in one process require particular care because
closing a descriptor can affect another library's locks; do not claim this safe
without an established coordination strategy and tests. Cross-process testing
must include a real SQLite reader/writer, not only MuLIB peers.

Before recovery is implemented, a partial reader may explicitly reject a hot
journal or required WAL snapshot. It must never ignore either and return stale
or inconsistent data. Before a write feature is implemented, fail before the
first persistent mutation. Unknown PRAGMAs and journal modes must not be accepted
as successful no-ops merely to make tests pass.

## 4. Implementation milestones and gates

M1–M6 have **partial profile implementations**, with native codecs, table
B-trees, parsec SQL, POSIX snapshot/journal storage, constrained writes and
transaction/savepoint handling. None is complete against its full gate below.
M7–M10 remain outstanding. M0's full pinned-reference/ABI/VFS qualification is
also outstanding. This design and the new profile tests are not evidence of
full database compatibility. Later milestones are cumulative; each must include
runnable tests and honest coverage documentation.

| Milestone | Deliverable | Required gate |
|---|---|---|
| M0 | Pin reference configuration; finalize ABI/VFS contracts; enumerate compatibility cases | Reviewed contracts, reproducible reference recipe, no fake runtime API |
| M1 | Native format/header/varint/record codecs, bounded page validator and read-only pager | Hand-checked vectors, every page-size boundary, corrupt-file and OOM tests |
| M2 | parsec tokenizer, expression and schema grammar; owned AST | Lexical/precedence/quoting corpus, complete-input/tail tests, parser budgets and OOM sweeps |
| M3 | Table/index read cursors, schema resolver, typed scalar execution and read-only SELECT | Reference-created files, overflow/deep B-trees, encodings, rowid and WITHOUT ROWID reads |
| M4 | Durable VFS, locks, rollback journal, recovery and pager undo | Exhaustive small crash schedules, hot-journal fixtures, mixed-process locking and repeated failure tests |
| M5 | Native B-tree writes, DDL/DML, indexes and statement/transaction rollback | MuLIB-created files pass reference integrity checks; reference/MuLIB alternation preserves data |
| M6 | Constraints, conflict handling, foreign keys, triggers, savepoints and schema invalidation | Errors and post-error state match reference, including partial FAIL effects and deferred constraints |
| M7 | Complete core query language, functions, collations and advanced schema operations | Per-feature SQL matrix and public regression/SQL logic suites; no parser-only success claims |
| M8 | WAL, checkpoints, ATTACH multi-file atomicity and vacuum variants | WAL/journal cross-engine recovery, crash/reorder tests and concurrent schedules |
| M9 | Extension framework, configured built-ins and remaining reference-profile behavior | Explicit optional-feature/platform inventory and documented ABI boundaries |
| M10 | Release qualification and compatibility audit | All required matrix entries pass, no hidden skips, fault/fuzz/portability evidence published |

M1–M3 are deliberately read-only. M5's initial public write surface must remain
limited to feature combinations whose constraints and dependencies are already
implemented; broaden it as M6–M9 pass their gates. This is temporary engineering
sequencing, **not** a redefinition of the requested endpoint as a small SQL
subset. Do not claim general writable SQLite compatibility at M5.

## 5. Validation strategy

### Differential compatibility

Add a future `tools/check_sqlite.py` runner and a small native MuLIB test driver.
The runner may launch reference SQLite for tests; the driver must not link it.
Compare typed values with exact integer/text/BLOB fidelity, column metadata
where specified, primary/extended errors, transaction state, changes and rowids.
Freeze time/randomness or use appropriate predicates for nondeterministic
functions. Avoid normalizing away real type/NULL/collation differences.

Use reference-generated databases and the reverse direction. Alternate committed
transactions between engines. Include indexed/overflow/freelist/auto-vacuum
files, all page sizes and encodings, STRICT/WITHOUT ROWID/generated columns,
views/triggers/foreign keys, and schema changes from a second connection. Run
reference `integrity_check` and `foreign_key_check` on written output, plus
independent low-level invariants; integrity_check alone cannot establish correct
SQL semantics. Exercise real journal/WAL sidecars and committed snapshots.

### Fault and security testing

* Sweep allocator failure once and persistently through every API operation.
  Track buffers, pages, ASTs, file handles, locks, temporary files and callbacks.
* Fail every VFS operation individually and persistently, including failed
  recovery. Include disk-full, short I/O, truncation and failed file/directory
  syncs. Verify cleanup and reopen outcomes, not only return codes.
* Model durable versus unsynced bytes, torn sectors and reordered unsynced
  writes. Crash after each small operation schedule. Full-atomicity modes must
  recover an old or new committed state, never mixed table/index state; weaker
  durability modes need their own stated outcomes.
* Fuzz SQL, database bytes, journals/WAL and combined SQL/database inputs under
  explicit fuel/memory limits. Detect loops, recursion, offset overflow and
  hostile schema execution. A read-only query must not execute arbitrary
  extension code merely because a schema names it.
* Run strict GCC/Clang C11, C++ declaration clients, ASan/UBSan, signed/unsigned
  char, 32-/64-bit and big-/little-endian tests where available. Run the common
  storage/parser hardening suites whenever shared dependencies change.
* Add each discovered discrepancy as a minimized permanent regression.

Publicly available SQLite tests and SQL Logic Test are candidates after a
license/provenance review. Do not claim upstream's coverage or reliability from
merely adopting its test methodology. Local passes against an older installed
SQLite are not evidence of full 3.54.1 compatibility.

## 6. Repository integration and current blockers

At the first implemented milestone with a useful, complete public surface:

1. Add the actual header, `manual/mulib-sqlite.md`, a runnable
   `examples/modules/sqlite.c` and focused `tests/test_sqlite*.c` tests.
2. Keep limitations and milestone status prominent until M10. Do not expose
   unimplemented operations as successful stubs.
3. Run `tools/sync_inventory.py` and `tools/stratify.py`, reviewing generated
   changes. Never register a comment-only header as an implemented dependency.
4. Add differential/crash test wiring separately from normal dependency-free
   runtime builds. Missing reference tooling must produce an explicit skip or
   failure, not a counterfeit compatibility pass.
5. Run the inventory checker, CMake/CTest, strict builds and sanitizers; report
   both targeted results and unrelated repository failures accurately.

The earlier static-file inventory mismatch has been resolved separately from
SQLite: tests, examples, documentation and inventory now consistently use
`include/mulib-static_file.h`.

The implementation now adds `include/mulib-sqlite.h` and its two support
headers. Their manuals describe actual functionality and outstanding gaps.
The next compatibility work includes the full pinned reference recipe, a
capability-aware VFS, index/WITHOUT ROWID/UTF-16 support, complete SQL/schema
semantics, WAL and broader crash/concurrency qualification. The milestone gates
above must not be marked complete merely because the implemented profile passes
its focused tests.

## References

Official specifications and behavioral descriptions consulted for this plan:

* Release target: <https://www.sqlite.org/releaselog/3_54_1.html>
* File format, records and journal/WAL formats:
  <https://www.sqlite.org/fileformat2.html>
* SQL: <https://www.sqlite.org/lang.html>
* Expression syntax: <https://www.sqlite.org/lang_expr.html>
* Affinities and collations: <https://www.sqlite.org/datatype3.html>
* Deliberate SQLite SQL omissions: <https://www.sqlite.org/omitted.html>
* Atomic commit: <https://www.sqlite.org/atomiccommit.html>
* Locking: <https://www.sqlite.org/lockingv3.html>
* WAL: <https://www.sqlite.org/wal.html>
* OS interface: <https://www.sqlite.org/vfs.html>
* Corruption hazards: <https://www.sqlite.org/howtocorrupt.html>
* Testing and publicly/proprietarily available test suites:
  <https://www.sqlite.org/testing.html>

These links specify the intended behavior; they are not implementation code or
proof that the proposed architecture already meets it.
