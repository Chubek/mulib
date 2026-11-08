# latex

[Manuals](README.md) · [Library index](index.md)

Zero initialize an owning document. `begin` writes an article preamble with
escaped title; `section` supports levels 1..3; `paragraph` appends escaped text.
`finish` appends end-document idempotently and rejects further content. All return
1 success, 0 invalid/state/allocation failure, without partial append. Borrow
`source.data`/`source.length` until mutation; `free` releases/reset.

This is source construction, not a TeX parser/compiler/renderer. No external
command, shell escape, package loading, custom raw commands or PDF generation.
Switches: `MULIB_IMPL_LATEX`, `MULIB_IMPL_TEX`, `MULIB_IMPL_STRBUF`,
`MULIB_IMPL_UTF8`. Example: `examples/modules/latex.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-latex.h`](../include/mulib-latex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-latex.h
 * @brief Escaped LaTeX article generation with an explicit document lifecycle.
 * Define MULIB_IMPL_LATEX plus TEX, STRBUF and UTF8. No external TeX process.
 */
#ifndef MULIB_LATEX_H
#define MULIB_LATEX_H
#include "mulib-tex.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Zero initialize. Owns source; fields are read-only to clients. */
  typedef struct mulib_latex
  {
    mulib_strbuf_t source;
    int state;
  } mulib_latex_t;
  /** @brief Begin article with escaped title; 1 success. Inactive document
   * only. */
  int mulib_latex_begin (mulib_latex_t *document, const char *title);
  /** @brief Append escaped section heading (level 1..3); 1 success. */
  int mulib_latex_section (mulib_latex_t *document, unsigned level,
                           const char *title);
  /** @brief Append escaped paragraph plus blank line; active document only. */
  int mulib_latex_paragraph (mulib_latex_t *document, const char *text,
                             size_t length);
  /** @brief End once, idempotent; no further content may be appended. */
  int mulib_latex_finish (mulib_latex_t *document);
  /** @brief Release source/reset; accepting NULL. */
  void mulib_latex_free (mulib_latex_t *document);
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
| Header | [`include/mulib-latex.h`](../include/mulib-latex.h) |
| Implementation switch | `MULIB_IMPL_LATEX` |
| Repository-local includes | [mulib-tex](mulib-tex.md) |
| Complete example | [`examples/modules/latex.c`](../examples/modules/latex.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/latex.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
