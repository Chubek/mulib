# objfilelibsearch

[Manuals](README.md) · [Library index](index.md)

Checked short-name Unix ar member lookup. `find` validates all member header
trailers, decimal lengths, name-field termination, payload/padding bounds before
publishing the first matching borrowed span. Returns 0 found, 1 absent, -1
invalid/malformed/unsupported; output unchanged on failure. Input must outlive
the returned span and must not be modified during use.

No filesystem library search, symbol index, thin archives, special members or
long-name extensions. Pair with object parser to inspect member contents.
Switches: `MULIB_IMPL_OBJFILELIBSEARCH`, `MULIB_IMPL_BINFILE`.
Example: `examples/modules/objfilelibsearch.c` also uses the ar writer.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-objfilelibsearch.h`](../include/mulib-objfilelibsearch.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-objfilelibsearch.h
 * @brief Checked short-name Unix ar lookup over borrowed bytes.
 * Define MULIB_IMPL_OBJFILELIBSEARCH once. Long-name and thin archives
 * rejected. */
#ifndef MULIB_OBJFILELIBSEARCH_H
#define MULIB_OBJFILELIBSEARCH_H
#include "mulib-binfile.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Validate every member before returning first matching member. 0
   * found, 1 absent, -1 malformed/unsupported/invalid. Output unchanged on
   * failure. */
  int mulib_objfilelibsearch_find (const void *archive, size_t size,
                                   const char *name, mulib_binfile_t *out);
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
| Header | [`include/mulib-objfilelibsearch.h`](../include/mulib-objfilelibsearch.h) |
| Implementation switch | `MULIB_IMPL_OBJFILELIBSEARCH` |
| Repository-local includes | [mulib-binfile](mulib-binfile.md) |
| Complete example | [`examples/modules/objfilelibsearch.c`](../examples/modules/objfilelibsearch.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/objfilelibsearch.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
