# mulib-formal-cec

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-formal-cec.h`  
**Implementation switch:** `MULIB_IMPL_FORMAL_CEC`  
**Dependencies:** `synth-truthtable`.

## Purpose and supported profile

Exact combinational equivalence for explicit truth tables with at most six inputs. Not sequential equivalence, miter conversion or SAT-based CEC.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_FORMAL_CEC` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_FORMAL_CEC
#include "mulib-formal-cec.h"
```

## Focused regression test

`tests/test_formal_cec.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_formal_cec.c -lm -o /tmp/test_formal_cec
/tmp/test_formal_cec
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-formal-cec.h`](../include/mulib-formal-cec.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-formal-cec.h
 * @brief Exact combinational equivalence for explicit truth tables with at most six inputs.
 * @details C11 single-header implementation; define MULIB_IMPL_FORMAL_CEC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FORMAL_CEC_H
#define MULIB_FORMAL_CEC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-synth-truthtable.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_truth_equivalent(mulib_synth_truth_t a, mulib_synth_truth_t b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FORMAL_CEC_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-formal-cec.h`](../include/mulib-formal-cec.h) |
| Implementation switch | `MULIB_IMPL_FORMAL_CEC` |
| Repository-local includes | [mulib-synth-truthtable](mulib-synth-truthtable.md) |
| Complete example | [`examples/modules/formal-cec.c`](../examples/modules/formal-cec.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/formal-cec.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
