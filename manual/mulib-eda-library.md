# mulib-eda-library

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-library.h`  
**Implementation switch:** `MULIB_IMPL_EDA_LIBRARY`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

In-memory part definition catalog with immutable key, description and pin count. Symbol geometry, footprints, PDKs and model families require separately versioned registries.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_LIBRARY` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_LIBRARY
#include "mulib-eda-library.h"
```

## Focused regression test

`tests/test_eda_library.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_library.c -lm -o /tmp/test_eda_library
/tmp/test_eda_library
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-library.h`](../include/mulib-eda-library.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-library.h
 * @brief In-memory part definition catalog with immutable key, description and pin count.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_LIBRARY in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_LIBRARY_H
#define MULIB_EDA_LIBRARY_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_partdef {
    char key[48], description[96];
    uint16_t pins;
} mulib_eda_partdef_t;
typedef struct mulib_eda_library {
    mulib_eda_partdef_t *parts;
    size_t count, cap;
} mulib_eda_library_t;
int mulib_eda_library_register(mulib_eda_library_t *lib, const mulib_eda_partdef_t *part);
const mulib_eda_partdef_t *mulib_eda_library_find(const mulib_eda_library_t *lib, const char *key);
void mulib_eda_library_free(mulib_eda_library_t *lib);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_LIBRARY_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-library.h`](../include/mulib-eda-library.h) |
| Implementation switch | `MULIB_IMPL_EDA_LIBRARY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/eda-library.c`](../examples/modules/eda-library.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-library.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
