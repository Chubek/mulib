# columnstats — numeric pruning statistics

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_COLUMNSTATS`.

Zero-initialize. Add increments `values` for finite valid values or `nulls` for
invalid/null rows; nonzero validity means valid. Min/max describe valid values
only and are undefined until one exists. Add returns 1 on success, 0 for invalid
arguments or exhausted counters, preserving state on failure.
`may_match` tests inclusive interval overlap, returning false for no valid
values, nonfinite bounds or low > high. A positive result is a pruning candidate,
not proof that a row matches. All operations are O(1), allocation-free.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-columnstats.h`](../include/mulib-columnstats.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Min/max/null statistics for numeric column pruning. C11 single-header; define
 * MULIB_IMPL_COLUMNSTATS once. */
#ifndef MULIB_COLUMNSTATS_H
#define MULIB_COLUMNSTATS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t values, nulls;
    double min, max;
  } mulib_columnstats_t;
  int mulib_columnstats_add (mulib_columnstats_t *s, double value, int valid);
  /* Returns true if [min,max] could contain the closed predicate range. */
  int mulib_columnstats_may_match (const mulib_columnstats_t *s, double low,
                                   double high);
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
| Header | [`include/mulib-columnstats.h`](../include/mulib-columnstats.h) |
| Implementation switch | `MULIB_IMPL_COLUMNSTATS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/columnstats.c`](../examples/modules/columnstats.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/columnstats.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
