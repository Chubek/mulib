# Independent SQLite-file SQL engine

[Manuals](README.md) · [Library index](index.md)

`mulib-sqlite.h` now implements a native SQLite-file database and parsec-based SQL
execution, **not yet the full SQLite system requested in the roadmap**. It uses
no linked/dynamically loaded SQLite, subprocess database backend or copied
SQLite amalgamation. Its support libraries are separate reusable headers:

* [`mulib-sqlite-format.h`](mulib-sqlite-format.md): values, record/varint codecs,
  file headers, table B-tree scanning and image construction.
* [`mulib-sqlite-pager.h`](mulib-sqlite-pager.md): snapshots, POSIX locks,
  rollback-journal commits and recovery.

The [full compatibility plan](sqlite-implementation-plan.md) remains the endpoint;
the current implementation must not be represented as full SQLite compatibility.

## Build and use

In exactly one C11 translation unit:

```c
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PEG
#define MULIB_IMPL_RNG
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_SQLITE_FORMAT
#define MULIB_IMPL_SQLITE_PAGER
#define MULIB_IMPL_SQLITE
#include "mulib-sqlite.h"
```

No dependency switch is enabled implicitly. Other C/C++ clients include the
declarations and link that C implementation. Link the platform math library
where required (`-lm` on typical POSIX systems). Native file storage requires
POSIX; memory databases do not. The runnable example is
[`examples/modules/sqlite.c`](../examples/modules/sqlite.c).

```c
mulib_sqlite_t *db = NULL;
mulib_sqlite_error_t error;
int rc = mulib_sqlite_open("example.db", MULIB_SQLITE_OPEN_CREATE, NULL,
                          &db, &error);
if (rc == MULIB_SQLITE_OK) {
    rc = mulib_sqlite_exec(db,
        "CREATE TABLE IF NOT EXISTS t(id INTEGER PRIMARY KEY, value TEXT);"
        "INSERT INTO t(value) VALUES('hello');", &error);
    /* Check rc, especially UNCERTAIN, before reporting success or retrying. */
    mulib_sqlite_close(db);
}
```

`:memory:` and NULL paths open memory storage. READONLY and CREATE are mutually
exclusive. Closing a connection with live statements returns BUSY without
destroying it. Closing an explicit uncommitted transaction discards its logical
changes. It never silently commits on close.

## Implemented SQL profile

The SQL grammar is compiled from PEG rules to actual `mulib-parsec.h`
combinators. Structural captures produce an owned AST; recognition is not a
permissive token gate and there is no second external SQL parser. A preparation
error does not mutate the database. Keywords are ASCII case-insensitive.
Unquoted identifiers, double-quoted/backtick identifiers with doubled delimiters,
bracket identifiers, whitespace, `--`/`/* */` comments, single-quoted strings,
hex BLOBs and numeric literals are recognized. This is a profile of SQLite
syntax; it does not yet reproduce all contextual keyword/legacy quoting rules.

* `CREATE TABLE [IF NOT EXISTS] name (...)`: named columns, declared type names,
  constant defaults (including parenthesized constant expressions), NOT NULL,
  and one column declared exactly `INTEGER PRIMARY KEY [ASC]`. That primary key
  aliases the rowid. Unsupported UNIQUE/CHECK clauses are recognized but rejected
  before persistent mutation. DESC/AUTOINCREMENT primary keys and table-level
  constraints are not implemented.
* `INSERT INTO name [(columns...)] VALUES (...), (...)` or `DEFAULT VALUES`.
  Bindings are preferable to interpolating user SQL. Explicit rowid aliases,
  generated rowids, default application, column affinities and primary-key/NOT
  NULL checks are implemented. Rowid allocation beyond the maximum signed rowid
  probes positive pseudorandom IDs using existing `mulib-rng.h` (not a security
  random-number interface). No implicit upsert is performed.
* `UPDATE name SET column=expression, ... [WHERE expression]` and
  `DELETE FROM name [WHERE expression]`. SET expressions see the old row.
  Statement failure restores the pre-statement model, including after a
  multi-row insert has already changed intermediate rows.
