# mulib-synth-truthtable

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-synth-truthtable.h`  
**Implementation switch:** `MULIB_IMPL_SYNTH_TRUTHTABLE`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Packed truth tables for zero to six inputs with variable generation, Boolean operations, evaluation and variable-removing cofactors. Bit zero corresponds to the all-zero input assignment. No NPN canonicalization or sequential synthesis.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_SYNTH_TRUTHTABLE` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_SYNTH_TRUTHTABLE
#include "mulib-synth-truthtable.h"
```

## Focused regression test

`tests/test_synth_truthtable.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_synth_truthtable.c -lm -o /tmp/test_synth_truthtable
/tmp/test_synth_truthtable
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-synth-truthtable.h`](../include/mulib-synth-truthtable.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-synth-truthtable.h
 * @brief Packed truth tables for zero to six inputs with variable generation, Boolean operations,
 * evaluation and variable-removing cofactors.
 * @details C11 single-header implementation; define MULIB_IMPL_SYNTH_TRUTHTABLE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SYNTH_TRUTHTABLE_H
#define MULIB_SYNTH_TRUTHTABLE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_synth_truth {
    uint8_t variables;
    uint64_t bits;
} mulib_synth_truth_t;
int mulib_synth_truth_valid(mulib_synth_truth_t t);
int mulib_synth_truth_variable(unsigned vars, unsigned index, mulib_synth_truth_t *out);
int mulib_synth_truth_apply(mulib_synth_truth_t a, mulib_synth_truth_t b, char operation,
                            mulib_synth_truth_t *out);
int mulib_synth_truth_eval(mulib_synth_truth_t t, uint8_t inputs, int *out);
int mulib_synth_truth_cofactor(mulib_synth_truth_t t, unsigned variable, int value,
                               mulib_synth_truth_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SYNTH_TRUTHTABLE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-synth-truthtable.h`](../include/mulib-synth-truthtable.h) |
| Implementation switch | `MULIB_IMPL_SYNTH_TRUTHTABLE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/synth-truthtable.c`](../examples/modules/synth-truthtable.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/synth-truthtable.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
