# MuLIB dynload: native shared libraries

[Manuals](README.md) · [Library index](index.md)

**Header:** `include/mulib-dynload.h`  
**Implementation switch:** `MULIB_IMPL_DYNLOAD`  
**MuLIB dependencies:** none. **Link:** `-ldl` on Linux/other libdl systems;
macOS uses its system loader, Windows its standard system libraries.

## Scope

POSIX builds use `dlopen(RTLD_NOW | RTLD_LOCAL)`, `dlsym`, and `dlclose`. Windows
builds use `LoadLibraryA`, `GetModuleHandleA`, `GetProcAddress`, and `FreeLibrary`.
Other platforms explicitly return `ENOTSUP`. Windows paths use the native ANSI
API encoding, not a promised UTF-8 conversion. This API does not implement a
plugin protocol, dependency isolation, custom search paths, or a
sandbox. Library constructors/destructors execute native code. Only load trusted
libraries; prefer explicit paths and understand the operating system's loader
search policy.

System headers are implementation-only, and no global feature-test macros are
defined. The declaration-only header is usable from C11 or C++17.

## API

```c
mulib_dynload_t library = MULIB_DYNLOAD_INIT;
int mulib_dynload_open(mulib_dynload_t *, const char *path, char *error, size_t capacity);
int mulib_dynload_open_ex(mulib_dynload_t *, const char *path, unsigned flags, char *error, size_t capacity);
int mulib_dynload_open_pinned(mulib_dynload_t *, const char *path, char *error, size_t capacity);
int mulib_dynload_symbol(const mulib_dynload_t *, const char *name, void **symbol,
                        char *error, size_t capacity);
int mulib_dynload_close(mulib_dynload_t *, char *error, size_t capacity);
int mulib_dynload_function(const mulib_dynload_t *, const char *name, void *function,
                          size_t function_size, char *error, size_t capacity);
int mulib_dynload_native_name(const char *name, char *output, size_t capacity);
```

Initialize each handle and do not copy it while live. Passing NULL as the path
opens the current process. POSIX owns that loader reference; Windows borrows the
main-module reference and does not pass it to `FreeLibrary`. Named Windows
modules are owned normally. Open fails with `ESTATE` if already open. Failure
leaves the handle unchanged.

`open_ex` adds NOW/LAZY and LOCAL/GLOBAL flags on POSIX; omitted groups default
to NOW and LOCAL. Conflicting/unknown flags are invalid. Windows rejects LAZY
and GLOBAL with ENOTSUP. `open` retains its eager/local default behavior.

The dynalo-style native-name helper formats a **bare** name as `libNAME.so`
(Linux/Unix), `libNAME.dylib` (macOS), or `NAME.dll` (Windows). It appends prefix
and suffix unconditionally, rejects empty/dot/path names, and returns ERANGE
without modifying output if capacity (including NUL) is insufficient. In-place
conversion is supported when the original name is in adequate writable storage.
The helper does not search/load the name or infer whether it already has a suffix.

`open_pinned` requires a named path and additionally pins the native module until
process exit using POSIX `RTLD_NODELETE` or Windows `GetModuleHandleEx` with PIN.
Unsupported platforms return `ENOTSUP`. This is useful for runtime libraries with
process-wide caches, singletons or callbacks that cannot be safely unloaded by
one client. Pinning is permanent and affects all references to the same module;
it is not a per-context isolation boundary. `close` still releases the owned
loader reference and resets the handle. Runtime global data remains mapped and
reusable across repeated open/close cycles. No MuLIB global cache is allocated.

Symbol lookup writes a borrowed native address on success only. POSIX's `dlerror`
protocol distinguishes a successful NULL-valued symbol from a failed lookup, so
use the return status, not merely `*symbol`. A symbol must not be used after the
owning library closes. Export visibility and symbol spelling are native-loader
concerns; opening the process does not export every private executable symbol.

ISO C does not guarantee object/function pointer interchangeability. To call a
function symbol use your platform's documented ABI conversion (commonly `memcpy`
between equal-sized pointer objects on POSIX), with exactly the exported function
type. Do not use a wrong prototype or assume all function pointers have the same
representation. The Windows implementation checks representational size before
copying `FARPROC` into the API's `void *` output.

`mulib_dynload_function(&library, "name", &function, sizeof function, error, cap)`
performs the equal-size host-ABI conversion without an unsafe `void **` cast.
`function` is an exactly typed function-pointer variable; its bytes change only
on successful lookup. The loader cannot validate the function's prototype.
This also works with an erased `mulib_dynbox_fn` for runtime-typed calls through
[dynbox](mulib-dynbox.md). Keep the library open until all calls/closures finish.

For building plugins, `MULIB_DYNLOAD_EXPORT` supplies native export visibility;
define `MULIB_DYNLOAD_EXPORT_SYMBOLS` when building a Windows DLL (otherwise it
means dllimport). `MULIB_DYNLOAD_CALL` is __cdecl on Windows, empty elsewhere.
These are C declarations/macros, not dynalo's C++ RAII/template interface.

Close is idempotent. A native close failure retains the handle for retry. Do not
close concurrently with lookups or calls through its symbols. No global mutable
MuLIB error buffer or process-global handle cache is maintained.

## Errors

All APIs return zero on success and negative `EINVAL`, `EIO` (native loader
failure), `ENOTSUP`, `ESTATE`, or `ERANGE` (name-buffer capacity). On loader calls an optional caller-owned error
buffer receives a terminated message, truncated to capacity minus one; success
clears it. NULL buffers or zero capacity are allowed. Capacity one produces only
a NUL. Error buffers must not overlap input strings, output symbols, or handle
storage. POSIX native messages are copied immediately; Windows messages include
the native error number. There is no promised `errno` contract.

## Runnable example

```c
#define MULIB_IMPL_DYNLOAD
#include "mulib-dynload.h"
#include <assert.h>
int main(void) {
    mulib_dynload_t d = MULIB_DYNLOAD_INIT;
    char error[256];
    void *address = NULL;
    assert(mulib_dynload_open(&d, NULL, error, sizeof(error)) == 0);
    assert(mulib_dynload_symbol(&d, "malloc", &address, error, sizeof(error)) == 0);
    assert(address);
    assert(mulib_dynload_close(&d, error, sizeof(error)) == 0);
    return 0;
}
```

This inline example targets a dynamically linked POSIX process. The full
`examples/modules/dynload.c` also selects a Windows system library when relevant.
On Linux build/run with
`cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/dynload.c -ldl -o dynload-example`
then `./dynload-example`. Storage tests cover native failures, unchanged symbol
outputs, current-process lookup, tiny error buffers, state, and repeated close.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dynload.h`](../include/mulib-dynload.h) |
| Implementation switch | `MULIB_IMPL_DYNLOAD` |
| Repository-local includes | None |
| Complete example | [`examples/modules/dynload.c`](../examples/modules/dynload.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dynload.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
