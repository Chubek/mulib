# containerserverd

[Manuals](README.md) · [Library index](index.md)

Single-runtime request dispatcher, not a running daemon/server. Caller must
authenticate/authorize transport **before** `dispatch`. Requires exactly one
complete START/STOP/DELETE/STATUS frame whose payload matches runtime ID.
Replies echo request ID, opcode|128, payload JSON state. STOP invokes code-zero
backend action; actual supervision/exit observation belongs to backend.

Response allocation precedes backend action; failure restores response length
and does not publish lifecycle state. Backend must obey its failure-atomic
contract. Returns 0 success, -1 malformed/unsupported/mismatch, -2 allocation,
or nonzero backend error. No listener, process, thread, permissions or multi-ID
registry. Response and request may share storage.

Switches: `MULIB_IMPL_CONTAINERSERVERD`, `MULIB_IMPL_OCICONTAINERRUNTIME`,
`MULIB_IMPL_OCICONTAINERMETADATA`, `MULIB_IMPL_OCICONTAINER`,
`MULIB_IMPL_CONTAINER2DAEMONPROTO`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`,
`MULIB_IMPL_CRC32`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`,
`MULIB_IMPL_UTF8`. Example: `examples/modules/containerserverd.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-containerserverd.h`](../include/mulib-containerserverd.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-containerserverd.h
 * @brief Single-container request dispatcher; no listener, threads or implicit
 * daemon. Define MULIB_IMPL_CONTAINERSERVERD plus runtime/metadata/protocol
 * dependencies. Caller MUST authenticate/authorize transport before dispatch.
 */
#ifndef MULIB_CONTAINERSERVERD_H
#define MULIB_CONTAINERSERVERD_H
#include "mulib-container2daemonproto.h"
#include "mulib-ocicontainermetadata.h"
#include "mulib-ocicontainerruntime.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Handle exactly one complete START/STOP/DELETE/STATUS frame for
   * runtime ID. Response opcode=request|128, ID echoed, payload state JSON. 0
   * success, -1 malformed/unsupported/ID mismatch, -2 allocation, other
   * backend errors. Allocation precedes backend action; failures leave
   * response bytes/state unchanged. STOP invokes backend with code zero;
   * actual process supervision belongs to backend. */
  int mulib_containerserverd_dispatch (mulib_ocicontainerruntime_t *runtime,
                                       const void *request, size_t size,
                                       mulib_buffer_t *response);
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
| Header | [`include/mulib-containerserverd.h`](../include/mulib-containerserverd.h) |
| Implementation switch | `MULIB_IMPL_CONTAINERSERVERD` |
| Repository-local includes | [mulib-container2daemonproto](mulib-container2daemonproto.md), [mulib-ocicontainermetadata](mulib-ocicontainermetadata.md), [mulib-ocicontainerruntime](mulib-ocicontainerruntime.md) |
| Complete example | [`examples/modules/containerserverd.c`](../examples/modules/containerserverd.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/containerserverd.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
