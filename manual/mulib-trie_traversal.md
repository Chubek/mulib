# Trie traversal

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_TRIE_TRAVERSAL` and, when constructing/freeing tries,
`MULIB_IMPL_TRIE`. [Example](../examples/modules/trie_traversal.c).

`mulib_trie_traverse` visits prefix matches in unsigned-byte lexicographic order,
including the exact prefix if stored. Callback keys borrow the trie, are binary
and not NUL-terminated, and must not be modified. Callback must not mutate/free
the trie. Nonzero continues, zero stops. Return counts include the stopping
call; SIZE_MAX means invalid input. Empty prefix enumerates every key.

Longest-prefix lookup includes a stored empty key, returns 1 when found and
optionally fills matched length and value. Outputs are untouched on absence.
NULL key/prefix requires zero length. No allocation or recursive stack growth;
traversal visits the selected subtree, prefix lookup follows input bytes.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-trie_traversal.h`](../include/mulib-trie_traversal.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-trie_traversal.h
 * @brief Allocation-free lexicographic prefix traversal and longest-prefix lookup.
 * Define MULIB_IMPL_TRIE_TRAVERSAL; nodes/keys are borrowed from mulib-trie.
 */
#ifndef MULIB_TRIE_TRAVERSAL_H
#define MULIB_TRIE_TRAVERSAL_H
#include "mulib-trie.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_trie_visit_fn)(const void *key, size_t length, uint64_t value, void *context);
/** Visit prefix matches in unsigned-byte order. Nonzero continues; no mutation
 * from callbacks. Return calls including stop, SIZE_MAX for invalid arguments. */
size_t mulib_trie_traverse(const mulib_trie_t *trie, const void *prefix, size_t length,
                          mulib_trie_visit_fn visit, void *context);
/** Find longest stored prefix of key. Return 1 if found, optional outputs.
 * Includes a stored empty key. Outputs are untouched when not found. */
int mulib_trie_longest_prefix(const mulib_trie_t *trie, const void *key, size_t length,
                             size_t *matched_length, uint64_t *value);
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
| Header | [`include/mulib-trie_traversal.h`](../include/mulib-trie_traversal.h) |
| Implementation switch | `MULIB_IMPL_TRIE_TRAVERSAL` |
| Repository-local includes | [mulib-trie](mulib-trie.md) |
| Complete example | [`examples/modules/trie_traversal.c`](../examples/modules/trie_traversal.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/trie_traversal.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
