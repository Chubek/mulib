# Linux TLS and native file scopes

`std.container.Tls` and `std.container.Foreign` are ordinary On1x modules
backed by a small trusted C11 leaf (`runtime.c`). They are not independently
reviewed KernelBind imports, and this leaf does not change the KernelBind
admission gate. The source-facing APIs exchange values and outcomes, not
pointers, native keys, descriptors, or integer handles.

## `std.container.Tls`

| Operation | Result and lifetime |
|---|---|
| `basis_tls_i64_current()` | `Option<i64>`: `None` before the first successful write or after a clear; all `i64` values (including zero) are valid. |
| `basis_tls_i64_set(value)` | `bool`: `false` if the native key could not be allocated or this thread could not install its first binding. A failed write does not change the value. |
| `basis_tls_i64_clear()` | Clears this thread's value; idempotent and allocation-free. |
| `basis_tls_i64_with(value, body)` | Installs a value and invokes the On1x callback once, returning `Some(body_result)`. On installation failure it returns `None` without running the body. On normal return, including a body returning a typed `Err`, restores the prior value or prior absence. Nesting and changes inside the body are supported. The result must fit the current nominal `Option` payload profile. |

MuLIB `xthreads` initializes one private native key exactly once and installs
each participating thread's own C11 `_Thread_local` storage as a **borrowed**
value. The key is retained for the executable's lifetime. Threads do not
share the slot, and the key never owns heap memory that must be destroyed at
thread exit. A cleared slot retains its binding, so even if the body clears
the value, scope restoration cannot require a new key or binding.

The four ABI calls in `runtime.h` mirror `present`, `value`, `set`, and
`clear`; `value` is guarded by `present` in On1x. These synchronous calls do
not invoke application code; the callback and its deferred restoration are
purely source-level. No signal reentry, forced termination, `longjmp`,
dynamically unloaded bridge or fatal-trap restoration guarantee is provided.
The On1x compiler still rejects raw foreign effects in cooperative
concurrency units: this is native-thread-local, **not task/fiber-local**.
There is no On1x thread creation or cross-thread capture/transfer interface.

## `std.container.Foreign`

`basis_foreign_file_byte_at(path, offset)` returns
`Result<Option<u8>, BasisForeignFileError>`. `Ok(Some(byte))` includes NUL and
`0xff`; `Ok(None)` means EOF. The path is a caller-selected NUL-free string
of 1..4095 bytes and the offset is 0..`INT64_MAX - 1`. `ForeignInvalid` reports
invalid input. The other error tags distinguish open errors
(`ForeignOpenFailed`), nonregular descriptors (`ForeignNotRegular`),
stat/read failures (`ForeignReadFailed`) and close failures
(`ForeignCloseFailed`).

The only raw file ABI call is `on1x_container_file_byte_at(const char *, int64_t)`.
Its native owner acquires one close-on-exec descriptor, verifies the open
descriptor is regular, reads a byte with 64-bit `pread`, then closes on every
post-acquisition path, including error and EOF. It publishes neither a byte
nor EOF if close fails. It never retries `close` on Linux. Unlike a shared
stream, there is no cursor, FD reuse identity, borrowed lifetime, or handle
for the application to manage. The bridge performs one attempt per syscall;
it does not hide retries or provide a time bound for filesystem I/O. No
snapshot consistency is promised across separate calls or concurrent file
updates.

This is not a path sandbox. The leaf does not follow a final symlink but
otherwise resolves paths using ordinary host filesystem authority; parent
symlinks and filesystem-specific open behavior still apply. `O_NONBLOCK`
avoids waiting for a FIFO writer before `fstat`; opening or reading from an
arbitrary filesystem or device is not guaranteed to be harmless or
nonblocking. Do not use the function to authorize paths from untrusted users.

## Unsupported

No first-class native file handles, raw pointer/descriptor adoption, generic
foreign destructors or callbacks, `Arc<T>`, weak references, or unchecked
cross-thread `Send`/`Sync`. The scoped TLS value is not atomic shared state.
There are no source APIs for namespace mutation, cgroup creation/attachment,
sandboxed worker launches, or pods. MuLIB's existing sandbox kernel is a
trusted single-owner worker supervisor, not a Docker-equivalent boundary. A
source adapter would need an independently approved bootstrap, delegated
cgroup ownership and host-enforcement/cleanup tests; merely passing paths
and flags would not establish any of these properties. See
[sandbox kernel limits](../../docs/kernel-sandbox.md).

The supported implementation is Linux direct C11; scalar MIR, cooperative
concurrency units, and KernelBind units reject these raw leaves. The measured
target is x86-64 Linux/glibc; other operating systems fail to compile the
leaf. The planner selects `ON1X_CONTAINER_TLS` and/or
`ON1X_CONTAINER_FOREIGN` from exact extern names; only TLS requires MuLIB's
one `MULIB_IMPL_XTHREADS` implementation and `-pthread`. The build also
checks the generated C declarations against the packaged companion header
before linking. This catches ABI-incompatible handwritten declarations
without authorizing the foreign operations as safe imports.

## Verification

From the repository root, with the configured compiler built:

```sh
python3 on1x/tests/test_container_native.py build/on1x-workstream4-gcc/on1xc gcc /tmp/opencode/container-gcc
python3 on1x/tests/test_container_native.py build/on1x-workstream4-clang/on1xc clang /tmp/opencode/container-clang
python3 on1x/tests/test_container_native.py build/on1x-workstream4-gcc/on1xc clang /tmp/opencode/container-asan \
  --cflag=-fsanitize=address,undefined --cflag=-fno-sanitize-recover=all --cflag=-fno-omit-frame-pointer
python3 on1x/tests/test_container_native.py build/on1x-workstream4-gcc/on1xc gcc /tmp/opencode/container-tsan \
  --native-only --cflag=-fsanitize=thread --cflag=-fno-omit-frame-pointer
```

The `.on1x` fixture covers nested restoration, typed error return, full-range
TLS values, binary bytes, EOF, sparse-file offsets past 4 GiB, and path
errors. The separate-TU C harness forces key and slot failure, checks
concurrent first use and native thread isolation, checks close-on-exec, injects
stat/read/close errors, and checks descriptor counts and one close per owner.
The suite also checks generated prototypes against the bridge header and
source-level diagnostics for integer handle substitution and unsupported
execution modes. Fault injection and descriptor counts in the test use
Linux/glibc linker symbols and `/proc/self/fd`; unsupported test hosts return
77 instead of claiming verification.

The verification checkout was based on MuLIB
`c2cbbf70537d12ba5015485239d227ef66b25cb8` with LibStone
`3399047f6c6328b8f07788795c610699e66c060a`. GCC 16.2.1 and Clang 22.1.8
passed both their default and `--cflag=-O2` builds on x86-64/glibc 2.44.
