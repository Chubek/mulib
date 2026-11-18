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
