# containerstub

[Manuals](README.md) · [Library index](index.md)

Client request encoder for START/STOP/DELETE/STATUS. `request` validates container
ID through the shared identity model, then appends a frame whose payload is ID
bytes without NUL. Request ID is echoed by server replies. Returns 1 success,
0 invalid/OOM without partial append. STOP requests backend's code-zero action.

No connection, authentication, retry, RPC correlation store or process creation.
Caller supplies transport/security and decodes replies with the shared protocol.
Switches: `MULIB_IMPL_CONTAINERSTUB`, `MULIB_IMPL_OCICONTAINER`,
`MULIB_IMPL_CONTAINER2DAEMONPROTO`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`,
`MULIB_IMPL_CRC32`. Example: `examples/modules/containerstub.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-containerstub.h`](../include/mulib-containerstub.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-containerstub.h
 * @brief Client-side lifecycle-request encoder, with caller-supplied
 * transport. Define MULIB_IMPL_CONTAINERSTUB plus OCICONTAINER and
 * CONTAINER2DAEMONPROTO deps. */
#ifndef MULIB_CONTAINERSTUB_H
#define MULIB_CONTAINERSTUB_H
#include "mulib-container2daemonproto.h"
#include "mulib-ocicontainer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Encode START/STOP/DELETE/STATUS with ID bytes as payload. 1
   * success. No NUL terminator on wire. STOP requests the backend's code-zero
   * stop action. */
  int mulib_containerstub_request (mulib_buffer_t *out, unsigned opcode,
                                   uint64_t request_id,
                                   const char *container_id);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-containerstub.h`](../include/mulib-containerstub.h) |
| Implementation switch | `MULIB_IMPL_CONTAINERSTUB` |
| Repository-local includes | [mulib-container2daemonproto](mulib-container2daemonproto.md), [mulib-ocicontainer](mulib-ocicontainer.md) |
| Complete example | [`examples/modules/containerstub.c`](../examples/modules/containerstub.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/containerstub.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
