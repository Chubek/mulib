# socket

POSIX connected stream ownership. Initialize handles with `MULIB_SOCKET_INIT`.
`pair` creates a close-on-exec AF_UNIX pair; outputs unchanged on failure.
`nonblocking` controls O_NONBLOCK; `send`/`receive` perform one possibly short
operation (maximum 1 MiB), retry EINTR and report AGAIN distinctly from EOF.
Counts are zero on error; zero-capacity receive succeeds without implying EOF.

Results: OK 0, AGAIN 1, EOF 2, negative invalid/I/O/unsupported. Send suppresses
SIGPIPE using MSG_NOSIGNAL/SO_NOSIGPIPE, or returns unsupported rather than
silently changing global signal policy. `close` resets, is idempotent, and never
retries close after EINTR. No DNS/TCP connect/listen, TLS, authentication or threads.
Switch: `MULIB_IMPL_SOCKET`. Example: `examples/modules/socket.c`.
