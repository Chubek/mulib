# MuLIB ZIP: in-memory ZIP32 archives

[Manuals](README.md) · [Library index](index.md)

## Integration and supported format

Define `MULIB_IMPL_ZIP`, `MULIB_IMPL_DEFLATE`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_CRC32` and `MULIB_IMPL_UTF8` before including `mulib-zip.h` in one
translation unit. No link libraries. [Complete example](../examples/modules/zip.c).

The reader supports single-disk ZIP32 with stored (0) or DEFLATE (8) entries,
UTF-8 names, optional data descriptors with/without signature, comments, and
ordinary extra fields. It rejects ZIP64, encryption, other compression methods,
split disks, unsupported flags/versions, duplicate paths, overlapping local
records, bad directory offsets, inconsistent local/central names and sizes,
and unsafe names. Central-directory order may differ from physical-file order.
Local/central required versions and timestamps must agree; version requirements
and method-specific flags are checked. EOCD-like signatures inside comments are
skipped when their directory geometry is inconsistent.
CRC checking is deferred until extraction. Reading does not claim to validate
the contents of every unrequested resource.

Paths are nonempty relative UTF-8 names up to 65535 bytes. Absolute paths,
backslashes, colons, ASCII control characters, empty components, `.` and `..`
components are rejected. A final slash is allowed for an empty directory entry.
Names without the UTF-8 flag must still be valid UTF-8; CP437 conversion is not
implemented. No filesystem extraction, symlinks or permission restoration is
performed; external attributes are not interpreted.

## Reading

Zero-initialize `mulib_zip_reader_t`, then call `mulib_zip_open`. Input remains
borrowed and must stay immutable and live through `mulib_zip_close`. Open
allocates an entry index, sorted by local-header offset, and changes the reader
only on success. Opening an already-live reader is an argument error.

`mulib_zip_open_limited(reader,data,size,max_entries,max_uncompressed)` additionally
bounds index entries before allocation and checks the aggregate declared decoded
size with overflow-safe subtraction. `mulib_zip_open` retains its original API
and uses `SIZE_MAX` budgets. A fake empty EOCD inside a nonempty archive's comment
is not accepted as an empty archive. Extraction still verifies actual size and CRC;
open does not decompress every entry merely to estimate its expansion.

`reader.entries[i]` contains decoded scalars and a borrowed **non-NUL-terminated**
name slice. `mulib_zip_find` returns an entry pointer or NULL. It performs an exact,
case-sensitive linear search; no Unicode normalization occurs.

`mulib_zip_extract(reader, index, limit, out)` checks declared output size before
allocating, decompresses within that size, checks actual size and IEEE CRC32, then
appends atomically. The limit applies to this entry, excluding an existing output
prefix. Empty files are supported. Output may alias archive storage, but a
successful reallocation then invalidates the reader's borrowed input: close it
before further use. Close releases only the index and resets the reader.

## Writing and errors

Zero-initialize `mulib_zip_writer_t`; `mulib_zip_add` copies a unique path and
payload with method 0 or 8. Inputs must not alias either writer buffer. `(NULL,0)`
is valid payload. `mulib_zip_finish` appends the central directory and EOCD,
idempotently. Additions after finish fail. Borrow `writer.bytes` for the archive;
`mulib_zip_writer_free` releases both buffers and resets the writer.

Output uses UTF-8 flags, no extra fields/comments, fixed DOS date 1980-01-01,
and no data descriptors. It is deterministic. Failed additions/finish preserve
logical bytes and count, although buffer addresses/capacity may change.

All operations return zero on success. Errors are `EINVAL`, `EFORMAT`,
`EUNSUPPORTED`, `ENOMEM`, `ELIMIT`, or `ECRC`. `find` uses NULL for absence.
Entry sizes, compressed sizes, directory sizes and offsets must be below
`UINT32_MAX`; writer output including its EOCD must fit `UINT32_MAX` bytes;
entry count is at most 65534. Reader indexing is O(n log n),
extraction is linear in decoded size, writer duplicate checks are O(n) per add.
Owned writer/reader structs must not be shallow-copied. Independent objects may
be used concurrently; a single object requires caller synchronization.

## Verification

See `tests/test_ebook_formats.c`, `tests/test_ebook_alloc.c` and
`tools/check_ebook_formats.py`. Python `zipfile` verifies output and supplies
stored/deflated archives with comments and streaming data descriptors.
`tests/test_compression_codecs.c` also checks aggregate/index budgets and forged
empty EOCDs in comments. The ZIP32/encryption/method scope above is unchanged;
this header does not claim ZIP64 or encrypted-archive support.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-zip.h`](../include/mulib-zip.h) |
| Implementation switch | `MULIB_IMPL_ZIP` |
| Repository-local includes | [mulib-crc32](mulib-crc32.md), [mulib-deflate](mulib-deflate.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/zip.c`](../examples/modules/zip.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/zip.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
