# mulib-eda-netgraph

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-netgraph.h`  
**Implementation switch:** `MULIB_IMPL_EDA_NETGRAPH`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Append-only connectivity hypergraph with indexed nets, terminals, explicit disconnection, net merging and deterministic degrees. Merging moves terminals to the target net and leaves the source net allocated but empty.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_NETGRAPH` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_NETGRAPH
#include "mulib-eda-netgraph.h"
```

## Focused regression test

`tests/test_eda_netgraph.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_netgraph.c -lm -o /tmp/test_eda_netgraph
/tmp/test_eda_netgraph
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-netgraph.h`](../include/mulib-eda-netgraph.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-netgraph.h
 * @brief Append-only connectivity hypergraph with indexed nets, terminals, explicit disconnection,
 * net merging and deterministic degrees.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_NETGRAPH in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_NETGRAPH_H
#define MULIB_EDA_NETGRAPH_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_terminal {
    uint32_t net;
    uint16_t kind;
} mulib_eda_terminal_t;
typedef struct mulib_eda_netgraph {
    mulib_eda_terminal_t *term;
    size_t count, capacity;
    uint32_t nets;
} mulib_eda_netgraph_t;
int mulib_eda_netgraph_net(mulib_eda_netgraph_t *g, uint32_t *id);
int mulib_eda_netgraph_terminal(mulib_eda_netgraph_t *g, uint16_t kind, uint32_t *id);
int mulib_eda_netgraph_connect(mulib_eda_netgraph_t *g, uint32_t terminal, uint32_t net);
size_t mulib_eda_netgraph_degree(const mulib_eda_netgraph_t *g, uint32_t net);
void mulib_eda_netgraph_free(mulib_eda_netgraph_t *g);
int mulib_eda_netgraph_merge(mulib_eda_netgraph_t *g, uint32_t target, uint32_t source);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_NETGRAPH_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-netgraph.h`](../include/mulib-eda-netgraph.h) |
| Implementation switch | `MULIB_IMPL_EDA_NETGRAPH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/eda-netgraph.c`](../examples/modules/eda-netgraph.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-netgraph.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
