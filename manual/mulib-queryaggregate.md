# queryaggregate — streaming numeric aggregates

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_QUERYAGGREGATE`.

Zero-initialize. Add finite values to count/sum/min/max; mean is sum/count,
avoiding overflow from subtracting opposite-sign extreme values. Merge combines
two valid aggregates, preserving the source; self-merge is rejected. Empty
source merge succeeds. Both return 1 on success or 0 on invalid arguments,
count overflow or nonfinite sum, without mutation on failure. Min/max/mean
are meaningful only for nonempty aggregates. Ordinary floating rounding applies;
this is not compensated summation. O(1), allocation-free.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-queryaggregate.h`](../include/mulib-queryaggregate.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Streaming count/sum/min/max/mean aggregates on finite double inputs. C11
 * single-header; define MULIB_IMPL_QUERYAGGREGATE once. */
#ifndef MULIB_QUERYAGGREGATE_H
#define MULIB_QUERYAGGREGATE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    size_t count;
    double sum, min, max, mean;
  } mulib_queryaggregate_t;
  int mulib_queryaggregate_add (mulib_queryaggregate_t *a, double value);
  int mulib_queryaggregate_merge (mulib_queryaggregate_t *dst,
                                  const mulib_queryaggregate_t *src);
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
| Header | [`include/mulib-queryaggregate.h`](../include/mulib-queryaggregate.h) |
| Implementation switch | `MULIB_IMPL_QUERYAGGREGATE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/queryaggregate.c`](../examples/modules/queryaggregate.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/queryaggregate.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
