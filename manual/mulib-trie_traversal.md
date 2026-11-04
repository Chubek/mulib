# Trie traversal

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
