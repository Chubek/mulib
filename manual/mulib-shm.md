# shm — System V shared memory

Define `MULIB_IMPL_SHM`; no MuLIB dependencies.
See [`examples/modules/shm.c`](../examples/modules/shm.c).

Initialize each handle with `MULIB_SHM_INIT`. `create` allocates a nonempty,
private-key, mode-0600 System V segment and attaches it read/write. Share its
integer ID explicitly with cooperating processes. `attach` discovers the
segment size and attaches read/write or read-only. Outputs remain unchanged on
failure; failed creation removes the new segment. Live handles must not be
copied or reinitialized. Read-only mappings must never be written.

`remove(id)` explicitly marks the segment for deletion after the final detach.
Existing mappings remain valid. `close` detaches and resets the local handle
without deleting the segment; it is idempotent and retains state if detach
fails. Applications must arrange removal, including failure paths, because
System V segments can outlive the creating process.

Returns 0 success, -1 invalid/state, -2 OS failure, -3 unsupported platform.
The backend uses native `shmget/shmat/shmctl/shmdt` on Unix systems providing
them. No named POSIX shm API, resize, allocator, semaphore, atomicity or
synchronization is implicit. Tests: `tests/test_ipc_extensions.c` verifies
cross-process visibility, a read-only second attachment, and removal lifetime.
