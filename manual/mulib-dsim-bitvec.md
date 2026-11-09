# mulib-dsim-bitvec

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-dsim-bitvec.h`  
**Implementation switch:** `MULIB_IMPL_DSIM_BITVEC`  
**Dependencies:** `dsim-logic4`.

## Purpose and supported profile

Four-state vectors of 1..64 bits with string parsing/formatting, extraction, bitwise AND/OR/XOR/NOT, zero-filled logical shifts, concatenation and modular addition. Addition yields an all-X vector if either operand contains X/Z. Shifts preserve X/Z; bitwise operations treat Z as unknown. No signed extension or arbitrary-width arithmetic.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_DSIM_BITVEC` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_DSIM_BITVEC
#include "mulib-dsim-bitvec.h"
```

## Focused regression test

`tests/test_dsim_bitvec.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_dsim_bitvec.c -lm -o /tmp/test_dsim_bitvec
/tmp/test_dsim_bitvec
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dsim-bitvec.h`](../include/mulib-dsim-bitvec.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dsim-bitvec.h
 * @brief Four-state vectors of 1..64 bits with string parsing/formatting, extraction, bitwise
 * AND/OR/XOR/NOT, zero-filled logical shifts, concatenation and modular addition.
 * @details C11 single-header implementation; define MULIB_IMPL_DSIM_BITVEC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_DSIM_BITVEC_H
#define MULIB_DSIM_BITVEC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-dsim-logic4.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_dsim_bitvec {
    uint64_t value, known, highz;
    uint8_t width;
} mulib_dsim_bitvec_t;
int mulib_dsim_bitvec_parse(mulib_dsim_bitvec_t *v, const char *msb_bits);
int mulib_dsim_bitvec_format(mulib_dsim_bitvec_t v, char *dst, size_t length);
mulib_dsim_logic4_t mulib_dsim_bitvec_bit(mulib_dsim_bitvec_t v, unsigned index);
int mulib_dsim_bitvec_and(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b, mulib_dsim_bitvec_t *out);
/* Operations preserve width; arithmetic turns the entire result X if any bit is X/Z.
 * Shift vacated bits are known zero. Concatenation places a above b. */
int mulib_dsim_bitvec_or(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b, mulib_dsim_bitvec_t *out);
int mulib_dsim_bitvec_xor(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b, mulib_dsim_bitvec_t *out);
int mulib_dsim_bitvec_not(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t *out);
int mulib_dsim_bitvec_add(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b, mulib_dsim_bitvec_t *out);
int mulib_dsim_bitvec_shift(mulib_dsim_bitvec_t a, unsigned amount, int right,
                            mulib_dsim_bitvec_t *out);
int mulib_dsim_bitvec_concat(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b,
                             mulib_dsim_bitvec_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DSIM_BITVEC_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dsim-bitvec.h`](../include/mulib-dsim-bitvec.h) |
| Implementation switch | `MULIB_IMPL_DSIM_BITVEC` |
| Repository-local includes | [mulib-dsim-logic4](mulib-dsim-logic4.md) |
| Complete example | [`examples/modules/dsim-bitvec.c`](../examples/modules/dsim-bitvec.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dsim-bitvec.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
