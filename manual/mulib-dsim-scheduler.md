# mulib-dsim-scheduler

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-dsim-scheduler.h`  
**Implementation switch:** `MULIB_IMPL_DSIM_SCHEDULER`  
**Dependencies:** `dsim-logic4`.

## Purpose and supported profile

Stable min-heap event ordering by time, delta, region and insertion order, with peek and cancellation by signal. Cancellation preserves the original order keys. No process execution or complete HDL event-region semantics.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_DSIM_SCHEDULER` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_DSIM_SCHEDULER
#include "mulib-dsim-scheduler.h"
```

## Focused regression test

`tests/test_dsim_scheduler.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_dsim_scheduler.c -lm -o /tmp/test_dsim_scheduler
/tmp/test_dsim_scheduler
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dsim-scheduler.h`](../include/mulib-dsim-scheduler.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dsim-scheduler.h
 * @brief Stable min-heap event ordering by time, delta, region and insertion order, with peek and
 * cancellation by signal.
 * @details C11 single-header implementation; define MULIB_IMPL_DSIM_SCHEDULER in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_DSIM_SCHEDULER_H
#define MULIB_DSIM_SCHEDULER_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-dsim-logic4.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_dsim_event {
    uint64_t time, delta, order;
    uint32_t region, signal;
    mulib_dsim_logic4_t value;
} mulib_dsim_event_t;
typedef struct mulib_dsim_scheduler {
    mulib_dsim_event_t *heap;
    size_t count, cap;
    uint64_t next_order;
} mulib_dsim_scheduler_t;
int mulib_dsim_scheduler_push(mulib_dsim_scheduler_t *s, uint64_t time, uint64_t delta,
                              uint32_t region, uint32_t signal, mulib_dsim_logic4_t value);
int mulib_dsim_scheduler_pop(mulib_dsim_scheduler_t *s, mulib_dsim_event_t *out);
void mulib_dsim_scheduler_free(mulib_dsim_scheduler_t *s);
int mulib_dsim_scheduler_peek(const mulib_dsim_scheduler_t *s, mulib_dsim_event_t *out);
size_t mulib_dsim_scheduler_cancel(mulib_dsim_scheduler_t *s, uint32_t signal);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DSIM_SCHEDULER_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dsim-scheduler.h`](../include/mulib-dsim-scheduler.h) |
| Implementation switch | `MULIB_IMPL_DSIM_SCHEDULER` |
| Repository-local includes | [mulib-dsim-logic4](mulib-dsim-logic4.md) |
| Complete example | [`examples/modules/dsim-scheduler.c`](../examples/modules/dsim-scheduler.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dsim-scheduler.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
