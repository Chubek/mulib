# mulib-mumq-subscriber

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
