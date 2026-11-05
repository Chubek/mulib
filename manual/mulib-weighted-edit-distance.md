# mulib-weighted-edit-distance

Canonical weighted-edit interface, reusing the existing typo-named
`mulib-weighed_edit_distane.h` rather than defining a conflicting second
`mulib_weighted_edit_distance` symbol. Define
`MULIB_IMPL_WEIGHTED_EDIT_DISTANCE` and `MULIB_IMPL_WEIGHED_EDIT_DISTANE` in one
C11 translation unit; link libm.

The inherited `mulib_weighted_edit_distance(a,n,b,m,insertion,deletion,substitution)`
compares byte spans with constant directional costs. It returns NAN on
invalid/OOM and infinity on overflow. Matching bytes cost zero. Embedded NULs
are allowed because lengths are explicit. See its original manual for details.

The new `mulib_weighted_edit_distance_ex` takes **uint32 token arrays**, a pure
cost callback, user data, a cell budget and a required output pointer. The
callback receives INSERT/DELETE/SUBSTITUTE with source/target symbols (unused
symbols are zero). It must return a finite nonnegative cost. Identical symbols
match for zero cost without invoking substitution. Direction transforms a into
b; asymmetric insert/delete or symbol-confusion costs are supported.

The implementation is weighted Wagner–Fischer dynamic programming using one
row: O(n*m) time and O(m) space. It does not substitute unit distance and scale
the answer afterward. Return 1 on success, 0 on invalid data, callback costs,
allocation failure, unrepresentable totals or work limit; output is unchanged
on error. NULL arrays require zero length. A zero budget selects 10000000 cells,
including the initial row/column. Empty sequences are supported.

Tokens can be Unicode scalars, grapheme IDs or other symbols. The function does
not decode UTF-8 or normalize strings. It does not implement transpositions,
multi-character rewrite costs or guarantee a metric for arbitrary callback
costs. Do not use arbitrary weighted costs with BK-tree metric pruning.

See [`examples/modules/weighted-edit-distance.c`](../examples/modules/weighted-edit-distance.c).
