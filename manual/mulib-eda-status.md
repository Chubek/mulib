# mulib-eda-status

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-status.h`  
**Implementation switch:** `MULIB_IMPL_EDA_STATUS`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Shared status names and bounded diagnostic record with line/column context. No dynamic allocation, no global state.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_STATUS` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_STATUS
#include "mulib-eda-status.h"
```

## Focused regression test

`tests/test_eda_status.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_status.c -lm -o /tmp/test_eda_status
/tmp/test_eda_status
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-status.h`](../include/mulib-eda-status.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-status.h
 * @brief Shared status names and bounded diagnostic record with line/column context.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_STATUS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_STATUS_H
#define MULIB_EDA_STATUS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_eda_code {
    MULIB_EDA_OK = 0,
    MULIB_EDA_INVALID = -1,
    MULIB_EDA_NOMEM = -2,
    MULIB_EDA_RANGE = -3,
    MULIB_EDA_NOTFOUND = -4,
    MULIB_EDA_CONFLICT = -5,
    MULIB_EDA_SINGULAR = -6,
    MULIB_EDA_UNSUPPORTED = -7
} mulib_eda_code_t;
typedef struct mulib_eda_diagnostic {
    mulib_eda_code_t code;
    size_t offset, line, column;
    char message[192];
} mulib_eda_diagnostic_t;
const char *mulib_eda_code_name(mulib_eda_code_t code);
void mulib_eda_diagnostic_set(mulib_eda_diagnostic_t *d, mulib_eda_code_t code, size_t line,
                              size_t column, const char *msg);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_STATUS_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-status.h`](../include/mulib-eda-status.h) |
| Implementation switch | `MULIB_IMPL_EDA_STATUS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/eda-status.c`](../examples/modules/eda-status.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-status.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
