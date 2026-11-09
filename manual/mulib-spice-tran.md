# mulib-spice-tran

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-spice-tran.h`  
**Implementation switch:** `MULIB_IMPL_SPICE_TRAN`  
**Dependencies:** `spice-netlist`, `spice-stamp`, `spice-r`, `spice-c`, `spice-independent`, `num-dense`.

## Purpose and supported profile

Fixed-timestep backward-Euler transient for linear R/C/I/V networks, zero initial capacitor voltages, time-invariant DC sources, no inductors. Not adaptive, no source breakpoints, no nonlinear devices. Result is transactionally committed after all steps.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_SPICE_TRAN` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_SPICE_TRAN
#include "mulib-spice-tran.h"
```

## Focused regression test

`tests/test_spice_tran.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_spice_tran.c -lm -o /tmp/test_spice_tran
/tmp/test_spice_tran
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-spice-tran.h`](../include/mulib-spice-tran.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-spice-tran.h
 * @brief Fixed-timestep backward-Euler transient for linear R/C/I/V networks, zero initial
 * capacitor voltages, time-invariant DC sources, no inductors.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_TRAN in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_TRAN_H
#define MULIB_SPICE_TRAN_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-spice-netlist.h"
#include "mulib-spice-stamp.h"
#include "mulib-spice-r.h"
#include "mulib-spice-c.h"
#include "mulib-spice-independent.h"
#include "mulib-num-dense.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_tran_fixed(const mulib_spice_netlist_t *net, double dt, size_t steps,
                           uint16_t probe, double *samples);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_TRAN_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-spice-tran.h`](../include/mulib-spice-tran.h) |
| Implementation switch | `MULIB_IMPL_SPICE_TRAN` |
| Repository-local includes | [mulib-num-dense](mulib-num-dense.md), [mulib-spice-c](mulib-spice-c.md), [mulib-spice-independent](mulib-spice-independent.md), [mulib-spice-netlist](mulib-spice-netlist.md), [mulib-spice-r](mulib-spice-r.md), [mulib-spice-stamp](mulib-spice-stamp.md) |
| Complete example | [`examples/modules/spice-tran.c`](../examples/modules/spice-tran.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/spice-tran.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
