# mulib-flow-graph

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-flow-graph.h`  
**Implementation switch:** `MULIB_IMPL_FLOW_GRAPH`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Deterministic topological sort and cycle detection on up to 64 jobs with dense adjacency. Execution, content hashes, hermeticity and retries are separate subsystems.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_FLOW_GRAPH` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_FLOW_GRAPH
#include "mulib-flow-graph.h"
```

## Focused regression test

`tests/test_flow_graph.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_flow_graph.c -lm -o /tmp/test_flow_graph
/tmp/test_flow_graph
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-flow-graph.h`](../include/mulib-flow-graph.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-flow-graph.h
 * @brief Deterministic topological sort and cycle detection on up to 64 jobs with dense adjacency.
 * @details C11 single-header implementation; define MULIB_IMPL_FLOW_GRAPH in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FLOW_GRAPH_H
#define MULIB_FLOW_GRAPH_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_flow_graph {
    size_t count;
    uint8_t edges[64][64];
} mulib_flow_graph_t;
int mulib_flow_graph_add(mulib_flow_graph_t *g, size_t count);
int mulib_flow_graph_depend(mulib_flow_graph_t *g, uint8_t prerequisite, uint8_t dependent);
int mulib_flow_graph_toposort(const mulib_flow_graph_t *g, uint8_t order[64]);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FLOW_GRAPH_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-flow-graph.h`](../include/mulib-flow-graph.h) |
| Implementation switch | `MULIB_IMPL_FLOW_GRAPH` |
| Repository-local includes | None |
| Complete example | [`examples/modules/flow-graph.c`](../examples/modules/flow-graph.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/flow-graph.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
