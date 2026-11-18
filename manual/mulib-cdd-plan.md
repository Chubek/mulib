# mulib-cdd-plan

ISO C11 single-header kernel. To compile this module with its dependencies in exactly one translation unit:

```c
#define MULIB_IMPL_CDD_CATALOGUE
#define MULIB_IMPL_CDD_PLAN
#include "mulib-cdd-plan.h"
```

Other translation units include the header without switches. See `examples/cdd-pipeline.c` and `tests/cdd-test.c`.

## API
`mulib_cdd_problem_t` supplies goal requirements, pre-existing capabilities, a 64-bit allowed-component mask (UINT64_MAX allows all, zero allows none), and max total cost (UINT64_MAX is unrestricted). Empty goals are legal. `mulib_cdd_plan_t` holds an array of entry indices in activation order, count, exact total cost and `found=1` for a candidate solution. Plans are tied to the input catalogue and problem; do not reuse them after either changes.

- `mulib_cdd_problem_valid(p)` checks inputs and revision ranges.
- `mulib_cdd_satisfied(c,p,selected,r)` and `mulib_cdd_can_append(c,p,selected,i)` evaluate availability and safe extension of a previously valid selected set. These low-level helpers require a validated catalogue and problem; `can_append` does not check total cost.
- `mulib_cdd_plan_validate(c,p,plan,failed)` independently verifies allowed indices, duplicates, symmetric conflicts, prior requirements, accumulated cost, exact claimed cost and final goal coverage. `failed` is optional; on an invalid step it is the step index; end-of-plan errors report count; initial structural errors report NONE. On success it is count.

Capabilities accumulate; a requirement must be provided externally or by an earlier step. A component cannot satisfy its own prerequisite unless an earlier/external provider exists. There is no resource consumption or invalidation model.
