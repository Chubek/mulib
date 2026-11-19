# mulib-cdd-schema

ISO C11 single-header kernel. To compile this module with its dependencies in exactly one translation unit:

```c
#define MULIB_IMPL_CDD_CATALOGUE
#define MULIB_IMPL_CDD_SCHEMA
#include "mulib-cdd-schema.h"
```

Other translation units include the header without switches. See `examples/cdd-pipeline.c` and `tests/cdd-test.c`.

## API
`mulib_cdd_schema_validate(c,s,diag)` checks structural catalogue validity and a category-scoped metadata schema. NULL category matches all. Fields have a key, expected kind, required flag, and optional inclusive integer range. `closed` rejects attributes not present in the schema. Optional fields are checked when present. Duplicate schema keys, invalid ranges or non-integer range declarations are rejected. Empty matching sets pass vacuously.

On a missing required attribute the status is NOT_FOUND; on mismatch or unknown closed-schema field it is INVALID. Diagnostics identify the catalogue entry and field (item is schema-field index except unknown attributes, where it is attribute index). Schema descriptors and diagnostics borrow strings from their input. No allocation or data mutation occurs.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-cdd-schema.h`](../include/mulib-cdd-schema.h) |
| Implementation switch | `MULIB_IMPL_CDD_SCHEMA` |
| Repository-local includes | [mulib-cdd-catalogue](mulib-cdd-catalogue.md) |
| Complete example | [`examples/cdd-pipeline.c`](../examples/cdd-pipeline.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/cdd-pipeline.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
