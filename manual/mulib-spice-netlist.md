# mulib-spice-netlist

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-spice-netlist.h`  
**Implementation switch:** `MULIB_IMPL_SPICE_NETLIST`  
**Dependencies:** `spice-lexer`.

## Purpose and supported profile

Transactional parser for four passive/independent-device card types: R, C, V, I with 2 nodes and one numeric DC value. Comments, .end, SPICE number suffixes. Maximum 128 non-ground nodes, 1024 components, line length 511. No subcircuits, model cards, continuation, AC, PULSE, or behavioral expressions.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_SPICE_NETLIST` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_SPICE_NETLIST
#include "mulib-spice-netlist.h"
```

## Focused regression test

`tests/test_spice_netlist.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_spice_netlist.c -lm -o /tmp/test_spice_netlist
/tmp/test_spice_netlist
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-spice-netlist.h`](../include/mulib-spice-netlist.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-spice-netlist.h
 * @brief Transactional parser for four passive/independent-device card types: R, C, V, I with 2
 * nodes and one numeric DC value.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_NETLIST in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_NETLIST_H
#define MULIB_SPICE_NETLIST_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-spice-lexer.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SPICE_MAX_NODES 128
#define MULIB_SPICE_MAX_PARTS 1024
typedef struct mulib_spice_part {
    char name[32];
    char kind;
    uint16_t a, b;
    double value;
} mulib_spice_part_t;
typedef struct mulib_spice_netlist {
    size_t nodes, parts;
    char node_names[MULIB_SPICE_MAX_NODES][32];
    mulib_spice_part_t parts_data[MULIB_SPICE_MAX_PARTS];
} mulib_spice_netlist_t;
int mulib_spice_netlist_parse(const char *source, mulib_spice_netlist_t *out, size_t *error_line);
int mulib_spice_netlist_find_node(const mulib_spice_netlist_t *net, const char *name);
int mulib_spice_netlist_valid(const mulib_spice_netlist_t *net);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_NETLIST_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-spice-netlist.h`](../include/mulib-spice-netlist.h) |
| Implementation switch | `MULIB_IMPL_SPICE_NETLIST` |
| Repository-local includes | [mulib-spice-lexer](mulib-spice-lexer.md) |
| Complete example | [`examples/modules/spice-netlist.c`](../examples/modules/spice-netlist.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/spice-netlist.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
