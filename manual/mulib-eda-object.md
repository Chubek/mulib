# mulib-eda-object

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-object.h`  
**Implementation switch:** `MULIB_IMPL_EDA_OBJECT`  
**Dependencies:** `eda-ids`.

## Purpose and supported profile

Small immutable-ID object header with checked type/name, monotonic revision on rename; object persistence and schema registry are outside scope.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_OBJECT` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_OBJECT
#include "mulib-eda-object.h"
```

## Focused regression test

`tests/test_eda_object.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_object.c -lm -o /tmp/test_eda_object
/tmp/test_eda_object
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-object.h`](../include/mulib-eda-object.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-object.h
 * @brief Small immutable-ID object header with checked type/name, monotonic revision on rename;
 * object persistence and schema registry are outside scope.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_OBJECT in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_OBJECT_H
#define MULIB_EDA_OBJECT_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-eda-ids.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_object {
    mulib_eda_id_t id;
    uint32_t revision, flags;
    uint16_t type;
    char name[64];
} mulib_eda_object_t;
int mulib_eda_object_init(mulib_eda_object_t *o, mulib_eda_id_t id, uint16_t type,
                          const char *name);
int mulib_eda_object_rename(mulib_eda_object_t *o, const char *name);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_OBJECT_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-object.h`](../include/mulib-eda-object.h) |
| Implementation switch | `MULIB_IMPL_EDA_OBJECT` |
| Repository-local includes | [mulib-eda-ids](mulib-eda-ids.md) |
| Complete example | [`examples/modules/eda-object.c`](../examples/modules/eda-object.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-object.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
