# mulib-cdd-query

ISO C11 single-header kernel. To compile this module with its dependencies in exactly one translation unit:

```c
#define MULIB_IMPL_CDD_CATALOGUE
#define MULIB_IMPL_CDD_QUERY
#include "mulib-cdd-query.h"
```

Other translation units include the header without switches. See `examples/cdd-pipeline.c` and `tests/cdd-test.c`.

## API
A `mulib_cdd_query_t` combines an optional category (NULL = any), a conjunction of required provided capabilities and a conjunction of typed metadata predicates. Missing attributes fail all predicates, including NE; EXISTS ignores its value. EQ/NE compare same-kind integer, boolean or string values; LT/LE/GT/GE are only for integers. All caps must be provided by the single candidate, not by its dependency closure.

- `mulib_cdd_query_valid(q)` validates predicate syntax.
- `mulib_cdd_query_matches(e,q)` evaluates against one structurally validated entry.
- `mulib_cdd_query_run(c,q,out,capacity,needed)` validates the catalogue and query, then sorts matching indices by cost ascending, ID ascending as tie-breaker. Writes at most capacity; `needed` is mandatory. A NULL output and zero capacity is valid for size discovery, returning LIMIT when matches exist. On argument or catalogue validation failure, including LIMIT for an oversized catalogue, `needed` is zero if supplied and the output array is untouched. Output storage must not overlap the inputs or `needed`.

This is a filter, not a dependency resolver. To constrain composition, translate candidate indices into a `mulib_cdd_problem_t.allowed` mask; keep required support categories enabled as appropriate. O(n²) worst-case insertion sort, bounded by 64 candidates.
