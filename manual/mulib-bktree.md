# mulib-bktree

A canonical API for MuLIB's existing owned BK-tree implementation in
`mulib-bk-trees.h`. Define `MULIB_IMPL_BKTREE`, `MULIB_IMPL_BK_TREES` and
`MULIB_IMPL_LEVENSHTEIN` in one C11 translation unit. No second tree or competing
distance implementation is maintained.

Zero-initialize `mulib_bktree_t`; `insert` copies NUL-terminated byte words,
ignoring duplicates. The first word is the root; each child edge stores its
unit Levenshtein distance from its parent. Insertion follows the edge with the
same distance until an empty slot is reached. On invalid/OOM, mappings remain
unchanged. Empty strings are legal. No Unicode normalization is inferred.

`search(tree, query, radius, callback, user)` computes node/query distance d and
only visits children with edge distance in `[d-radius,d+radius]`. Subtraction
forms avoid unsigned overflow. This is metric pruning, not a full dictionary
scan. The callback borrows each matching word and distance; nonzero continues,
zero stops. Order is unspecified. Return the number of callbacks including a
stopping callback, or SIZE_MAX on error. Allocation failure can follow earlier
callbacks. Callbacks must not modify the tree.

`insert_bounded` and `search_bounded` additionally bound cumulative edit-DP
cells; zero selects 10000000. Optional work is initialized on every return.
Tree insertion remains transactional; search callbacks are not rolled back.
`free` iteratively releases all nodes/words, accepts NULL, and resets the tree.

The metric is **unit byte Levenshtein**, not arbitrary weighted edits or Unicode
grapheme distance. Weighted/nonmetric pruning can miss results and is not
offered. Trees can degenerate with adversarial insertion order; there is no
balance/minimization guarantee. Spellcheck supplies deterministic ranking and
owned transactional result lists around this callback API.

See [`examples/modules/bktree.c`](../examples/modules/bktree.c).
