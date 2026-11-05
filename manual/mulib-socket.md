# socket

POSIX connected stream ownership. Initialize handles with `MULIB_SOCKET_INIT`.
`pair` creates a close-on-exec AF_UNIX pair; outputs unchanged on failure.
`nonblocking` controls O_NONBLOCK; `send`/`receive` perform one possibly short
operation (maximum 1 MiB), retry EINTR and report AGAIN distinctly from EOF.
Counts are zero on error; zero-capacity receive succeeds without implying EOF.

Results: OK 0, AGAIN 1, EOF 2, negative invalid/I/O/unsupported. Send suppresses
SIGPIPE using MSG_NOSIGNAL/SO_NOSIGPIPE, or returns unsupported rather than
silently changing global signal policy. `close` resets, is idempotent, and never
retries close after EINTR. No DNS, TLS, authentication or threads.
Switch: `MULIB_IMPL_SOCKET`. Example: `examples/modules/socket.c`.

`connect(out, numeric_ipv4, port)` creates a close-on-exec TCP connection with
blocking connection establishment. Port is 1..65535. `listen(out, numeric_ipv4,
port, &bound_port)` creates a close-on-exec nonblocking listener with backlog 32
and SO_REUSEADDR; port zero selects an ephemeral port. No implicit wildcard
address is chosen. `accept(listener, out)` returns AGAIN when no client is ready,
and otherwise a new close-on-exec stream (call nonblocking if desired). These
operations require initialized invalid output handles and transfer ownership
only on success; `bound_port` remains unchanged on error. They use the platform
IPv4 socket interface, not a third-party networking library. There is no timeout
for blocking connect or name resolution. TCP roundtrip setup is tested in
`tests/test_mumq.c`.
