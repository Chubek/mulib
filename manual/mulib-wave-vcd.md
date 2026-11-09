# mulib-wave-vcd

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-wave-vcd.h`  
**Implementation switch:** `MULIB_IMPL_WAVE_VCD`  
**Dependencies:** `dsim-logic4`.

## Purpose and supported profile

VCD writer for one scalar four-state signal with a fixed identifier (!) and 1ns timescale. Multiple signals, scopes, and variable-width vectors not supported. FILE is caller-owned.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_WAVE_VCD` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_WAVE_VCD
#include "mulib-wave-vcd.h"
```

## Focused regression test

`tests/test_wave_vcd.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_wave_vcd.c -lm -o /tmp/test_wave_vcd
/tmp/test_wave_vcd
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-wave-vcd.h`](../include/mulib-wave-vcd.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-wave-vcd.h
 * @brief VCD writer for one scalar four-state signal with a fixed identifier (!) and 1ns timescale.
 * @details C11 single-header implementation; define MULIB_IMPL_WAVE_VCD in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_WAVE_VCD_H
#define MULIB_WAVE_VCD_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "mulib-dsim-logic4.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_wave_vcd {
    FILE *stream;
    uint64_t time;
    int started;
} mulib_wave_vcd_t;
int mulib_wave_vcd_begin(mulib_wave_vcd_t *v, FILE *fp, const char *signal_name);
int mulib_wave_vcd_at(mulib_wave_vcd_t *v, uint64_t time, mulib_dsim_logic4_t value);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WAVE_VCD_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-wave-vcd.h`](../include/mulib-wave-vcd.h) |
| Implementation switch | `MULIB_IMPL_WAVE_VCD` |
| Repository-local includes | [mulib-dsim-logic4](mulib-dsim-logic4.md) |
| Complete example | [`examples/modules/wave-vcd.c`](../examples/modules/wave-vcd.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/wave-vcd.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
