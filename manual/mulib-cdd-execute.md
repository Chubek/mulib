# mulib-cdd-execute

ISO C11 single-header kernel. To compile this module with its dependencies in exactly one translation unit:

```c
#define MULIB_IMPL_CDD_CATALOGUE
#define MULIB_IMPL_CDD_PLAN
#define MULIB_IMPL_CDD_EXECUTE
#include "mulib-cdd-execute.h"
```

Other translation units include the header without switches. See `examples/cdd-pipeline.c` and `tests/cdd-test.c`.

## API
`mulib_cdd_execute(c,p,plan,executor,report)` requires a report, validates the whole plan before calling the host, then invokes `apply(entry,context)` for each selected component. Callback success is 0; nonzero stops execution. If `undo` exists, completed steps are compensated in reverse order; rollback continues even if a compensation fails. The failed apply step must self-clean its partial effects. No automatic unloading or rollback happens on success.

The report contains count of successful apply callbacks (`completed`, even if later undone), failed step or NONE, original callback error code, number of attempted compensations and number of failed compensations. `CALLBACK` means apply failure; preflight validation errors are propagated without invoking either callback. A preflight report has zero counters/code and `failed_step` as reported by plan validation (or NONE if no step applies). Without undo, no compensation is attempted.

The host supplies and authorizes the callbacks and decides which descriptors they may act on. Plan validation checks composition, not authorization. `implementation` is a display label, not a symbol to resolve or code to load. Caller owns context and descriptors. Keep the plan, catalogue, problem, executor and their borrowed payloads alive and immutable throughout the call; callbacks may mutate their own context but not the report or those inputs. Execution is synchronous, trusted and not transactional, isolated, dynamically linked or parallel.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-cdd-execute.h`](../include/mulib-cdd-execute.h) |
| Implementation switch | `MULIB_IMPL_CDD_EXECUTE` |
| Repository-local includes | [mulib-cdd-plan](mulib-cdd-plan.md) |
| Complete example | [`examples/cdd-pipeline.c`](../examples/cdd-pipeline.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/cdd-pipeline.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
