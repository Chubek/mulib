# Health hysteresis

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_HEALTH_CHECKER`. The upstream type is a declaration dependency;
no upstream functions are called. Nonzero rise/fall thresholds and zero-initialized
counters apply externally supplied consecutive probe results. No active network
probes. [Example](../examples/modules/health_checker.c), [contracts](storage-server.md).

## Hysteresis contract

Zero the success/failure counters and set positive `rise` and `fall` thresholds.
The application supplies each probe result to `record(check, upstream, success)`;
any nonzero result counts as success. Success resets consecutive failures and
increments successes up to rise, marking healthy at that threshold. Failure
does the reverse and marks unhealthy at fall. Below a threshold, the existing
health flag is retained.

Record returns 1 for a valid update, **including a failed probe**, and 0 for
NULL state or zero thresholds. The checker allocates nothing and performs no
probe, timer, or retry scheduling. Both structures remain caller-owned; protect
them against concurrent access with selection/acquisition operations.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-health_checker.h`](../include/mulib-health_checker.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-health_checker.h
 * @brief Consecutive-result health hysteresis, not an active network probe.
 * Define MULIB_IMPL_HEALTH_CHECKER. Initialize counters to zero. */
#ifndef MULIB_HEALTH_CHECKER_H
#define MULIB_HEALTH_CHECKER_H
#include "mulib-upstream.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_health_checker
  {
    unsigned successes, failures, rise, fall;
  } mulib_health_checker_t;
  /** Nonzero rise/fall required. Returns 1 on valid input. */
  int mulib_health_checker_record (mulib_health_checker_t *check,
                                   mulib_upstream_t *upstream, int success);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-health_checker.h`](../include/mulib-health_checker.h) |
| Implementation switch | `MULIB_IMPL_HEALTH_CHECKER` |
| Repository-local includes | [mulib-upstream](mulib-upstream.md) |
| Complete example | [`examples/modules/health_checker.c`](../examples/modules/health_checker.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/health_checker.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
