# mulib-fmt-sv

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-fmt-sv.h`  
**Implementation switch:** `MULIB_IMPL_FMT_SV`  
**Dependencies:** `hwil-module`.

## Purpose and supported profile

Structural synthesizable Verilog module emitter for one-bit HWIL combinational nodes, one output and generated port names. No source-level SystemVerilog, signedness, hierarchy or register support.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_FMT_SV` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_FMT_SV
#include "mulib-fmt-sv.h"
```

## Focused regression test

`tests/test_fmt_sv.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_fmt_sv.c -lm -o /tmp/test_fmt_sv
/tmp/test_fmt_sv
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-fmt-sv.h`](../include/mulib-fmt-sv.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-fmt-sv.h
 * @brief Structural synthesizable Verilog module emitter for one-bit HWIL combinational nodes, one
 * output and generated port names.
 * @details C11 single-header implementation; define MULIB_IMPL_FMT_SV in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FMT_SV_H
#define MULIB_FMT_SV_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "mulib-hwil-module.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fmt_sv_emit(FILE *out, const mulib_hwil_module_t *module, size_t output_node);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FMT_SV_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fmt-sv.h`](../include/mulib-fmt-sv.h) |
| Implementation switch | `MULIB_IMPL_FMT_SV` |
| Repository-local includes | [mulib-hwil-module](mulib-hwil-module.md) |
| Complete example | [`examples/modules/fmt-sv.c`](../examples/modules/fmt-sv.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/fmt-sv.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
