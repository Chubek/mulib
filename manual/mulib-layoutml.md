# layoutml

[Manuals](README.md) · [Library index](index.md)

Strict XML profile:

```xml
<layout direction="row" gap="4"><item weight="1"/><item weight="3"/></layout>
```

Direction is row/column, optional gap defaults zero. Numbers are unsigned decimal
integers 0..1000000; <=4096 empty item elements, each with exactly weight. Unknown
attributes/elements, nesting, comments and non-whitespace text are rejected.
XML syntax uses the existing bounded XML parser, with no DTD/external entities.
`parse` computes rectangles for a caller-supplied parent; 1 success, 0 error,
optional static diagnostic. Output is malloc-owned; caller frees it. Empty input
layout succeeds with NULL array/count zero; other outputs unchanged on failure.

Switches: `MULIB_IMPL_LAYOUTML`, `MULIB_IMPL_LAYOUT`, `MULIB_IMPL_XML`,
`MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`. Example: `examples/modules/layoutml.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-layoutml.h`](../include/mulib-layoutml.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-layoutml.h
 * @brief Strict XML layout profile: flat weighted items in a row/column.
 * Define MULIB_IMPL_LAYOUTML plus XML, UTF8, STRBUF and LAYOUT. */
#ifndef MULIB_LAYOUTML_H
#define MULIB_LAYOUTML_H
#include "mulib-layout.h"
#include "mulib-xml.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Parse <layout direction="row|column" gap="N"><item
   * weight="N"/>...</layout>. Unsigned decimal integers only (0..1000000), at
   * most 4096 items. Reject unknown elements/attributes/non-whitespace
   * content. Returns malloc-owned rectangles; empty layout returns NULL with
   * count=0 and success=1. Outputs unchanged on failure. error receives static
   * diagnostic. Caller frees rectangles. */
  int mulib_layoutml_parse (const char *xml, size_t length,
                            mulib_layout_rect_t parent,
                            mulib_layout_rect_t **out, size_t *count,
                            mulib_xml_error_t *error);
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
| Header | [`include/mulib-layoutml.h`](../include/mulib-layoutml.h) |
| Implementation switch | `MULIB_IMPL_LAYOUTML` |
| Repository-local includes | [mulib-layout](mulib-layout.md), [mulib-xml](mulib-xml.md) |
| Complete example | [`examples/modules/layoutml.c`](../examples/modules/layoutml.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/layoutml.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
