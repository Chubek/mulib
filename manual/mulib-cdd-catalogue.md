# mulib-cdd-catalogue

ISO C11 single-header kernel. To compile this module with its dependencies in exactly one translation unit:

```c
#define MULIB_IMPL_CDD_CATALOGUE
#include "mulib-cdd-catalogue.h"
```

Other translation units include the header without switches. See `examples/cdd-pipeline.c` and `tests/cdd-test.c`.

## Data model
`mulib_cdd_catalogue_t` is a view of up to 64 `mulib_cdd_entry_t` records. An entry has a unique ID, category, implementation label, positive component revision, unsigned cost, provided capabilities, requirements, conflicts and attributes. All arrays have an explicit count. Empty arrays may use NULL. The host owns all storage and must keep it alive and immutable while views, plans or executions use it. Validation checks descriptor structure, not pointer provenance or allocation sizes: each nonempty array must have at least its declared number of elements, and strings must be NUL-terminated. Do not pass serialized pointers. Implementation labels are descriptive only, never executed directly.

`mulib_cdd_cap_t` names a capability and positive integer revision; `mulib_cdd_req_t` represents inclusive `min_revision..max_revision`, with zero maximum meaning unbounded. Integer, boolean (0 or 1), and string values are supported; unused value fields are ignored. IDs, category names, capability names and attribute keys use ASCII letters/digits plus `_-. :` (without spaces). Conflicts are symmetric even when declared on only one side; unknown conflict IDs are permitted to support partial catalogues.

## API
- `mulib_cdd_validate(c, diag)` checks arrays, identity, uniqueness, revisions and typed attributes; reports the first error with entry and item indices and a borrowed field name. `MULIB_CDD_NONE` means no applicable index. It does not prove satisfiability or require all references to exist.
- `mulib_cdd_find(c,id)` returns exact ID index or NONE. An oversized catalogue is not searched. `mulib_cdd_attr_get(e,key)` returns a borrowed attribute or NULL.
- `mulib_cdd_name_valid`, `mulib_cdd_value_valid`, `mulib_cdd_req_valid` check individual inputs. `mulib_cdd_cap_matches`, `mulib_cdd_entry_provides`, `mulib_cdd_entries_conflict` implement exact matching and conflict checking. Entry-level helpers assume structurally validated entries; never pass malformed pointer/count pairs.

Status values: `OK=0`, `INVALID`, `DUPLICATE`, `NOT_FOUND`, `CONFLICT`, `UNSAT`, `LIMIT`, `NOMEM`, `CALLBACK` are negative. No API sets errno as a contract. No memory allocation occurs in this kernel.
