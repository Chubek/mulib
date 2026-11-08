# wal

[Manuals](README.md) · [Library index](index.md)

Write-ahead-log **byte codec**, with no file or durability operations. `append`
adds a complete record atomically to an initialized buffer. Lengths are uint32;
nonzero sequences must increase when read. Tombstones require empty values.
Reader borrows input and starts zeroed with data/size assigned.

Wire record: `MWAL`, LE32 key/value lengths, LE64 sequence, deletion byte,
four reserved zero bytes, key, value, LE32 CRC32 of preceding frame bytes.
`next` returns RECORD and advances; OK is clean EOF; TORN is an incomplete final
frame; negative errors mean invalid/corrupt data. Output/cursor change only on a
complete record. Callers decide whether to truncate a torn tail; never skip CRC
errors. Persistence, fsync, serialization of concurrent writers and recovery
publication are caller policy. Append inputs must not alias log storage.

Switches: `MULIB_IMPL_WAL`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`,
`MULIB_IMPL_CRC32`. Example: `examples/modules/wal.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-wal.h`](../include/mulib-wal.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-wal.h
 * @brief CRC-framed, sequence-checked write-ahead-log byte codec, not disk
 * I/O. Define MULIB_IMPL_WAL plus BUFFER, BYCUTILS and CRC32 dependency
 * switches.
 */
#ifndef MULIB_WAL_H
#define MULIB_WAL_H
#include "mulib-buffer.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_WAL_OK = 0,
    MULIB_WAL_RECORD = 1,
    MULIB_WAL_TORN = 2,
    MULIB_WAL_EINVAL = -1,
    MULIB_WAL_ECORRUPT = -2,
    MULIB_WAL_ENOMEM = -3
  };
  typedef struct mulib_wal_record
  {
    const unsigned char *key, *value;
    size_t key_size, value_size;
    uint64_t sequence;
    int deleted;
  } mulib_wal_record_t;
  /** @brief Borrowed input. Initialize all fields to zero, then set data/size.
   * last_sequence enforces strictly increasing nonzero sequences. */
  typedef struct mulib_wal_reader
  {
    const unsigned char *data;
    size_t size, offset;
    uint64_t last_sequence;
  } mulib_wal_reader_t;
  /** @brief Append a complete frame atomically. Caller enforces append
   * sequence. Records use 32-bit lengths; input must not alias log storage. */
  int mulib_wal_append (mulib_buffer_t *log, const void *key, size_t kn,
                        const void *value, size_t vn, uint64_t sequence,
                        int deleted);
  /** @brief RECORD advances; OK means clean EOF. TORN means incomplete final
   * frame. Other errors mean corruption. Cursor/output unchanged except on
   * RECORD. Never skip a corrupt frame; persistence, fsync and tail truncation
   * are caller policy. */
  int mulib_wal_next (mulib_wal_reader_t *reader, mulib_wal_record_t *out);
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
| Header | [`include/mulib-wal.h`](../include/mulib-wal.h) |
| Implementation switch | `MULIB_IMPL_WAL` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-bycutils](mulib-bycutils.md), [mulib-crc32](mulib-crc32.md) |
| Complete example | [`examples/modules/wal.c`](../examples/modules/wal.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/wal.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
