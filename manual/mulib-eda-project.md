# mulib-eda-project

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-project.h`  
**Implementation switch:** `MULIB_IMPL_EDA_PROJECT`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Validated in-memory project identity, revision, variant count and reproducible seed. Persisted project schema and configuration imports are out of scope.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_PROJECT` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_PROJECT
#include "mulib-eda-project.h"
```

## Focused regression test

`tests/test_eda_project.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_project.c -lm -o /tmp/test_eda_project
/tmp/test_eda_project
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-project.h`](../include/mulib-eda-project.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-project.h
 * @brief Validated in-memory project identity, revision, variant count and reproducible seed.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_PROJECT in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_PROJECT_H
#define MULIB_EDA_PROJECT_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_project {
    char name[80], revision[32];
    uint32_t board_variants;
    uint64_t seed;
} mulib_eda_project_t;
int mulib_eda_project_init(mulib_eda_project_t *p, const char *name, const char *rev,
                           uint32_t variants, uint64_t seed);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_PROJECT_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-project.h`](../include/mulib-eda-project.h) |
| Implementation switch | `MULIB_IMPL_EDA_PROJECT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/eda-project.c`](../examples/modules/eda-project.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-project.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
