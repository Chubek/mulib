# Byte trie

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_TRIE`. [Example](../examples/modules/trie.c).

Zero-initialize or call init on uninitialized storage. The trie owns nodes and
copied binary keys, including empty keys; uint64 values are copied. NULL key
requires zero length. No terminator is appended to stored keys. Nodes exposed
in the struct are read-only implementation data for traversal adapters.

Set returns 1 for insertion/replacement, 0 on invalid input/overflow/OOM;
allocation failure preserves existing mappings. Get optionally copies a found
value; missing keys leave output untouched. Remove returns whether found and
prunes unused suffix nodes without allocating. Free is iterative and resets
the object. Do not shallow-copy live tries. Sorted sibling edges use at most
256 scans per byte. Keys consume O(length) nodes plus one full terminal copy;
there is no compression. Pair with [traversal](mulib-trie_traversal.md).

`mulib_trie_approximate` traverses with Levenshtein DP rows and prunes subtrees
whose row minimum exceeds the radius. Query/key units are bytes, including NUL.
Callback receives borrowed key/length, stored value and distance; nonzero
continues. Return callback count including stop, SIZE_MAX on invalid/OOM/limit.
Callbacks may have run before a later failure and must not mutate the trie.
Optional work counts DP cells/edges and is initialized on every return. Zero
budget means 10000000; query and visited key depth cap at 4096, live DP rows at
64 MiB. Errors distinguish allocation through errno=ENOMEM (invalid/limits use
EINVAL/EOVERFLOW). No recursion or per-candidate whole-word distance scan is
used. [Spellcheck](mulib-spellcheck.md) wraps it with owned ranked results.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-trie.h`](../include/mulib-trie.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-trie.h
 * @brief Owned binary-key byte trie with sorted sibling edges and uint64 values.
 * Define MULIB_IMPL_TRIE. Zero-initialize objects; nodes are read-only to clients.
 */
#ifndef MULIB_TRIE_H
#define MULIB_TRIE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_trie_node {
  struct mulib_trie_node *parent, *child, *next;
  unsigned char edge, *key;
  size_t length;
  uint64_t value;
  int terminal;
} mulib_trie_node_t;
typedef struct mulib_trie { mulib_trie_node_t *root; size_t size; } mulib_trie_t;
/** Initialize an uninitialized object (do not use on a live trie). */
void mulib_trie_init(mulib_trie_t *trie);
/** Copy key and insert/replace. NULL key requires zero length.
 * Return 1 on success; failure preserves all mappings. */
int mulib_trie_set(mulib_trie_t *trie, const void *key, size_t length, uint64_t value);
/** Return whether found, optionally copying the value. */
int mulib_trie_get(const mulib_trie_t *trie, const void *key, size_t length, uint64_t *value);
/** Remove and prune without allocation; return whether found. */
int mulib_trie_remove(mulib_trie_t *trie, const void *key, size_t length);
/** Free iteratively and reset. NULL is harmless. */
void mulib_trie_free(mulib_trie_t *trie);
typedef int (*mulib_trie_match_fn)(const void *key, size_t length, uint64_t value,
    size_t distance, void *user);
/** DP-row traversal with subtree pruning, byte Levenshtein radius. Callback
 * borrows key, nonzero continues; return count including stop, SIZE_MAX on
 * invalid/OOM/work limit (callbacks may already have run). Query/key traversal
 * depth <=4096; live DP storage <=64 MiB. max_work=0 means 10000000 cells/edges.
 * Optional work is always initialized; callbacks must not mutate the trie. */
size_t mulib_trie_approximate(const mulib_trie_t *, const void *, size_t, size_t radius,
    size_t max_work, mulib_trie_match_fn, void *, size_t *work);
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
| Header | [`include/mulib-trie.h`](../include/mulib-trie.h) |
| Implementation switch | `MULIB_IMPL_TRIE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/trie.c`](../examples/modules/trie.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/trie.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
