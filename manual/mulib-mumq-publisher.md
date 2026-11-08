# mulib-mumq-publisher

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_MUMQ_PUBLISHER`, `MULIB_IMPL_MUMQ`, `MULIB_IMPL_SOCKET`,
`MULIB_IMPL_AES`, `MULIB_IMPL_AUTHCRYPTO` once.
`mulib_mumq_publish(channel, topic, bytes, length)` copies a PUBLISH frame into
the borrowed channel's bounded queue. It returns zero, AGAIN (queue full, no
mutation), or a negative MuMQ error. It never flushes, closes the channel,
creates a socket, or confirms remote delivery. NULL bytes are valid at zero
length. Payload is binary, <=1 MiB, topic is <=255 validated ASCII bytes.

Use `mulib_mumq_flush` until drained; preserve the channel on AGAIN. Broker
failure can lose enqueued messages. Reliable applications must implement
application-level request IDs, results and retries (as distribcomp does).
`examples/modules/mumq-publisher.c` runs an actual broker/publisher/subscriber
roundtrip. See `mulib-mumq.md` for wire format and security boundaries.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-mumq-publisher.h`](../include/mulib-mumq-publisher.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-mumq-publisher.h
 * @brief MuMQ exact-topic publisher convenience API.
 * Define MULIB_IMPL_MUMQ_PUBLISHER plus MUMQ dependencies once.
 */
#ifndef MULIB_MUMQ_PUBLISHER_H
#define MULIB_MUMQ_PUBLISHER_H
#include "mulib-mumq.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Copy a publication to a borrowed channel's bounded output queue. */
int mulib_mumq_publish(mulib_mumq_t *, const char *topic, const void *data, size_t length);
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
| Header | [`include/mulib-mumq-publisher.h`](../include/mulib-mumq-publisher.h) |
| Implementation switch | `MULIB_IMPL_MUMQ_PUBLISHER` |
| Repository-local includes | [mulib-mumq](mulib-mumq.md) |
| Complete example | [`examples/modules/mumq-publisher.c`](../examples/modules/mumq-publisher.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mumq-publisher.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
