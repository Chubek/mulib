# SQLite format-3 codecs and table B-trees

`mulib-sqlite-format.h` implements native SQLite varints, typed records, main-file
header inspection, rowid-table scanning and fresh table B-tree image construction.
It has no SQLite dependency and does not execute SQL or write files.

## Integration

Define `MULIB_IMPL_BUFFER`, `MULIB_IMPL_ENDIAN` and
`MULIB_IMPL_SQLITE_FORMAT` in one C11 implementation translation unit before
including `mulib-sqlite-format.h`. Other translation units include declarations
only. See [`examples/modules/sqlite-format.c`](../examples/modules/sqlite-format.c).
Eight-bit bytes and IEEE binary64 `double` are assumed for real records.

## Values and records

`mulib_sqlite_value_t` distinguishes NULL, signed 64-bit INTEGER, REAL, TEXT and
BLOB. Initialize values to zero. TEXT/BLOB bytes are owned, length-delimited and
have an extra convenience trailing zero. Embedded NULs and empty non-NULL values
are preserved. `value_bytes` and `value_copy` replace an initialized destination
only on success. `value_clear` frees its bytes and resets it to NULL. Do not free
two shallow copies of an owned value. Copies of real NaN normalize to SQL NULL.
There is no double conversion on the integer path.

`format_decode` accepts an exact record, returning an owned array and count only
on success. Use `format_values_free` to release it. The default column bound is
2000. Truncated headers, reserved serial types 10/11, invalid body lengths and
trailing record bytes fail. TEXT bytes retain the database's encoding; this layer
does not convert UTF-16 to UTF-8. Integer serial types include signed 24-/48-bit
values, and serial types 8/9 encode integers 0/1. `format_encode` appends a record
atomically with respect to allocation failures; it emits format-4 serial types.

The varint codec is SQLite's **big-endian, 1–9-byte** codec, not the LEB128 format
in `mulib-varint.h`. Truncated input fails without publishing a decoded value.
Non-minimal encodings are accepted. Negative rowids use their 64-bit two's
complement bit representation.

## Main files and reading

`format_open` borrows a complete file image. It validates magic, page size,
payload fractions, usable size, read format, schema/text encoding tags and page
counts, honoring the valid-for/change-counter rule. Page sizes 512–65536 are
supported; header value 1 means 65536. A physically short image is corruption.
It does not overlay WAL, recover a rollback journal, validate every page or
establish that the image represents the latest committed snapshot.

`format_scan` walks a rowid table rooted at a page number, in signed rowid order.
Interior and leaf table pages, page 1's 100-byte header, overflow records and
multiple tree levels are supported. Its callback borrows a raw record only for
the call and returns OK to continue. Any other callback result stops the scan.
The default payload bound is 64 MiB. Traversal tracks visited pages, detects
overflow/tree cycles and non-increasing rowids, checks byte offsets and limits
tree depth to 64. It is **not** a complete integrity checker: freelist ownership,
cross-table page sharing, exact fragment accounting and every index invariant are not
audited. Index B-trees/WITHOUT ROWID roots return UNSUPPORTED, not guessed rows.

## Writing fresh images

Initialize a builder (default 4096-byte pages, 64 MiB maximum image). Page 1 is
reserved automatically. Reserve other roots with `format_page` **before**
populating tables. Populate each zeroed root once with `format_table`, supplying
strictly increasing, unique signed rowids and borrowed encoded record payloads.
The builder owns the copied image, not the input row descriptors. It allocates
leaf/interior/overflow pages, uses SQLite's local-payload formula and promotes
root levels as necessary. Empty tables are valid.

Call `format_finish` to fill database size and version-valid fields, then publish
through a transactional pager. Customize schema/user/application/change fields
before finishing. The writer-version field is zero rather than an impersonated
SQLite release number. A newly built image uses UTF-8/schema format 4 and no
reserved page bytes, indexes, freelist or pointer maps. It is **not** an in-place
editor for arbitrary existing databases; callers must preserve all schema
dependencies or reject unsupported updates.

Discard a builder after a construction error; partial builder contents are not
failure-atomic. `format_page` returns zero for allocation/capacity failure.
`format_builder_free` releases its storage. Bounds apply to payload/file sizes,
not total transient memory: encoded cells, overflow pages and descriptors can
coexist during construction. Allocation sizes are checked for overflow.

Tests: `tests/test_sqlite_format.c` exercises signed integer boundaries, 9-byte
varints, truncated varints, every page size, multi-level trees and 7000-byte
overflow records. `tools/check_sqlite.py` validates native written files with an
independent SQLite engine. The format specification is
<https://www.sqlite.org/fileformat2.html>; implementation code is independent.
