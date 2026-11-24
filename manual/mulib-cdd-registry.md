# mulib-cdd-registry

ISO C11 single-header kernel. To compile this module with its dependencies in exactly one translation unit:

```c
#define MULIB_IMPL_CDD_CATALOGUE
#define MULIB_IMPL_CDD_REGISTRY
#include "mulib-cdd-registry.h"
```

Other translation units include the header without switches. See `examples/cdd-pipeline.c` and `tests/cdd-test.c`.

## API
- `mulib_cdd_registry_init(r)` initializes an empty caller-owned registry.
- `mulib_cdd_registry_add(r,e)` shallow-copies a descriptor into fixed storage and validates the prospective catalogue. On failure the visible count and prior records remain unchanged; nested strings/arrays remain borrowed. Duplicate IDs are rejected.
- `mulib_cdd_registry_remove(r,id)` removes by exact ID and compacts indices. Returns NOT_FOUND if absent.
- `mulib_cdd_registry_view(r)` returns a borrowed view; NULL yields an empty view.

Maximum capacity is 64; LIMIT indicates a full registry. No dynamic allocation, destructor or implicit copy of nested records. Registry mutation invalidates all associated views and plans; do not mutate during queries or execution. The caller synchronizes concurrent access.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-cdd-registry.h`](../kernels/mulib-cdd-registry.h) |
| Implementation switch | `MULIB_IMPL_CDD_REGISTRY` |
| Repository-local includes | [mulib-cdd-catalogue](mulib-cdd-catalogue.md) |
| Complete example | [`examples/cdd-pipeline.c`](../examples/cdd-pipeline.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/cdd-pipeline.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
