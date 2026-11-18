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
