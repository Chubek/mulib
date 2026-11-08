# sstfile

[Manuals](README.md) · [Library index](index.md)

Versioned MuLIB sorted-table snapshots, not LevelDB/RocksDB files. `encode`
appends a complete table atomically. `decode` creates an owned memtable in an
uninitialized destination; outputs unchanged on failure. Returns 0 success,
negative invalid/corrupt/allocation errors. Empty tables are supported.

Wire: eight-byte `MuSST` version-1 magic, LE64 count, then each record's original
LE64 sequence and a WAL frame (ordinal sequence 1..count), then whole-file CRC32.
Decoded keys must be strictly increasing. Duplicate/unsorted keys, bad checksums,
truncation and trailing bytes fail. Values, tombstones and sequences roundtrip.
Decoded tables own their bytes; free with `mulib_memtable_free`.

Switches: `MULIB_IMPL_SSTFILE`, `MULIB_IMPL_MEMTABLE`, `MULIB_IMPL_WAL`,
`MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_CRC32`.
Example: `examples/modules/sstfile.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-sstfile.h`](../include/mulib-sstfile.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-sstfile.h
 * @brief Immutable sorted-table byte snapshots, preserving
 * tombstones/sequences. Define MULIB_IMPL_SSTFILE and MEMTABLE, WAL, BUFFER,
 * BYCUTILS, CRC32 switches.
 */
#ifndef MULIB_SSTFILE_H
#define MULIB_SSTFILE_H
#include "mulib-buffer.h"
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Encode to an initialized buffer; appends one snapshot atomically.
   */
  int mulib_sstfile_encode (const mulib_memtable_t *table,
                            mulib_buffer_t *out);
  /** @brief Decode exactly one snapshot to a zero/uninitialized owned table.
   * Reject duplicate/unsorted keys, bad checksums and trailing/incomplete
   * bytes. Outputs unchanged on failure. Returns 0, negative on
   * invalid/corrupt/OOM. */
  int mulib_sstfile_decode (const void *data, size_t size,
                            mulib_memtable_t *out);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sstfile.h`](../include/mulib-sstfile.h) |
| Implementation switch | `MULIB_IMPL_SSTFILE` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-bycutils](mulib-bycutils.md), [mulib-crc32](mulib-crc32.md), [mulib-memtable](mulib-memtable.md), [mulib-wal](mulib-wal.md) |
| Complete example | [`examples/modules/sstfile.c`](../examples/modules/sstfile.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sstfile.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
