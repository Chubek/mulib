# mulib-qa-repro

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-qa-repro.h`  
**Implementation switch:** `MULIB_IMPL_QA_REPRO`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Platform-independent FNV-1a fingerprint for a reproducibility record (seed, feature mask, compiler and target strings). Hash is not cryptographic authentication and does not include all build artifacts.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_QA_REPRO` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_QA_REPRO
#include "mulib-qa-repro.h"
```

## Focused regression test

`tests/test_qa_repro.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_qa_repro.c -lm -o /tmp/test_qa_repro
/tmp/test_qa_repro
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-qa-repro.h`](../include/mulib-qa-repro.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-qa-repro.h
 * @brief Platform-independent FNV-1a fingerprint for a reproducibility record (seed, feature mask,
 * compiler and target strings).
 * @details C11 single-header implementation; define MULIB_IMPL_QA_REPRO in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_QA_REPRO_H
#define MULIB_QA_REPRO_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_qa_repro {
    uint64_t seed;
    uint32_t version, features;
    char compiler[64], target[64];
} mulib_qa_repro_t;
int mulib_qa_repro_init(mulib_qa_repro_t *r, uint64_t seed, uint32_t features, const char *compiler,
                        const char *target);
uint64_t mulib_qa_repro_hash(const mulib_qa_repro_t *r);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_QA_REPRO_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-qa-repro.h`](../include/mulib-qa-repro.h) |
| Implementation switch | `MULIB_IMPL_QA_REPRO` |
| Repository-local includes | None |
| Complete example | [`examples/modules/qa-repro.c`](../examples/modules/qa-repro.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/qa-repro.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
