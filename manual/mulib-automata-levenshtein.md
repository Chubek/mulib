# Streaming Levenshtein recognizer

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_AUTOMATA_LEVENSHTEIN`.
[Example](../examples/modules/automata-levenshtein.c).

Creation copies the binary pattern and initializes the empty candidate state.
It returns NULL for invalid pointers, SIZE_MAX limit, dimension overflow or OOM.
`feed` consumes arbitrary byte chunks; chunk boundaries do not affect results.
It returns 0 on invalid input, without consuming it. `accepts` tests whether the
entire current candidate has edit distance <= limit. `reset` reuses storage.
Destroy the object once; NULL reset/destroy is harmless.

States are capped dynamic-programming rows, not a precompiled DFA graph.
O(pattern length) storage and per-byte work, no allocations during feed.
No Unicode normalization, transpositions, substring matching or tree traversal.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-automata-levenshtein.h`](../include/mulib-automata-levenshtein.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-automata-levenshtein.h
 * @brief Streaming Levenshtein recognizer using capped dynamic-programming states.
 * Define MULIB_IMPL_AUTOMATA_LEVENSHTEIN. Owns a copied binary pattern.
 */
#ifndef MULIB_AUTOMATA_LEVENSHTEIN_H
#define MULIB_AUTOMATA_LEVENSHTEIN_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_levenshtein_automaton mulib_levenshtein_automaton_t;
/** Create a recognizer accepting distance <= limit; SIZE_MAX limit is invalid. */
mulib_levenshtein_automaton_t *mulib_levenshtein_automaton_create(const void *pattern, size_t n, size_t limit);
/** Reset to the empty candidate without allocating. NULL is harmless. */
void mulib_levenshtein_automaton_reset(mulib_levenshtein_automaton_t *a);
/** Consume candidate bytes. Return 1, or 0 for invalid arguments. */
int mulib_levenshtein_automaton_feed(mulib_levenshtein_automaton_t *a, const void *bytes, size_t n);
/** Return whether the current candidate is accepted; NULL gives zero. */
int mulib_levenshtein_automaton_accepts(const mulib_levenshtein_automaton_t *a);
/** Release all storage. */
void mulib_levenshtein_automaton_destroy(mulib_levenshtein_automaton_t *a);
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
| Header | [`include/mulib-automata-levenshtein.h`](../include/mulib-automata-levenshtein.h) |
| Implementation switch | `MULIB_IMPL_AUTOMATA_LEVENSHTEIN` |
| Repository-local includes | None |
| Complete example | [`examples/modules/automata-levenshtein.c`](../examples/modules/automata-levenshtein.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/automata-levenshtein.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
