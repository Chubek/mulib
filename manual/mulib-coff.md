# COFF reader

[Manuals](README.md) · [Library index](index.md)

Include `mulib-coff.h`; define `MULIB_IMPL_COFF` and `MULIB_IMPL_ENDIAN` in one TU.
See [example](../examples/modules/coff.c) and [shared contracts](binary-formats.md).

`mulib_coff_open(&view, data, size)` borrows Microsoft little-endian COFF bytes.
`open_at` supports a COFF header embedded in PE; section/symbol offsets are still
relative to the original file, not the embedded header. The optional-header span
is bounded but opaque. COFF has no universal magic: a nonzero machine field is
accepted, so this function is not a reliable arbitrary-file format detector.

`section`, `section_name`, `symbol`, `symbol_name`, and `bytes` return decoded
records or borrowed bounded spans. Fixed eight-byte names need not end in NUL;
always use the returned length. `/decimal` section names and zero-prefix symbol
names resolve against the length-prefixed string table, with termination checked.
`symbol_count` counts raw 18-byte entries including auxiliaries. Iterate primary
symbols by `i += 1 + symbol.auxiliary_count`; accessing an auxiliary index fails.
Section indexes preserve signed COFF values (0 undefined, -1 absolute, -2 debug).

Open validates table and section payload/relocation/line-number ranges and auxiliary
counts. It does not apply relocations or decode auxiliaries/line records. BSS can
have a nonzero memory size with no file payload. Name strings and symbol section
indexes are validated on access. Archives, bigobj, import objects and relocation
overflow extensions are unsupported. Views must remain unmodified.

Results: `OK=0`, `EINVAL=-1`, `EFORMAT=-2`, `EBOUNDS=-3`, `EUNSUPPORTED=-4`,
`ENOTFOUND=-5`. There is no allocation, hidden error state or file I/O.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-coff.h`](../include/mulib-coff.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-coff.h
 * @brief Checked little-endian Microsoft COFF sections, symbols and names.
 * Define MULIB_IMPL_COFF and MULIB_IMPL_ENDIAN. Not a loader; no bigobj,
 * archives, import objects or relocation-overflow records. */
#ifndef MULIB_COFF_H
#define MULIB_COFF_H
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C" {
#endif
enum mulib_coff_result {
  MULIB_COFF_OK = 0, MULIB_COFF_EINVAL = -1, MULIB_COFF_EFORMAT = -2,
  MULIB_COFF_EBOUNDS = -3, MULIB_COFF_EUNSUPPORTED = -4,
  MULIB_COFF_ENOTFOUND = -5
};
/** @brief Borrowed validated tables; initialize with open, do not modify. */
typedef struct mulib_coff {
  const unsigned char *data;
  size_t size, header_offset, section_offset, section_count;
  size_t symbol_offset, symbol_count, string_offset, string_size;
  uint16_t machine, characteristics, optional_size;
  uint32_t timestamp;
} mulib_coff_t;
/** @brief Raw file size/offset differ from virtual size/address in PE images. */
typedef struct mulib_coff_section {
  uint32_t virtual_size, address, size, offset, relocation_offset;
  uint32_t line_offset, flags;
  uint16_t relocation_count, line_count;
} mulib_coff_section_t;
/** @brief Primary 18-byte symbol. Auxiliary entries are not symbols. */
typedef struct mulib_coff_symbol {
  uint32_t value;
  int32_t section_index;
  uint16_t type;
  unsigned storage_class, auxiliary_count;
} mulib_coff_symbol_t;
/** @brief Open a COFF file at offset zero; outputs unchanged on failure. */
int mulib_coff_open (mulib_coff_t *out, const void *data, size_t size);
/** @brief Open a COFF header inside a PE file; all offsets remain file-relative. */
int mulib_coff_open_at (mulib_coff_t *out, const void *data, size_t size, size_t offset);
/** @brief Decode a zero-based section and validate payload/relocation ranges. */
int mulib_coff_section (const mulib_coff_t *v, size_t index, mulib_coff_section_t *out);
/** @brief Borrow a section name (fixed eight bytes or /decimal string offset). */
int mulib_coff_section_name (const mulib_coff_t *v, size_t index, const char **name, size_t *length);
/** @brief Decode a primary symbol by raw table index; rejects auxiliary indexes. */
int mulib_coff_symbol (const mulib_coff_t *v, size_t index, mulib_coff_symbol_t *out);
/** @brief Borrow a primary symbol's short or string-table name. */
int mulib_coff_symbol_name (const mulib_coff_t *v, size_t index, const char **name, size_t *length);
/** @brief Borrow a checked file range, allowing empty one-past-end. */
int mulib_coff_bytes (const mulib_coff_t *v, uint64_t offset, uint64_t size, const unsigned char **out);
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
| Header | [`include/mulib-coff.h`](../include/mulib-coff.h) |
| Implementation switch | `MULIB_IMPL_COFF` |
| Repository-local includes | [mulib-endian](mulib-endian.md) |
| Complete example | [`examples/modules/coff.c`](../examples/modules/coff.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/coff.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
