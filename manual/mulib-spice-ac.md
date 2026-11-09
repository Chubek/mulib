# mulib-spice-ac

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-spice-ac.h`  
**Implementation switch:** `MULIB_IMPL_SPICE_AC`  
**Dependencies:** `spice-netlist`.

## Purpose and supported profile

Frequency-domain complex MNA for linear R/C/I/V circuits at one nonnegative frequency, interpreting independent DC source magnitudes as AC phasor amplitudes of zero phase for this narrow profile. No source AC metadata, inductor, BJT/MOS small-signal, noise, or sweep. Uses C11 complex arithmetic.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_SPICE_AC` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_SPICE_AC
#include "mulib-spice-ac.h"
```

## Focused regression test

`tests/test_spice_ac.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_spice_ac.c -lm -o /tmp/test_spice_ac
/tmp/test_spice_ac
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-spice-ac.h`](../include/mulib-spice-ac.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-spice-ac.h
 * @brief Frequency-domain complex MNA for linear R/C/I/V circuits at one nonnegative frequency,
 * interpreting independent DC source magnitudes as AC phasor amplitudes of zero phase for this
 * narrow profile.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_AC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_AC_H
#define MULIB_SPICE_AC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-spice-netlist.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_spice_complex {
    double re, im;
} mulib_spice_complex_t;
int mulib_spice_ac_solve(const mulib_spice_netlist_t *net, double frequency_hz,
                         mulib_spice_complex_t *node_voltages, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_AC_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-spice-ac.h`](../include/mulib-spice-ac.h) |
| Implementation switch | `MULIB_IMPL_SPICE_AC` |
| Repository-local includes | [mulib-spice-netlist](mulib-spice-netlist.md) |
| Complete example | [`examples/modules/spice-ac.c`](../examples/modules/spice-ac.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/spice-ac.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
