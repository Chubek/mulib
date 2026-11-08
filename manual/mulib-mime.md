# MIME lookup

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_MIME` and `SECURITY`. Case-insensitive filename extensions
select static MIME strings. Unknown/missing extensions return
application/octet-stream. No content sniffing or allocation.
[Example](../examples/modules/mime.c), [contracts](storage-server.md).

## Lookup contract

`mulib_mime_type(filename)` returns a static immutable string; never free it.
It compares the final filename extension case-insensitively, resetting extension
tracking at either `/` or `\`. NULL, missing extensions, and unknown extensions
return `application/octet-stream`. There is no allocation or content inspection.

The built-in table covers html/htm, css, js, json, txt, svg, png, jpg/jpeg, gif,
ico, pdf, wasm, and xml. HTML, CSS, JavaScript, and plain-text results include
`charset=utf-8`; the function does not validate that the file actually uses UTF-8.
Pass a filename rather than a URL with a query or fragment.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-mime.h`](../include/mulib-mime.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-mime.h
 * @brief Case-insensitive filename-extension MIME lookup; no content sniffing.
 * Define MULIB_IMPL_MIME and SECURITY. Returned strings have static lifetime.
 */
#ifndef MULIB_MIME_H
#define MULIB_MIME_H
#include "mulib-seurity.h"
#ifdef __cplusplus
extern "C"
{
#endif
  const char *mulib_mime_type (const char *filename);
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
| Header | [`include/mulib-mime.h`](../include/mulib-mime.h) |
| Implementation switch | `MULIB_IMPL_MIME` |
| Repository-local includes | [mulib-seurity](mulib-seurity.md) |
| Complete example | [`examples/modules/mime.c`](../examples/modules/mime.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mime.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
