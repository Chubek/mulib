# Weighted edit distance

Define `MULIB_IMPL_WEIGHED_EDIT_DISTANE`; link `-lm`.
[Example](../examples/modules/weighed_edit_distane.c). Filename spelling retained.

`mulib_weighted_edit_distance` transforms a binary buffer a into b, with supplied
finite nonnegative insertion, deletion and substitution costs. Equal bytes
cost zero. Direction matters if insertion and deletion differ. Zero costs are
valid. No transposition, context weighting or Unicode decoding is performed.

NULL buffers require zero length. Invalid input, dimension overflow and OOM
give NAN; numeric cost overflow gives INFINITY. Inputs are borrowed. Temporary
storage O(m), time O(n*m); no operation-count/work limit is imposed.
