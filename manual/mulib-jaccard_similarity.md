# Jaccard similarity

Define `MULIB_IMPL_JACCARD_SIMILARITY`; link `-lm` where needed.
[Example](../examples/modules/jaccard_similarity.c).

`mulib_jaccard_similarity(a,n,b,m)` treats borrowed uint64 arrays as sets,
ignoring duplicate elements and order, and returns intersection/union.
Two empty sets return 1, exactly one empty set returns 0. NULL requires zero
corresponding length; invalid input returns NAN.
No hashing, allocation, sorting, weighting or multiset semantics: time
O(n²+m²+n*m), constant extra storage. Intended for small sets.
