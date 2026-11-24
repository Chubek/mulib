# mulib-cdd-diff

ISO C11 single-header kernel. To compile this module with its dependencies in exactly one translation unit:

```c
#define MULIB_IMPL_CDD_CATALOGUE
#define MULIB_IMPL_CDD_DIFF
#include "mulib-cdd-diff.h"
```

Other translation units include the header without switches. See `examples/cdd-pipeline.c` and `tests/cdd-test.c`.

## API
`mulib_cdd_diff(before,after,out,capacity,needed)` validates both catalogues and compares entries by stable ID. It detects additions, removals and changes to category, implementation label, revision, cost, provided capabilities, requirements, conflicts or typed attribute values. Entry order does not matter; nested array order is intentionally significant. Unused value representation fields are ignored. Rename is remove + add.

A change stores kind and old/new indices; NONE means not applicable. Output order is old-catalogue removals/changes followed by new-catalogue additions. `needed` is required and receives the total count, including truncated records; LIMIT with nonzero `needed` means insufficient capacity. NULL output and zero capacity are valid for count discovery. On argument or catalogue validation failure, including LIMIT for an oversized catalogue, `needed` is zero if supplied and the output array is untouched. Output storage must not overlap the inputs or `needed`. No allocations or catalogue mutations. Diff is structural, not an ABI-compatibility classifier and not a patch generator.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-cdd-diff.h`](../kernels/mulib-cdd-diff.h) |
| Implementation switch | `MULIB_IMPL_CDD_DIFF` |
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
