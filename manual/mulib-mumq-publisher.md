# mulib-mumq-publisher

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
