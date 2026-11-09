# mulib-fpga-lut

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-fpga-lut.h`  
**Implementation switch:** `MULIB_IMPL_FPGA_LUT`  
**Dependencies:** `synth-truthtable`.

## Purpose and supported profile

Pure combinational lookup-table element evaluator and truth-table mapping for up to six inputs. Does not implement FPGA packing, vendor LUT INIT bit ordering or bitstreams.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_FPGA_LUT` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_FPGA_LUT
#include "mulib-fpga-lut.h"
```

## Focused regression test

`tests/test_fpga_lut.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_fpga_lut.c -lm -o /tmp/test_fpga_lut
/tmp/test_fpga_lut
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-fpga-lut.h`](../include/mulib-fpga-lut.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-fpga-lut.h
 * @brief Pure combinational lookup-table element evaluator and truth-table mapping for up to six
 * inputs.
 * @details C11 single-header implementation; define MULIB_IMPL_FPGA_LUT in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FPGA_LUT_H
#define MULIB_FPGA_LUT_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-synth-truthtable.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_fpga_lut {
    uint8_t inputs;
    uint64_t configuration;
} mulib_fpga_lut_t;
int mulib_fpga_lut_eval(mulib_fpga_lut_t lut, uint8_t input_bits, int *out);
int mulib_fpga_lut_from_truth(mulib_synth_truth_t truth, mulib_fpga_lut_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FPGA_LUT_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fpga-lut.h`](../include/mulib-fpga-lut.h) |
| Implementation switch | `MULIB_IMPL_FPGA_LUT` |
| Repository-local includes | [mulib-synth-truthtable](mulib-synth-truthtable.md) |
| Complete example | [`examples/modules/fpga-lut.c`](../examples/modules/fpga-lut.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/fpga-lut.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
