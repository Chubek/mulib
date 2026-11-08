# binfile

[Manuals](README.md) · [Library index](index.md)

Borrowed binary span, no file opening/reading or ownership transfer. `open`
accepts NULL only for empty bytes. `slice` checks subtraction-based bounds and
can return a one-past-end zero-length span; output unchanged on error. Both
return 1 success, 0 invalid. Source must outlive every slice.

`format` recognizes ELF/ar magic only, returning UNKNOWN otherwise. It does not
certify structural validity. Use ELF/ar readers for actual validation.
Switch: `MULIB_IMPL_BINFILE`. Example: `examples/modules/binfile.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-binfile.h`](../include/mulib-binfile.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-binfile.h
 * @brief Borrowed binary file spans and format identification, without I/O.
 * Define MULIB_IMPL_BINFILE once. */
#ifndef MULIB_BINFILE_H
#define MULIB_BINFILE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_binfile
  {
    const unsigned char *data;
    size_t size;
  } mulib_binfile_t;
  enum
  {
    MULIB_BINFILE_UNKNOWN = 0,
    MULIB_BINFILE_ELF = 1,
    MULIB_BINFILE_AR = 2
  };
  /** @brief Open borrowed bytes; NULL allowed only for empty span. 1 success.
   */
  int mulib_binfile_open (mulib_binfile_t *out, const void *data, size_t size);
  /** @brief Slice with subtraction-based bounds checking; output unchanged on
   * error. */
  int mulib_binfile_slice (const mulib_binfile_t *file, size_t offset,
                           size_t size, mulib_binfile_t *out);
  /** @brief Identify magic only, not structural validity. */
  int mulib_binfile_format (const mulib_binfile_t *file);
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
| Header | [`include/mulib-binfile.h`](../include/mulib-binfile.h) |
| Implementation switch | `MULIB_IMPL_BINFILE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/binfile.c`](../examples/modules/binfile.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/binfile.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
