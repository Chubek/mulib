# ocicontainernetwrork

[Manuals](README.md) · [Library index](index.md)

Compatibility spelling retained exactly as requested. The type aliases
`mulib_ocicontainernetwork_t`; `init` forwards to canonical IPv4 plan validation.
New code should prefer [ocicontainernetwork](mulib-ocicontainernetwork.md). Both
headers may coexist; implementation switches remain explicit and distinct.

No network changes/isolation are performed. Returns 1 valid, 0 invalid.
Switches: `MULIB_IMPL_OCICONTAINERNETWRORK`, `MULIB_IMPL_OCICONTAINERNETWORK`,
`MULIB_IMPL_INET4`, `MULIB_IMPL_PARSEC`.
Example: `examples/modules/ocicontainernetwrork.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ocicontainernetwrork.h`](../include/mulib-ocicontainernetwrork.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ocicontainernetwrork.h
 * @brief Compatibility entry point for the legacy OCI network spelling.
 * Define MULIB_IMPL_OCICONTAINERNETWRORK and dependencies once; see its manual.
 */
#ifndef MULIB_OCICONTAINERNETWRORK_COMPAT_H
#define MULIB_OCICONTAINERNETWRORK_COMPAT_H
#include "mulib-ocicontainernetwork.h"
#include "../.deprecated/mulib-ocicontainernetwrork.h"
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ocicontainernetwrork.h`](../include/mulib-ocicontainernetwrork.h) |
| Implementation switch | `MULIB_IMPL_OCICONTAINERNETWRORK` |
| Repository-local includes | [`.deprecated/mulib-ocicontainernetwrork.h`](../.deprecated/mulib-ocicontainernetwrork.h), [mulib-ocicontainernetwork](mulib-ocicontainernetwork.md) |
| Complete example | [`examples/modules/ocicontainernetwrork.c`](../examples/modules/ocicontainernetwrork.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ocicontainernetwrork.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
