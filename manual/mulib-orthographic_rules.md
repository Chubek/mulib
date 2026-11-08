# Literal orthographic rules

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_ORTHOGRAPHIC_RULES`.
[Example](../examples/modules/orthographic_rules.c).

Rules borrow NUL-terminated match/replacement strings. Apply validates all
rules, then substitutes only the first matching prefix, suffix or whole word.
`minimum_stem` is the number of bytes left after removing the match; whole-word
rules normally require zero. Empty matches/invalid kinds are rejected. Empty
replacement and empty input word are valid. If no rule matches, return a copy.

Result is freshly malloc-owned; caller frees it. NULL indicates invalid input,
size overflow or allocation failure. No caller state is modified. Matching is
case-sensitive bytes, not a regex, Unicode normalizer or built-in language
stemmer. Rules never recurse, avoiding rewrite loops.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-orthographic_rules.h`](../include/mulib-orthographic_rules.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-orthographic_rules.h
 * @brief Ordered literal prefix/suffix/whole-word orthographic rules.
 * Define MULIB_IMPL_ORTHOGRAPHIC_RULES. Byte-based, case-sensitive, no linguistics inferred.
 */
#ifndef MULIB_ORTHOGRAPHIC_RULES_H
#define MULIB_ORTHOGRAPHIC_RULES_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_orthographic_kind { MULIB_ORTHOGRAPHIC_PREFIX, MULIB_ORTHOGRAPHIC_SUFFIX, MULIB_ORTHOGRAPHIC_WORD } mulib_orthographic_kind_t;
typedef struct mulib_orthographic_rule {
  mulib_orthographic_kind_t kind;
  const char *match, *replacement;
  size_t minimum_stem;
} mulib_orthographic_rule_t;
/** Apply the first matching rule once; otherwise copy word. Caller frees result.
 * NULL on invalid rule/OOM/overflow. Empty matches are rejected. No recursive rules. */
char *mulib_orthographic_apply(const char *word, const mulib_orthographic_rule_t *rules, size_t n);
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
| Header | [`include/mulib-orthographic_rules.h`](../include/mulib-orthographic_rules.h) |
| Implementation switch | `MULIB_IMPL_ORTHOGRAPHIC_RULES` |
| Repository-local includes | None |
| Complete example | [`examples/modules/orthographic_rules.c`](../examples/modules/orthographic_rules.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/orthographic_rules.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
