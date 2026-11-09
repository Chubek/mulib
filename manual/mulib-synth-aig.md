# mulib-synth-aig

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-synth-aig.h`  
**Implementation switch:** `MULIB_IMPL_SYNTH_AIG`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Bounded AIG with constant 0/1, complemented edges, structural hashing and simple constant/identity simplification. Maximum 1023 nodes, no deletion. No sequential latches or technology mapping.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_SYNTH_AIG` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_SYNTH_AIG
#include "mulib-synth-aig.h"
```

## Focused regression test

`tests/test_synth_aig.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_synth_aig.c -lm -o /tmp/test_synth_aig
/tmp/test_synth_aig
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-synth-aig.h`](../include/mulib-synth-aig.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-synth-aig.h
 * @brief Bounded AIG with constant 0/1, complemented edges, structural hashing and simple
 * constant/identity simplification.
 * @details C11 single-header implementation; define MULIB_IMPL_SYNTH_AIG in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SYNTH_AIG_H
#define MULIB_SYNTH_AIG_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef uint16_t mulib_synth_aig_lit_t;
typedef struct mulib_synth_aig_node {
    uint16_t a, b;
    uint8_t input;
} mulib_synth_aig_node_t;
typedef struct mulib_synth_aig {
    size_t count, inputs;
    mulib_synth_aig_node_t nodes[1024];
} mulib_synth_aig_t;
int mulib_synth_aig_input(mulib_synth_aig_t *g, mulib_synth_aig_lit_t *out);
int mulib_synth_aig_and(mulib_synth_aig_t *g, mulib_synth_aig_lit_t a, mulib_synth_aig_lit_t b,
                        mulib_synth_aig_lit_t *out);
int mulib_synth_aig_eval(const mulib_synth_aig_t *g, const uint8_t *inputs, size_t count,
                         mulib_synth_aig_lit_t root, int *out);
int mulib_synth_aig_valid(const mulib_synth_aig_t *g);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SYNTH_AIG_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-synth-aig.h`](../include/mulib-synth-aig.h) |
| Implementation switch | `MULIB_IMPL_SYNTH_AIG` |
| Repository-local includes | None |
| Complete example | [`examples/modules/synth-aig.c`](../examples/modules/synth-aig.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/synth-aig.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