* `DROP TABLE [IF EXISTS] name`.
* `SELECT [DISTINCT] expressions-or-* [FROM one-table] [WHERE expression]
  [ORDER BY expressions [ASC|DESC], ...] [LIMIT integer [OFFSET integer]]`.
  Result aliases use AS. ORDER BY accepts aliases and one-based output ordinals.
  `main.name`, table-qualified columns, `rowid`, `_rowid_`, `oid`,
  `sqlite_schema` and its `sqlite_master` alias are supported. No-FROM scalar
  queries are supported. Results are bounded and materialized before the first
  row is exposed.
* Expressions: literal NULL/TRUE/FALSE, decimal/hex signed-64-bit or real numeric
  values, strings/BLOBs, columns, parameters, grouping, unary +/-, arithmetic
  `+ - * / %`, `||`, comparisons `= == != <> < <= > >=`, IS/IS NOT, LIKE,
  NOT/AND/OR. Arithmetic division by zero yields NULL; checked integer overflow
  promotes to real, avoiding C signed-overflow UB. Integer/real comparison does
  not first round all integers to binary64. NULL logic is three-valued.
* Scalar functions `coalesce`, `ifnull`, `typeof`, `length`, `abs`, `upper` and
  `lower`. Case conversion is ASCII. Aggregates `count(*)`, `count(expr)`,
  `sum`, `total`, `avg`, `min` and `max` without GROUP BY; min/max currently
  accept one argument. Unsupported functions fail even on an empty source.
* `BEGIN [DEFERRED|IMMEDIATE|EXCLUSIVE] [TRANSACTION]`, COMMIT/END, ROLLBACK,
  SAVEPOINT, ROLLBACK TO and RELEASE. Transaction/savepoint snapshots are owned
  in memory. Dirty statement state is not written until autocommit or commit.
  These keywords do not change the pager's lifetime locking policy.
* PRAGMA `user_version`, `application_id` (signed 32-bit assignment/query),
  `schema_version`, `page_size` (query), `table_info(name)` and `journal_mode`
  (DELETE query or quoted `'delete'` assignment). Unknown/unsupported PRAGMAs
  fail rather than silently succeeding. `table_info` reports saved default SQL
  spelling, not a guessed value-to-SQL reconstruction.

No joins, CTEs, subqueries, GROUP BY/HAVING, windows, compound SELECTs, IN/BETWEEN,
CASE/CAST, collations, CREATE INDEX, foreign keys, triggers, views, ALTER TABLE,
STRICT/WITHOUT ROWID, virtual tables/extensions, RETURNING/UPSERT/conflict modes,
ATTACH/DETACH, VACUUM, ANALYZE or EXPLAIN execution is implemented yet. Full
built-in function/date/JSON coverage, exact numeric/text conversion edge cases,
full SQLite error-code/diagnostic parity also remain. LIKE supports Unicode
code points with ASCII case folding, but not ESCAPE or custom collations;
invalid UTF-8 and BLOB LIKE operands are explicitly unsupported.
These are outstanding work, not successful no-op stages.

## Prepared statements and ownership

`prepare(db, bytes, length, &stmt, &tail, &error)` copies the SQL and AST for one
statement. It recognizes complete syntax through a semicolon or end of input,
not semicolon splitting inside strings/comments. `tail` is the consumed byte
offset. Empty/whitespace input yields a NULL statement. `exec` executes an
explicit NUL-terminated script and discards result rows; earlier autocommitted
statements are not rolled back if a later one fails. Identifiers/functions are
resolved on execution rather than fully at prepare time.

Parameter slots are one-based: `?`, `?NNN`, `:name`, `@name`, `$name`. Repeated
named parameters share a slot; numbered parameters can leave NULL gaps. The
supported named syntax is ASCII identifier syntax, not Tcl-style parameter
suffixes. `bind`, `bind_int64`, `bind_text`, `bind_blob` copy values before use.
Construct an explicit NULL value to bind SQL NULL; binding a zero-length text or
BLOB creates the corresponding empty non-NULL storage class.

`step` returns ROW, DONE or an error. Access values using `column`, not by parsing
formatted strings. `column_name`/`column_count` become available after initial
execution, even for an empty result. Borrowed column values remain valid until
reset/finalize. Statements retain a connection reference. Reset frees results,
preserves bindings and returns the prior execution error; clear_bindings resets
slots to NULL. Bind only before execution or after reset. Finalize releases all
statement allocations. Error/DONE states are sticky until reset.

