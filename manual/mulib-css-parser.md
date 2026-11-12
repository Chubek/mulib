# css-parser

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Owned bounded CSS stylesheet rules with selector/declaration spans.

## Contract and supported profile

The owned result type is `mulib_css_parsed_sheet_t`. It is deliberately distinct
from the existing opaque `mulib_css_sheet_t` used by `mulib-css.h`.

Parse copies a stylesheet of flat selector { declaration-bytes } rules,
stripping block comments outside quoted strings. Quoted escapes and parenthesis
balance are respected. Input<=32 MiB, selector<=255 bytes, positive rule limit;
empty input succeeds. Unterminated syntax, nested braces, at-rules, empty
selectors and trailing garbage fail without changing output. Declaration
semantics and supported simple-selector validation happen in computed/cascade.
On success output must be fresh/released; sheet_free recursively releases owned
declarations. No imports, nesting, media queries or full CSS grammar.

## Integration and ownership

Include `include/mulib-css-parser.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_CSS_PARSER`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/css-parser.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_css.c`](../tests/test_web_css.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-css-parser.h`](../include/mulib-css-parser.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-css-parser.h
 * @brief Owned bounded CSS stylesheet rules with selector/declaration spans.
 * C11 single-header module. Define MULIB_IMPL_CSS_PARSER in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_CSS_PARSER_H
#define MULIB_CSS_PARSER_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_css_rule
  {
    char selector[256];
    mulib_shylock_buffer_t declarations;
  } mulib_css_rule_t;
  typedef struct mulib_css_parsed_sheet
  {
    mulib_css_rule_t *rules;
    size_t len, cap;
  } mulib_css_parsed_sheet_t;
  /** @brief See manual/mulib-css-parser.md for this operation's contract. */
  int mulib_css_parse_sheet (const char *source, size_t n, size_t max_rules,
                             mulib_css_parsed_sheet_t *out);
  /** @brief See manual/mulib-css-parser.md for this operation's contract. */
  void mulib_css_sheet_free (mulib_css_parsed_sheet_t *sheet);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_PARSER_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-css-parser.h`](../include/mulib-css-parser.h) |
| Implementation switch | `MULIB_IMPL_CSS_PARSER` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/css-parser.c`](../examples/modules/css-parser.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/css-parser.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
