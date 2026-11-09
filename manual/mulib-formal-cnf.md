# mulib-formal-cnf

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-formal-cnf.h`  
**Implementation switch:** `MULIB_IMPL_FORMAL_CNF`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Bounded CNF storage for 32 variables, 512 clauses and 16 literals per clause, with structural validation and an AND-gate Tseitin encoder. Empty clauses encode contradiction. Failed additions preserve both clauses and variable count. The gate encoder requires three distinct positive variables.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_FORMAL_CNF` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_FORMAL_CNF
#include "mulib-formal-cnf.h"
```

## Focused regression test

`tests/test_formal_cnf.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_formal_cnf.c -lm -o /tmp/test_formal_cnf
/tmp/test_formal_cnf
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-formal-cnf.h`](../include/mulib-formal-cnf.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-formal-cnf.h
 * @brief Bounded CNF storage for 32 variables, 512 clauses and 16 literals per clause, with
 * structural validation and an AND-gate Tseitin encoder.
 * @details C11 single-header implementation; define MULIB_IMPL_FORMAL_CNF in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FORMAL_CNF_H
#define MULIB_FORMAL_CNF_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_FORMAL_MAX_VARS 32
#define MULIB_FORMAL_MAX_CLAUSES 512
#define MULIB_FORMAL_MAX_LITERALS 16
typedef struct mulib_formal_clause {
    int8_t literals[MULIB_FORMAL_MAX_LITERALS];
    uint8_t size;
} mulib_formal_clause_t;
typedef struct mulib_formal_cnf {
    uint8_t vars;
    size_t count;
    mulib_formal_clause_t clauses[MULIB_FORMAL_MAX_CLAUSES];
} mulib_formal_cnf_t;
int mulib_formal_cnf_add(mulib_formal_cnf_t *c, const int8_t *literals, size_t count);
int mulib_formal_cnf_validate(const mulib_formal_cnf_t *c);
int mulib_formal_cnf_and(mulib_formal_cnf_t *c, int8_t output, int8_t a, int8_t b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_FORMAL_CNF_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-formal-cnf.h`](../include/mulib-formal-cnf.h) |
| Implementation switch | `MULIB_IMPL_FORMAL_CNF` |
| Repository-local includes | None |
| Complete example | [`examples/modules/formal-cnf.c`](../examples/modules/formal-cnf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/formal-cnf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