The value model in `mulib-sqlite-format.h` preserves full signed integer
precision, TEXT vs BLOB, embedded NULs and empty BLOB vs NULL. NaN bindings become
NULL. Numbers parsed/formatted through the platform runtime have not yet been
qualified for bit-for-bit SQLite conversion compatibility under all locales and
floating-point environments.

The connection owns its last diagnostic; an optional output error copies it.
Errors include SQL offsets when available. `changes`, `last_insert_rowid` and
`autocommit` expose connection state. No statement or connection is thread-safe,
and there are no progress/interrupt/UDF callbacks yet.

## File interoperability and safety boundaries

The engine reads UTF-8, rollback-mode format-3 rowid tables whose CREATE TABLE
schemas fit the implemented profile. It traverses multi-level table B-trees,
supports SQLite page sizes 512–65536, overflow payloads, signed rowids, all
ordinary record storage classes and missing trailing columns filled by defaults.
Legacy schema formats may be readable but are write-blocked. Unsupported schema
definitions fail opening instead of guessing their column layouts.

Existing secondary index/trigger/view schema entries make the database
**write-blocked**: supported ordinary tables can still be queried, but updates
cannot silently abandon those dependencies. Reserved bytes and auto-vacuum
metadata likewise block rewriting. WITHOUT ROWID/UTF-16/WAL storage is rejected.
Queries of SQLite's schema include preserved non-table entries. An image rewrite
is allowed only when the entire stored schema is representable by this engine.
It builds all table/schema B-trees, then commits through the independent pager.

Native file locking and recovery restrictions are detailed in the
[pager manual](mulib-sqlite-pager.md). Writable connections hold an exclusive
native lock for their lifetime; there is no ordinary concurrent-reader/write
transaction behavior yet. Never use this implementation and another database
library on the same file in one process. Use only trusted/stable filesystem
paths. I/O errors poison the connection; close/reopen for recovery. UNCERTAIN
means the mutation committed but directory durability was not confirmed.

This is not yet a production-qualified replacement for SQLite. A successful
format roundtrip is not equivalent to full transaction/concurrency correctness,
hostile-file integrity validation or full SQL compatibility.

## Limits and verification

Zero options select 64 MiB file/payload, 1 MiB SQL, 100000 rows per table/result,
2000 columns/parameter slots and 10000000 evaluation/sort work steps. Columns/
slots are limited to 32766. Default parsec depth is 256, clamped at 512, with
parsec default step/capture bounds; expression evaluation depth is 128. Size
products are checked. B-tree depth is 64. These are operation bounds, **not**
total-memory quotas: snapshots, undo, schema models, ASTs, encoded cells, sorter
keys and query results may coexist. Writes currently rebuild snapshots and are
not appropriate for large databases or high-throughput workloads. Stable sorting
is quadratic but charged against the evaluation work budget.

`tests/test_sqlite.c` exercises typed queries, prepared bindings, constraints,
transactions/savepoints and rollback. `test_sqlite_alloc.c` sweeps allocation
failures through open, prepare and mutation, checking cleanup and undo without
allocating on the error path. `test_sqlite_format.c` covers record/varint/page
boundaries; `test_sqlite_pager.c` injects pager I/O failures.

Run cross-engine tests with an independently installed Python SQLite oracle:

```sh
python3 tools/check_sqlite.py --engine /path/to/mulib_test_sqlite
```

The runner tests typed results, native/reference alternating writes, independent
integrity checks, page sizes/overflow, unsupported-write preservation, native
lock conflicts, hot-journal recovery and WAL rejection. It reports the actual
oracle version, not a counterfeit pass against the full 3.54.1 target. SQLite
is used by the test runner only; no SQLite dependency enters the native engine.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sqlite.h`](../include/mulib-sqlite.h) |
| Implementation switch | `MULIB_IMPL_SQLITE` |
| Repository-local includes | [mulib-arena](mulib-arena.md), [mulib-parsec](mulib-parsec.md), [mulib-peg](mulib-peg.md), [mulib-rng](mulib-rng.md), [mulib-sqlite-pager](mulib-sqlite-pager.md), [mulib-strbuf](mulib-strbuf.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/sqlite.c`](../examples/modules/sqlite.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sqlite.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
