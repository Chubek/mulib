# filectl — descriptor flags and locks

Define `MULIB_IMPL_FILECTL`; no dependencies.
See [`examples/modules/filectl.c`](../examples/modules/filectl.c).

Descriptors are borrowed. `nonblocking(fd, enabled)` changes only O_NONBLOCK;
`cloexec(fd, enabled)` changes only FD_CLOEXEC, preserving other flags. Enabled
must be 0 or 1. These are read/modify/write operations, so callers must avoid
concurrent flag modifications. Setting close-on-exec after opening is not an
atomic open-with-CLOEXEC operation.

`lock(fd, mode)` requests a nonblocking whole-file POSIX record lock with
`fcntl(F_SETLK)`: 0 unlock, 1 shared/read, 2 exclusive/write. Conflicts return
BUSY (1), not an exception. Locks are process-associated, are not inherited by
forked children, and closing any descriptor for that file can release the
process's locks. They are not thread mutexes or open-file-description locks.

All functions return 0 success, -1 invalid, -2 OS error, -3 unsupported. Only
lock returns BUSY. No ownership transfer, allocation, or blocking lock waits.
POSIX backends preserve errno on OS failures. Tests: `tests/test_ipc_extensions.c`
uses a child process to verify lock exclusion and checks native descriptor flags.
