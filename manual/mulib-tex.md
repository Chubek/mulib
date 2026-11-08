# tex

[Manuals](README.md) · [Library index](index.md)

`init` borrows validated UTF-8 source, rejecting NUL/control characters except
tab/CR/LF. `next` returns 1 token, 0 EOF, -1 invalid/trailing backslash; error
preserves cursor/output. Tokens carry kind and byte offset/length. Fixed lexical
rules recognize ASCII control words, escaped control symbols, braces, whitespace,
percent comments and ordinary text. No TeX execution, catcodes or macro expansion.

`escape` appends literal LaTeX text-mode source atomically to a strbuf, escaping
backslash/braces/#/$/%/&/_/^/~. It validates the same text profile, preserves
UTF-8 and whitespace, and does not perform typography or math rendering.
Switches: `MULIB_IMPL_TEX`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`.
Example: `examples/modules/tex.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-tex.h`](../include/mulib-tex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-tex.h
 * @brief TeX text escaping and a bounded lexical scanner, not a TeX engine.
 * Define MULIB_IMPL_TEX plus STRBUF and UTF8 switches. */
#ifndef MULIB_TEX_H
#define MULIB_TEX_H
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_TEX_TEXT = 1,
    MULIB_TEX_CONTROL = 2,
    MULIB_TEX_OPEN = 3,
    MULIB_TEX_CLOSE = 4,
    MULIB_TEX_COMMENT = 5,
    MULIB_TEX_SPACE = 6
  };
  typedef struct mulib_tex_token
  {
    int kind;
    size_t offset, length;
  } mulib_tex_token_t;
  /** @brief Cursor over borrowed UTF-8; initialize with init. No macro
   * expansion/catcodes. */
  typedef struct mulib_tex
  {
    const char *source;
    size_t length, position;
  } mulib_tex_t;
  /** @brief Validate UTF-8, reject NUL/control bytes except tab/CR/LF. 1
   * success. */
  int mulib_tex_init (mulib_tex_t *scanner, const char *source, size_t length);
  /** @brief Return 1 token, 0 EOF, -1 invalid/trailing backslash. Cursor
   * unchanged on error. ASCII letters form control words; other escaped bytes
   * form control symbols. Braces, percent comments and whitespace are separate
   * tokens. */
  int mulib_tex_next (mulib_tex_t *scanner, mulib_tex_token_t *out);
  /** @brief Append escaped literal UTF-8 for LaTeX text mode, atomically.
   * Escapes backslash, braces, # $ % & _ ^ ~; no math or raw-command
   * interpretation. */
  int mulib_tex_escape (mulib_strbuf_t *out, const char *text, size_t length);
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
| Header | [`include/mulib-tex.h`](../include/mulib-tex.h) |
| Implementation switch | `MULIB_IMPL_TEX` |
| Repository-local includes | [mulib-strbuf](mulib-strbuf.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/tex.c`](../examples/modules/tex.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/tex.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
