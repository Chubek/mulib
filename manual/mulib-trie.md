# Byte trie

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
