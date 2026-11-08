# MuLIB color

[Manuals](README.md) · [Library index](index.md)

RGB to HSV and HSV to RGB color-model conversion. RGB channels are assumed to lie between 0 and 1 and HSV hue is measured in degrees. The API does not apply gamma correction or clamp invalid input.

## Contract and behavior

RGB components and HSV saturation/value use the normalized 0..1
convention; hue is in degrees. HSV-to-RGB wraps finite hue modulo 360, including
negative angles. RGB-to-HSV assigns hue zero to gray colors and saturation zero
to black. Inputs are not clamped or validated, and results are floating-point
values rather than status codes. No gamma correction, alpha channel, color
profile conversion, or allocation is performed.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_COLOR
#include "mulib-color.h"
int main(void) {
    mulib_color_hsv_t h = mulib_color_to_hsv((mulib_color_rgb_t){1, 0, 0});
    assert(h.h == 0);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-color.h`](../include/mulib-color.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-color.h
 * @brief RGB to HSV and HSV to RGB color-model conversion.
 * @defgroup mulib_color MuLIB color
 * @{
 * @details RGB channels are assumed to lie between 0 and 1 and HSV hue is
 * measured in degrees. The API does not apply gamma correction or clamp
 * invalid input.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_COLOR before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_COLOR_H
#define MULIB_COLOR_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief RGB to HSV and HSV to RGB color-model conversion. */
  typedef struct mulib_color_rgb
  {
    double r, g, b;
  } mulib_color_rgb_t;
  /** @brief RGB to HSV and HSV to RGB color-model conversion. */
  typedef struct mulib_color_hsv
  {
    double h, s, v;
  } mulib_color_hsv_t;
  /** @brief mulib color to hsv; see manual/mulib-color.md for contracts and
   * examples. */
  mulib_color_hsv_t mulib_color_to_hsv (mulib_color_rgb_t color);
  /** @brief mulib color from hsv; see manual/mulib-color.md for contracts and
   * examples. */
  mulib_color_rgb_t mulib_color_from_hsv (mulib_color_hsv_t hsv);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_COLOR_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-color.h`](../include/mulib-color.h) |
| Implementation switch | `MULIB_IMPL_COLOR` |
| Repository-local includes | None |
| Complete example | [`examples/modules/color.c`](../examples/modules/color.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/color.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
