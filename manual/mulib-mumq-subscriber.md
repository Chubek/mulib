# mulib-mumq-subscriber

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_MUMQ_SUBSCRIBER`, `MULIB_IMPL_MUMQ`, `MULIB_IMPL_SOCKET`,
`MULIB_IMPL_AES`, `MULIB_IMPL_AUTHCRYPTO` once.
`mulib_mumq_subscribe(channel, topic)` queues an empty SUBSCRIBE. Zero means
queued, not active. Flush, advance the broker, and receive until
`mulib_mumq_subscribed(&message, topic)` recognizes the matching SUBACK.
Free every received message with `mulib_mumq_message_free`.

One exact-match topic per broker connection; another successful subscription
replaces it. Use multiple channels for multiple topics. No wildcards, retained
messages, implicit unsubscription or offline persistence. Publications sent
before SUBACK may be lost; requests that need delivery guarantees require
application-level retries. Channels are borrowed and remain caller-owned.
AGAIN and errors follow `mulib-mumq.md`. Runnable exchange:
`examples/modules/mumq-subscriber.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-mumq-subscriber.h`](../include/mulib-mumq-subscriber.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-mumq-subscriber.h
 * @brief Acknowledged exact-topic MuMQ subscriptions.
 * Define MULIB_IMPL_MUMQ_SUBSCRIBER plus MUMQ dependencies once.
 */
#ifndef MULIB_MUMQ_SUBSCRIBER_H
#define MULIB_MUMQ_SUBSCRIBER_H
#include "mulib-mumq.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Queue a subscription. Each broker connection has one replaceable topic.
 * Wait for matching SUBACK before relying on delivery. */
int mulib_mumq_subscribe(mulib_mumq_t *, const char *topic);
/** Whether a received message acknowledges this topic. Does not free it. */
int mulib_mumq_subscribed(const mulib_mumq_message_t *, const char *topic);
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
| Header | [`include/mulib-mumq-subscriber.h`](../include/mulib-mumq-subscriber.h) |
| Implementation switch | `MULIB_IMPL_MUMQ_SUBSCRIBER` |
| Repository-local includes | [mulib-mumq](mulib-mumq.md) |
| Complete example | [`examples/modules/mumq-subscriber.c`](../examples/modules/mumq-subscriber.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mumq-subscriber.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
