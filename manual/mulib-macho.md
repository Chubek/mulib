# Mach-O reader

[Manuals](README.md) · [Library index](index.md)

Include `mulib-macho.h`; define `MULIB_IMPL_MACHO` and `MULIB_IMPL_ENDIAN`.
[Example](../examples/modules/macho.c), [shared contracts](binary-formats.md).

`mulib_macho_open` borrows a thin Mach-O32/Mach-O64 file in either byte order.
It checks the header, exact command-table consumption/alignment, segment payload
ranges, section/relocation ranges and LC_SYMTAB table spans. LC_SEGMENT versus
LC_SEGMENT_64 must match the file class; duplicate symbol-table commands fail.
Unknown load commands remain bounded opaque bytes rather than being interpreted.

`command` returns a type, size and borrowed command span. `section` uses a
zero-based index flattened across segment commands and copies names into
NUL-terminated 17-byte arrays. `section_name` instead borrows up to sixteen bytes.
Alignment is the native base-two exponent. Zero-fill section types 1, 12 and 18
have virtual sizes without file payload. `bytes` always checks actual file bounds.

`symbol` decodes nlist/nlist_64, preserving type, section ordinal, description
and value. `symbol_name` checks the string index and terminating NUL on access.
Symbol section ordinals are native one-based values, not the section API's index.
Debug and undefined symbols remain visible to dump tools; lookup filters them.

Results are `OK=0`, `EINVAL=-1`, `EFORMAT=-2`, `EBOUNDS=-3`,
`EUNSUPPORTED=-4`, `ENOTFOUND=-5`. No allocation or I/O. Fat/universal files
are rejected; select a thin slice externally. This is not a dyld loader, relocation
engine, codesign verifier or parser of every command's private payload.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-macho.h`](../include/mulib-macho.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-macho.h
 * @brief Checked thin Mach-O 32/64-bit views in either byte order.
 * Define MULIB_IMPL_MACHO and ENDIAN. No fat files, loading, dyld fixups or
 * signature verification. Unknown load commands remain bounded opaque records. */
#ifndef MULIB_MACHO_H
#define MULIB_MACHO_H
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C" {
#endif
enum mulib_macho_result {
  MULIB_MACHO_OK = 0, MULIB_MACHO_EINVAL = -1, MULIB_MACHO_EFORMAT = -2,
  MULIB_MACHO_EBOUNDS = -3, MULIB_MACHO_EUNSUPPORTED = -4, MULIB_MACHO_ENOTFOUND = -5
};
/** @brief Borrowed validated view; do not modify fields or backing bytes. */
typedef struct mulib_macho {
  const unsigned char *data;
  size_t size, header_size, command_count, command_size, section_count;
  size_t symbol_offset, symbol_count, string_offset, string_size;
  unsigned bits, little_endian;
  uint32_t cpu, subtype, type, flags;
} mulib_macho_t;
/** @brief Borrowed bounded command bytes, including the eight-byte header. */
typedef struct mulib_macho_command {
  uint32_t type, size;
  const unsigned char *data;
} mulib_macho_command_t;
/** @brief Names are copied and NUL-terminated; size is virtual for zero-fill. */
typedef struct mulib_macho_section {
  char name[17], segment[17];
  uint64_t address, size;
  uint32_t offset, alignment, relocation_offset, relocation_count, flags;
} mulib_macho_section_t;
typedef struct mulib_macho_symbol {
  uint32_t name;
  unsigned type, section_index;
  uint16_t description;
  uint64_t value;
} mulib_macho_symbol_t;
/** @brief Validate command structure, segments, sections and symbol/string ranges. */
int mulib_macho_open (mulib_macho_t *out, const void *data, size_t size);
/** @brief Decode a zero-based load command. */
int mulib_macho_command (const mulib_macho_t *v, size_t index, mulib_macho_command_t *out);
/** @brief Decode a zero-based section across all segment commands. */
int mulib_macho_section (const mulib_macho_t *v, size_t index, mulib_macho_section_t *out);
/** @brief Borrow the section name; length may be sixteen, no terminating NUL required. */
int mulib_macho_section_name (const mulib_macho_t *v, size_t index, const char **out, size_t *length);
/** @brief Decode a zero-based nlist/nlist_64 symbol. */
int mulib_macho_symbol (const mulib_macho_t *v, size_t index, mulib_macho_symbol_t *out);
/** @brief Borrow a bounded NUL-terminated symbol name. */
int mulib_macho_symbol_name (const mulib_macho_t *v, size_t index, const char **out, size_t *length);
/** @brief Borrow checked file bytes; zero length at EOF is valid. */
int mulib_macho_bytes (const mulib_macho_t *v, uint64_t offset, uint64_t size, const unsigned char **out);
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
| Header | [`include/mulib-macho.h`](../include/mulib-macho.h) |
| Implementation switch | `MULIB_IMPL_MACHO` |
| Repository-local includes | [mulib-endian](mulib-endian.md) |
| Complete example | [`examples/modules/macho.c`](../examples/modules/macho.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/macho.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
