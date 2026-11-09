# mulib-ecad-pcb-sync

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-ecad-pcb-sync.h`  
**Implementation switch:** `MULIB_IMPL_ECAD_PCB_SYNC`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Stable bijective object-ID lookup between 256 schematic and PCB objects; no object model introspection, reconciliation or automatic synchronization.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_ECAD_PCB_SYNC` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_ECAD_PCB_SYNC
#include "mulib-ecad-pcb-sync.h"
```

## Focused regression test

`tests/test_ecad_pcb_sync.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_ecad_pcb_sync.c -lm -o /tmp/test_ecad_pcb_sync
/tmp/test_ecad_pcb_sync
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ecad-pcb-sync.h`](../include/mulib-ecad-pcb-sync.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ecad-pcb-sync.h
 * @brief Stable bijective object-ID lookup between 256 schematic and PCB objects; no object model
 * introspection, reconciliation or automatic synchronization.
 * @details C11 single-header implementation; define MULIB_IMPL_ECAD_PCB_SYNC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ECAD_PCB_SYNC_H
#define MULIB_ECAD_PCB_SYNC_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_ecad_pcb_link {
    uint64_t schematic_id, pcb_id;
} mulib_ecad_pcb_link_t;
typedef struct mulib_ecad_pcb_sync {
    mulib_ecad_pcb_link_t links[256];
    size_t count;
} mulib_ecad_pcb_sync_t;
int mulib_ecad_pcb_sync_add(mulib_ecad_pcb_sync_t *s, uint64_t schematic_id, uint64_t pcb_id);
uint64_t mulib_ecad_pcb_sync_lookup(const mulib_ecad_pcb_sync_t *s, uint64_t schematic_id);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ECAD_PCB_SYNC_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ecad-pcb-sync.h`](../include/mulib-ecad-pcb-sync.h) |
| Implementation switch | `MULIB_IMPL_ECAD_PCB_SYNC` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ecad-pcb-sync.c`](../examples/modules/ecad-pcb-sync.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ecad-pcb-sync.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
