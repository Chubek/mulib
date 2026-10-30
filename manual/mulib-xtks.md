# xtks — X transport toolkit

Define `MULIB_IMPL_XTKS`; no dependencies. Here **xtks** means X transport
toolkit: a generic byte-stream abstraction, not the X11 protocol.
See [`examples/modules/xtks.c`](../examples/modules/xtks.c).

`mulib_xtks_t` borrows a context and read/write callbacks. There is no close
ownership, allocation, scheduling or implicit buffering. Read-only/write-only
streams are allowed; invoking a missing operation is invalid. Callbacks return
0 with positive progress, 1 AGAIN with zero count, 2 EOF with zero count for
reads only, or a negative error with zero count. Zero-length operations are
handled without invoking the backend. The checked wrappers reject oversized
counts, invalid statuses and success without progress, returning EIO (-2).
They reset public counts on errors.

`read_exact` and `write_all` loop over short operations and update a caller-owned
offset, initially zero. On AGAIN retain the same buffers, length, stream and
offset, then resume when ready. EOF may leave a partial read. The offset must
never exceed length. These helpers make progress on each iteration; a blocking
backend can still block, and timeout/cancellation policy belongs to that backend.
State and callbacks must be externally synchronized.

Tests: `tests/test_transport_extensions.c`.
