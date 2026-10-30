# mqueue — System V interprocess queues

Define `MULIB_IMPL_MQUEUE`; no MuLIB dependencies.
See [`examples/modules/mqueue.c`](../examples/modules/mqueue.c).

`create` allocates a private-key mode-0600 queue and returns its native integer
ID. `send` copies one message with a positive long type; `receive` copies one
message to caller storage. Operations are nonblocking and retry EINTR. Payload
and receive capacity are capped at 1 MiB; native queue limits can be smaller.
Each operation allocates a temporary native message buffer and frees it before
returning. Zero-length messages are valid.

Receive selectors use `msgrcv` semantics: zero means FIFO; positive matches an
exact type; negative selects the lowest type <= its absolute value. LONG_MIN
is rejected. If the caller buffer is too small, TOOSMALL (-5) is returned and
the message remains queued. No silent truncation occurs. Counts reset on
failure; returned type changes only on success.

Results: 0 success, 1 AGAIN (empty receive/full send), -1 invalid, -2 OS error,
-3 unsupported platform, -4 allocation failure, -5 TOOSMALL. `remove` deletes
the queue and all pending messages. There is no implicit deletion or per-handle
close. This is System V IPC, not POSIX `mq_open`. Tests:
`tests/test_ipc_extensions.c`, including child-process communication.
