# MuLIB semver

[Manuals](README.md) · [Library index](index.md)

Semantic version triplet parsing and numeric precedence comparison. Implements `MAJOR.MINOR.PATCH` only. Prerelease tags and build metadata from the complete SemVer 2.0 specification are not supported.

## Contract and behavior

Parse accepts exactly three decimal unsigned fields `major.minor.patch`.
Leading zeros in multi-digit fields, signs, whitespace, trailing text, and
values beyond `UINT_MAX` are rejected. Prerelease/build metadata is not part
of this profile. Parse returns 1 on success and 0 on failure, preserving output
on failure. Compare takes values by value and returns -1, 0, or 1 by numeric
major/minor/patch order. No allocation occurs.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_SEMVER
#include "mulib-semver.h"
int main(void) {
    mulib_semver_t v;
    assert(mulib_semver_parse("1.2.3", &v));
    assert(v.patch == 3);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-semver.h`](../include/mulib-semver.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-semver.h
 * @brief Semantic version triplet parsing and numeric precedence comparison.
 * @defgroup mulib_semver MuLIB semver
 * @{
 * @details Implements `MAJOR.MINOR.PATCH` only. Prerelease tags and build
 * metadata from the complete SemVer 2.0 specification are not supported.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SEMVER before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_SEMVER_H
#define MULIB_SEMVER_H
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

  /** @brief Semantic version triplet parsing and numeric precedence
   * comparison. */
  typedef struct mulib_semver
  {
    unsigned major, minor, patch;
  } mulib_semver_t;
  /** @brief Parse three unsigned decimal components without signs, whitespace
   * or leading zeroes. Overflow and trailing data fail without modifying out.
   * No prerelease/build syntax.
   */
  int mulib_semver_parse (const char *source, mulib_semver_t *out);
  /** @brief mulib semver compare; see manual/mulib-semver.md for contracts and
   * examples. */
  int mulib_semver_compare (mulib_semver_t a, mulib_semver_t b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SEMVER_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-semver.h`](../include/mulib-semver.h) |
| Implementation switch | `MULIB_IMPL_SEMVER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/semver.c`](../examples/modules/semver.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/semver.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
