# ocicontainer

[Manuals](README.md) · [Library index](index.md)

Container identity/lifecycle model, not a native container engine. `create`
validates/copies a 1..64 byte ASCII alnum/dot/underscore/hyphen ID, excluding
dot/dot-dot. IDs are never interpreted as host paths. CREATED may transition to
RUNNING or DELETED; RUNNING to STOPPED; STOPPED to DELETED. Deleted is terminal.
Stopped exit code must be 0..255; other transitions ignore it.

`can_transition` queries; `transition` changes only allowed state. All return
1 success/allowed, 0 invalid, preserving object on failure. No allocation or
implicit process/namespace/cgroup/mount changes. Runtime backend performs actions.
Switch: `MULIB_IMPL_OCICONTAINER`. Example: `examples/modules/ocicontainer.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ocicontainer.h`](../include/mulib-ocicontainer.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ocicontainer.h
 * @brief Compatibility entry point for the archived OCI lifecycle model.
 * Retained because public container adapters still depend on this API.
 * Define MULIB_IMPL_OCICONTAINER once; see manual/mulib-ocicontainer.md.
 */
#ifndef MULIB_OCICONTAINER_COMPAT_H
#define MULIB_OCICONTAINER_COMPAT_H
#include "../.deprecated/mulib-ocicontainer.h"
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ocicontainer.h`](../include/mulib-ocicontainer.h) |
| Implementation switch | `MULIB_IMPL_OCICONTAINER` |
| Repository-local includes | [`.deprecated/mulib-ocicontainer.h`](../.deprecated/mulib-ocicontainer.h) |
| Complete example | [`examples/modules/ocicontainer.c`](../examples/modules/ocicontainer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ocicontainer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
