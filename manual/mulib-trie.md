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
