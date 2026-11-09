# mulib-hwil-module

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-hwil-module.h`  
**Implementation switch:** `MULIB_IMPL_HWIL_MODULE`  
**Dependencies:** `dsim-logic4`.

## Purpose and supported profile

Append-only combinational, single-bit HWIL netlist with forward-referenced DAG disallowed by construction; 512-node bound. Evaluates 4-state primitives, ordered input nodes. No registers, hierarchy, source spans or elaborated HDL.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_HWIL_MODULE` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_HWIL_MODULE
#include "mulib-hwil-module.h"
```

## Focused regression test

`tests/test_hwil_module.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_hwil_module.c -lm -o /tmp/test_hwil_module
/tmp/test_hwil_module
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-hwil-module.h`](../include/mulib-hwil-module.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-hwil-module.h
 * @brief Append-only combinational, single-bit HWIL netlist with forward-referenced DAG disallowed
 * by construction; 512-node bound.
 * @details C11 single-header implementation; define MULIB_IMPL_HWIL_MODULE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_HWIL_MODULE_H
#define MULIB_HWIL_MODULE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-dsim-logic4.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_hwil_gate {
    MULIB_HWIL_INPUT = 0,
    MULIB_HWIL_CONST0 = 1,
    MULIB_HWIL_CONST1 = 2,
    MULIB_HWIL_NOT = 3,
    MULIB_HWIL_AND = 4,
    MULIB_HWIL_OR = 5,
    MULIB_HWIL_XOR = 6,
    MULIB_HWIL_MUX = 7
} mulib_hwil_gate_t;
typedef struct mulib_hwil_node {
    mulib_hwil_gate_t op;
    uint16_t a, b, c;
} mulib_hwil_node_t;
typedef struct mulib_hwil_module {
    size_t count;
    mulib_hwil_node_t nodes[512];
} mulib_hwil_module_t;
int mulib_hwil_module_add(mulib_hwil_module_t *m, mulib_hwil_gate_t op, uint16_t a, uint16_t b,
                          uint16_t c, uint16_t *id);
int mulib_hwil_module_eval(const mulib_hwil_module_t *m, const mulib_dsim_logic4_t *input,
                           size_t inputs, mulib_dsim_logic4_t *values, size_t capacity);
int mulib_hwil_module_valid(const mulib_hwil_module_t *m);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_HWIL_MODULE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-hwil-module.h`](../include/mulib-hwil-module.h) |
| Implementation switch | `MULIB_IMPL_HWIL_MODULE` |
| Repository-local includes | [mulib-dsim-logic4](mulib-dsim-logic4.md) |
| Complete example | [`examples/modules/hwil-module.c`](../examples/modules/hwil-module.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hwil-module.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
