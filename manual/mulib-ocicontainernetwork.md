# ocicontainernetwork

[Manuals](README.md) · [Library index](index.md)

IPv4 address/prefix/optional gateway plan. `init` uses strict shared IPv4 literal
parsing, prefix 0..32; a supplied gateway must belong to the subnet and differ
from the address. Returns 1 success, 0 invalid, unchanged output on error. Value
object owns no allocations. Does not reject every subnet-specific reserved host
address; a real network backend must enforce its address policy.

No interfaces, bridges, NAT, DNS, DHCP, namespace changes or network privileges.
Switches: `MULIB_IMPL_OCICONTAINERNETWORK`, `MULIB_IMPL_INET4`, `MULIB_IMPL_PARSEC`.
Example: `examples/modules/ocicontainernetwork.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ocicontainernetwork.h`](../include/mulib-ocicontainernetwork.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ocicontainernetwork.h
 * @brief Compatibility entry point for archived OCI network policies.
 * Define MULIB_IMPL_OCICONTAINERNETWORK and dependencies once; see its manual.
 */
#ifndef MULIB_OCICONTAINERNETWORK_COMPAT_H
#define MULIB_OCICONTAINERNETWORK_COMPAT_H
#include "mulib-inet4.h"
#include "../.deprecated/mulib-ocicontainernetwork.h"
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ocicontainernetwork.h`](../include/mulib-ocicontainernetwork.h) |
| Implementation switch | `MULIB_IMPL_OCICONTAINERNETWORK` |
| Repository-local includes | [`.deprecated/mulib-ocicontainernetwork.h`](../.deprecated/mulib-ocicontainernetwork.h), [mulib-inet4](mulib-inet4.md) |
| Complete example | [`examples/modules/ocicontainernetwork.c`](../examples/modules/ocicontainernetwork.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ocicontainernetwork.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
