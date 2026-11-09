# mulib-wave-signal

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-wave-signal.h`  
**Implementation switch:** `MULIB_IMPL_WAVE_SIGNAL`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Owned finite time-value sample series with nondecreasing timestamps, validation and interpolation. Analog interpolation is linear; digital interpolation holds the previous sample. Repeated timestamps select the last sample at that time. Out-of-range queries fail without changing output.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_WAVE_SIGNAL` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_WAVE_SIGNAL
#include "mulib-wave-signal.h"
```

## Focused regression test

`tests/test_wave_signal.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_wave_signal.c -lm -o /tmp/test_wave_signal
/tmp/test_wave_signal
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-wave-signal.h`](../include/mulib-wave-signal.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-wave-signal.h
 * @brief Owned finite time-value sample series with nondecreasing timestamps, validation and
 * interpolation.
 * @details C11 single-header implementation; define MULIB_IMPL_WAVE_SIGNAL in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_WAVE_SIGNAL_H
#define MULIB_WAVE_SIGNAL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_wave_kind { MULIB_WAVE_DIGITAL = 1, MULIB_WAVE_ANALOG = 2 } mulib_wave_kind_t;
typedef struct mulib_wave_sample {
    double time, value;
} mulib_wave_sample_t;
typedef struct mulib_wave_signal {
    mulib_wave_sample_t *samples;
    size_t count, cap;
    mulib_wave_kind_t kind;
} mulib_wave_signal_t;
int mulib_wave_signal_push(mulib_wave_signal_t *s, double time, double value);
void mulib_wave_signal_free(mulib_wave_signal_t *s);
int mulib_wave_signal_valid(const mulib_wave_signal_t *s);
int mulib_wave_signal_interpolate(const mulib_wave_signal_t *s, double time, double *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_WAVE_SIGNAL_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-wave-signal.h`](../include/mulib-wave-signal.h) |
| Implementation switch | `MULIB_IMPL_WAVE_SIGNAL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/wave-signal.c`](../examples/modules/wave-signal.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/wave-signal.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
