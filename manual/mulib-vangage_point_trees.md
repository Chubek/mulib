# Vantage-point metric trees

Define `MULIB_IMPL_VANGAGE_POINT_TREES`; link `-lm`.
[Example](../examples/modules/vangage_point_trees.c). Filename spelling retained.

Zero-initialize or init uninitialized storage. Build copies fixed-stride points
and constructs balanced median-distance partitions, retaining original indices.
Stride must support the metric's alignment for every point (e.g. sizeof(type)).
Pointer members are shallow copies: pointed-to data remains caller-owned.
Metric/context are borrowed, deterministic and immutable for tree lifetime.
Require finite nonnegative symmetric distances obeying the triangle inequality;
these metric laws are not verified. Bad returned distances fail explicitly.

Build returns 0 on invalid/OOM/bad metric, preserving the old tree. Empty build
works with NULL points and replaces it with an empty tree. Search takes a finite
nonnegative radius; callback borrows points, must not mutate tree/context,
nonzero continues. Return counts include stop; SIZE_MAX invalid/bad metric,
possibly after earlier callbacks. Free resets ownership; no shallow tree copies.
Build uses O(n) auxiliary records and O(n log²n) sorting time; query uses O(log n)
recursive stack and can visit every point. No nearest-neighbor/k-NN API.
