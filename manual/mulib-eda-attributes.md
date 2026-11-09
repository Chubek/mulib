# mulib-eda-attributes

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-attributes.h`  
**Implementation switch:** `MULIB_IMPL_EDA_ATTRIBUTES`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Owned typed attribute table with integer, finite real and bounded byte-string values, replacement, lookup and removal. No inheritance or schema-level validation.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_ATTRIBUTES` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_ATTRIBUTES
#include "mulib-eda-attributes.h"
```

## Focused regression test

`tests/test_eda_attributes.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_attributes.c -lm -o /tmp/test_eda_attributes
/tmp/test_eda_attributes
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-attributes.h`](../include/mulib-eda-attributes.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-attributes.h
 * @brief Owned typed attribute table with integer, finite real and bounded byte-string values,
 * replacement, lookup and removal.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_ATTRIBUTES in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_ATTRIBUTES_H
#define MULIB_EDA_ATTRIBUTES_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_eda_attr_type {
    MULIB_EDA_ATTR_INTEGER = 1,
    MULIB_EDA_ATTR_REAL = 2,
    MULIB_EDA_ATTR_STRING = 3
} mulib_eda_attr_type_t;
typedef struct mulib_eda_attr {
    uint64_t owner;
    char key[48];
    mulib_eda_attr_type_t type;
    int64_t integer;
    double real;
    char text[128];
} mulib_eda_attr_t;
typedef struct mulib_eda_attrs {
    mulib_eda_attr_t *data;
    size_t count, cap;
} mulib_eda_attrs_t;
int mulib_eda_attrs_set(mulib_eda_attrs_t *a, const mulib_eda_attr_t *value);
const mulib_eda_attr_t *mulib_eda_attrs_get(const mulib_eda_attrs_t *a, uint64_t owner,
                                            const char *key);
void mulib_eda_attrs_free(mulib_eda_attrs_t *a);
int mulib_eda_attrs_remove(mulib_eda_attrs_t *a, uint64_t owner, const char *key);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_ATTRIBUTES_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-attributes.h`](../include/mulib-eda-attributes.h) |
| Implementation switch | `MULIB_IMPL_EDA_ATTRIBUTES` |
| Repository-local includes | None |
| Complete example | [`examples/modules/eda-attributes.c`](../examples/modules/eda-attributes.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-attributes.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
