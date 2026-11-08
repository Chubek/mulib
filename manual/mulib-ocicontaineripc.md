# ocicontaineripc

[Manuals](README.md) · [Library index](index.md)

Binary channel messages over shared protocol frames. `send` uses IPC opcode and
request-ID field as uint64 channel. Payload may contain NUL, <=1 MiB. Returns 1
success, 0 invalid/OOM. `receive` requires IPC opcode and returns borrowed payload,
channel, length and consumed size only on success: 1 frame, 0 incomplete, -1
invalid/corrupt/wrong opcode. Input must outlive payload use.

No native IPC namespace, message queue, socket, authorization or encryption.
Switches: `MULIB_IMPL_OCICONTAINERIPC`, `MULIB_IMPL_CONTAINER2DAEMONPROTO`,
`MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_CRC32`.
Example: `examples/modules/ocicontaineripc.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ocicontaineripc.h`](../include/mulib-ocicontaineripc.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ocicontaineripc.h
 * @brief Compatibility entry point for archived OCI IPC framing.
 * Define MULIB_IMPL_OCICONTAINERIPC and dependencies once; see its manual.
 */
#ifndef MULIB_OCICONTAINERIPC_COMPAT_H
#define MULIB_OCICONTAINERIPC_COMPAT_H
#include "mulib-container2daemonproto.h"
#include "../.deprecated/mulib-ocicontaineripc.h"
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ocicontaineripc.h`](../include/mulib-ocicontaineripc.h) |
| Implementation switch | `MULIB_IMPL_OCICONTAINERIPC` |
| Repository-local includes | [`.deprecated/mulib-ocicontaineripc.h`](../.deprecated/mulib-ocicontaineripc.h), [mulib-container2daemonproto](mulib-container2daemonproto.md) |
| Complete example | [`examples/modules/ocicontaineripc.c`](../examples/modules/ocicontaineripc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ocicontaineripc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
