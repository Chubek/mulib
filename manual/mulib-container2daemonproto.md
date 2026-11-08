# container2daemonproto

[Manuals](README.md) · [Library index](index.md)

Bounded transport-independent frame codec. Header: `MCDP`, version byte 1,
nonzero opcode byte, two zero reserved bytes, LE64 request ID, LE32 payload size;
then <=1 MiB payload and LE32 CRC32 of header+payload. Opcode is 1..255;
START/STOP/DELETE/STATUS are 1..4, IPC 16, response opcode=request|128.

`encode` appends atomically and permits input/output aliasing; 1 success, 0 error.
`decode`: 1 complete frame with borrowed payload and consumed count, 0 incomplete,
-1 invalid/version/CRC/limit. Extra stream bytes permitted; outputs unchanged
except on success. Request IDs are correlation tokens, not replay protection.
**CRC is not authentication**; caller supplies authenticated/authorized transport.
No sockets, encryption, peer verification, streaming reassembly or daemon.

Switches: `MULIB_IMPL_CONTAINER2DAEMONPROTO`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_CRC32`.
Example: `examples/modules/container2daemonproto.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-container2daemonproto.h`](../include/mulib-container2daemonproto.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-container2daemonproto.h
 * @brief Bounded versioned CRC framing for caller-transported container
 * messages. Define MULIB_IMPL_CONTAINER2DAEMONPROTO plus BUFFER, BYCUTILS and
 * CRC32. CRC detects accidents, not malicious modification; authentication is
 * external. */
#ifndef MULIB_CONTAINER2DAEMONPROTO_H
#define MULIB_CONTAINER2DAEMONPROTO_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_CONTAINER2DAEMONPROTO_START = 1,
    MULIB_CONTAINER2DAEMONPROTO_STOP = 2,
    MULIB_CONTAINER2DAEMONPROTO_DELETE = 3,
    MULIB_CONTAINER2DAEMONPROTO_STATUS = 4,
    MULIB_CONTAINER2DAEMONPROTO_IPC = 16,
    MULIB_CONTAINER2DAEMONPROTO_MAX_PAYLOAD = 1048576
  };
  typedef struct mulib_container2daemonproto_frame
  {
    unsigned opcode;
    uint64_t request_id;
    const unsigned char *payload;
    size_t size;
  } mulib_container2daemonproto_frame_t;
  /** @brief Append frame atomically; opcode 1..255, size <=1MiB. 1 success.
   * Input may alias output; protocol fields are little-endian. */
  int mulib_container2daemonproto_encode (mulib_buffer_t *out, unsigned opcode,
                                          uint64_t request_id,
                                          const void *payload, size_t size);
  /** @brief Decode one frame: 1 complete, 0 need more, -1
   * invalid/version/CRC/limit. Extra stream bytes permitted; consumed/output
   * unchanged except on success. Header is magic MCDP, version=1, opcode, two
   * reserved zero bytes, LE64 id, LE32 length; payload follows, then
   * CRC32(header+payload). */
  int
  mulib_container2daemonproto_decode (const void *data, size_t size,
                                      mulib_container2daemonproto_frame_t *out,
                                      size_t *consumed);
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
| Header | [`include/mulib-container2daemonproto.h`](../include/mulib-container2daemonproto.h) |
| Implementation switch | `MULIB_IMPL_CONTAINER2DAEMONPROTO` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-bycutils](mulib-bycutils.md), [mulib-crc32](mulib-crc32.md) |
| Complete example | [`examples/modules/container2daemonproto.c`](../examples/modules/container2daemonproto.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/container2daemonproto.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
