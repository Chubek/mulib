# ocicontainerruntime

[Manuals](README.md) · [Library index](index.md)

Explicit-backend lifecycle driver. `init` copies ID and requires an action callback
plus borrowed context. `apply` validates transition/exit code, calls backend with
current state/desired state, then publishes new state only on callback success.
Returns 0 success, -1 invalid, or backend's nonzero result. No automatic callbacks
on object destruction and no implicit backend implementation.

Backend/context must outlive runtime; no reentrancy or runtime mutation during
callback. Callback failure must leave backend state unchanged. Real backend must
perform isolation, process supervision, privileges, mounts, namespaces/cgroups
and security policy. This module alone is **not a usable isolated OCI runtime**.
Switches: `MULIB_IMPL_OCICONTAINERRUNTIME`, `MULIB_IMPL_OCICONTAINER`.
Example: `examples/modules/ocicontainerruntime.c` uses a test-only backend.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ocicontainerruntime.h`](../include/mulib-ocicontainerruntime.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ocicontainerruntime.h
 * @brief Compatibility entry point for archived OCI runtime adapters.
 * Define MULIB_IMPL_OCICONTAINERRUNTIME and dependencies once; see its manual.
 */
#ifndef MULIB_OCICONTAINERRUNTIME_COMPAT_H
#define MULIB_OCICONTAINERRUNTIME_COMPAT_H
#include "mulib-ocicontainer.h"
#include "../.deprecated/mulib-ocicontainerruntime.h"
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ocicontainerruntime.h`](../include/mulib-ocicontainerruntime.h) |
| Implementation switch | `MULIB_IMPL_OCICONTAINERRUNTIME` |
| Repository-local includes | [`.deprecated/mulib-ocicontainerruntime.h`](../.deprecated/mulib-ocicontainerruntime.h), [mulib-ocicontainer](mulib-ocicontainer.md) |
| Complete example | [`examples/modules/ocicontainerruntime.c`](../examples/modules/ocicontainerruntime.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ocicontainerruntime.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
