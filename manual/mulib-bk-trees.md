# BK spelling tree

Define `MULIB_IMPL_BK_TREES` and `MULIB_IMPL_LEVENSHTEIN`.
[Example](../examples/modules/bk-trees.c).

Zero-initialize or init uninitialized storage. Insert copies NUL-terminated
byte strings and ignores duplicates. Invalid/OOM fails without mutation;
successful insert returns 1. Empty string is valid. Free iteratively resets
the tree. Do not shallow-copy live trees.

Search applies Levenshtein triangle-inequality pruning on child edge distances.
Callback borrows each word and distance, must not mutate tree; nonzero continues.
Return counts include stopping call; SIZE_MAX means invalid/OOM (possibly after
earlier callbacks if a distance workspace allocation fails). Order unspecified.
Query allocates O(tree size) pointers plus distance workspace. Tree is unbalanced
and may degrade to linear visits, each with quadratic edit-distance work.
No Unicode normalization or weighted/nonmetric distance substitution is allowed.
