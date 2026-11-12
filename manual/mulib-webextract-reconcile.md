# webextract-reconcile

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Evidence-aware consensus of candidate numeric price extractions.

## Contract and supported profile

Select one of 1..4096 finite nonnegative prices, each with confidence in
[0,1]. Candidate score is confidence*(has_evidence?1.25:0.5) plus .01*priority
plus .2*confidence for each other candidate within tolerance*max(1,candidate
price). Tolerance is finite/nonnegative; larger priority is preferred; ties keep
the first candidate. Both outputs remain unchanged on failure. O(n squared),
no allocation. Evidence is a caller assertion; this function does not verify it,
blend prices or return calibrated confidence.

## Integration and ownership

Include `include/mulib-webextract-reconcile.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_WEBEXTRACT_RECONCILE`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/webextract-reconcile.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-webextract-reconcile.h`](../include/mulib-webextract-reconcile.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-webextract-reconcile.h
 * @brief Evidence-aware consensus of candidate numeric price extractions.
 * C11 single-header module. Define MULIB_IMPL_WEBEXTRACT_RECONCILE in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_WEBEXTRACT_RECONCILE_H
#define MULIB_WEBEXTRACT_RECONCILE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_webextract_candidate
  {
    double price, confidence;
    unsigned source_priority;
    int has_evidence;
  } mulib_webextract_candidate_t;
  int
  /** @brief See manual/mulib-webextract-reconcile.md for this operation's
     contract. */
  mulib_webextract_reconcile (const mulib_webextract_candidate_t *candidates,
                              size_t n, double relative_tolerance,
                              double *price, size_t *selected);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBEXTRACT_RECONCILE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-webextract-reconcile.h`](../include/mulib-webextract-reconcile.h) |
| Implementation switch | `MULIB_IMPL_WEBEXTRACT_RECONCILE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/webextract-reconcile.c`](../examples/modules/webextract-reconcile.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/webextract-reconcile.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
