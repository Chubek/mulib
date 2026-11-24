# On1xRT embedding ABI 1

`On1xRT` is the recoverable C11 application embedding API. It is independently
versioned from the compiler extension SDK and does not change
`on1x/runtime/on1x_runtime.h`, the private ABI used by generated programs.

## Supported profile

- thread-confined engines using a copied host allocator table;
- statically registered native modules;
- native AOT modules only after an explicit host authorization callback;
- on Linux/glibc with the optional loader, trusted native shared objects after
  host authorization of the exact artifact bytes;
- transactional dependency registration and reverse lifecycle compensation;
- module, instance, function, host-function, string, and buffer handles;
- exact `i64`, `bool`, `f64`, string, buffer, and void signatures;
- synchronous host callbacks and callback-scoped module/call capabilities;
- length-aware UTF-8 strings and arbitrary buffers, including embedded NUL;
- allocator-domain structured errors and recoverable lifecycle failures.

Native callbacks execute in the host process with the host process's authority.
They are **not sandboxed**. Path-only dynamic loading, WASM, public concurrency,
tasks/fibers/channels/schedulers, sandboxing, debugger, and profiler integration
return `ON1X_RT_UNSUPPORTED`. This does not change the separate private MuLIB
CSP runtime used by generated programs or add a public Plaid adapter.

## Ownership and lifetime

- A successful create/register/lookup returns one owned handle. Release/unload
  consumes that handle and clears the caller's slot on success.
- `on1x_rt_value_string` and `on1x_rt_value_buffer` are non-retaining handle
  wrappers. Function arguments borrow their handles for the synchronous call.
  A successful string/buffer result owns one handle; release it exactly once
  and do not release copied aliases.
- Handles encode `generation32-slot32` and also carry a pointer-free engine
  owner identity. Every operation checks owner, generation, kind, lifecycle,
  and creating thread.
- Engine destruction returns `BUSY` until every module, instance, function,
  host function, string, and buffer has been released.
- Descriptor strings, dependency arrays, exports, and signatures are copied.
  Callback code, provider context, and function context remain provider-owned
  until unload/unregister.
- String/buffer views are borrowed until the handle is released (and, for
  buffers, until mutation).
- A callback returns owned string/buffer results only through
  `on1x_rt_call_*_create`. Unreturned callback allocations are reclaimed when
  the outer call ends. `on1x_rt_call_value_release` drops an intermediate
  callback-owned value.
- Errors are owned objects. Release them with `on1x_rt_error_release`, including
  after the originating engine has been destroyed; therefore the allocator
  context must outlive all of its objects and errors.
- Module allocator operations are valid only during a lifecycle callback.
  Call-context and module-context pointers may not be retained.
- A failing initialize/create callback leaves its output state null. Shutdown
  is the reverse compensation for successful initialization and must be able to
  run during transaction rollback. A failing public shutdown/destroy operation
  is failure-atomic: the handle remains live so the host can retry it.

Callbacks are synchronous, serialized, and may not throw through C. Public
engine operations reject reentry; host functions may call other host functions,
but recursive invocation of an active host function rejects.

Extensible ABI records use an `abi_version`/`struct_size` prefix. Missing
trailing fields are zero, unknown ABI-1 trailing fields are ignored, and no
field is read unless its complete representation is covered by `struct_size`.

## Build and consume

The SDK hook builds `On1x::RT`. Installation provides a separate CMake package:

```cmake
find_package(On1xRT 1 CONFIG REQUIRED)
target_link_libraries(host PRIVATE On1x::RT)
```

A separately compiled module exports an ordinary descriptor accessor:

```c
ON1X_RT_EXPORT const On1xRT_ModuleDescriptor *
ON1X_RT_MODULE_ENTRY_SYMBOL(void);
```

The host links that module and passes the descriptor to
`on1x_rt_engine_register_static_module`.

### Calling a scalar export from C, C++ or Python

On1xRT package 1.1 adds `on1x_rt_abi_version()` and a one-call C convenience
operation for exports with 0–32 `i64` arguments and an `i64` result:

```c
int64_t args[] = {21, 42}, result;
On1xRT_Error *error = NULL;
On1xRT_Status status = on1x_rt_instance_call_i64(
    engine, instance, (On1xRT_StringView){"choose", 6}, args, 2, &result, &error);
/* Inspect result only on OK. Otherwise inspect status and optional error. */
if (error) on1x_rt_error_release(&error);
```

This operation acquires a temporary function handle, checks the exact export
signature and releases the handle even when the callback fails. For repeated
calls without lookup, or for other value types, use `on1x_rt_instance_function`
and `on1x_rt_function_call` directly.

`<On1xRT.hpp>` provides a C++17 move-only host that owns one engine, one
statically registered module and its instance. Link `On1x::RT` from
`find_package(On1xRT 1.1 CONFIG REQUIRED)`. Given a linked descriptor:

```cpp
on1x::rt::host host;
host.register_static(*ON1X_RT_MODULE_ENTRY_SYMBOL());
host.instantiate();
auto greatest = host.call_i64("choose", {21, 42});
```

Failures throw `on1x::rt::error` with the runtime `status`, diagnostic `code`
and message. Destruction tears down in reverse order. If provider lifecycle
callbacks can fail, call `host.close()` explicitly and retry after resolving
the failure. A failed implicit close in the noexcept destructor terminates.
All operations, including moving and destroying the host, belong on its
creating thread.

For Python 3, build the same runtime as a shared library (default: static):

```sh
cmake -S on1x -B build/embedding -DON1X_RT_BUILD_SHARED=ON
cmake --build build/embedding --target on1x_rt_sdk
cmake --install build/embedding --prefix "$PWD/build/rt-prefix" --component On1xRT
export PYTHONPATH="$PWD/build/rt-prefix/lib/on1x-runtime/python"
```

Substitute the configured CMake library directory for `lib` if different.
The installed `on1x_rt` module finds its sibling library relative to the
installation tree on Linux. An explicit `Host(library=...)` or environment
variable `ON1X_RT_LIBRARY` overrides discovery. For a Python-enabled
application with a linked, exported `on1x_rt_module_descriptor_v1`:

```python
import ctypes
from on1x_rt import Host

with Host() as host:
    host.register_static(ctypes.CDLL(None))  # trusted provider in this process
    host.instantiate()
    greatest = host.call_i64("choose", 21, 42)
```

Register only already trusted native code linked against the **same** On1xRT
library. The wrapper retains the provider until module unload. This interface
does not authorize or load native module files; use the dedicated C API below
for that operation. `call_i64` rejects Python `bool` and integers outside the
signed 64-bit range. Native errors expose `status` and `code` on `on1x_rt.Error`.
Use a `with` block on the creating thread; if close fails, the host still owns
the remaining resources and `close()` can be retried there.

### Executable example and verification

`tests/choose.on1x` is compiled by `on1xc --no-prelude --emit-c` during the
test build. `tests/choose_provider.c` adapts that particular generated
function to a versioned module descriptor. The generated C symbol is private
to the fixture: this is an application embedding API, not a general On1x
export generator or an in-process source compiler. It uses the ordinary On1x
C11 backend; it requires no Cephyr installation. The compiler/frontend
embedding interfaces are documented separately in [SDK 2](../sdk/README.md).

```sh
cmake --build build/embedding --target on1x_rt_embed_c on1x_rt_embed_cpp on1x_rt_python_fixture
ctest --test-dir build/embedding -R 'on1x.runtime.embedding_' --output-on-failure
```

`on1x_rt_embed_ownership` is a separate fixture for a statically registered
module. An exact `(i64, i64) -> i64` export calls a generated On1x function that
boxes a `Cell`, takes shared and exclusive lexical borrows, replaces the payload
through `ref`, and transfers the box through both arms of an owning `Result`.
A function-local recursive chain is dropped on normal and propagated-error
returns. Two defers retain a `sexpr` after its block exits. The fixture checks
their LIFO order and zero live unique and reference-counted allocations after
each call. It rejects unknown value kinds and nonzero signature flags before
provider initialization or invocation. ABI 1
does not transport On1x `own<T>` values or lexical loans, including as pointers
or as `On1xRT_Buffer` handles. The private C11 runtime retains its ordinary
failure behavior (including fatal allocation failure); running through On1xRT
does not make that code recoverable or sandboxed.

```sh
cmake --build build/embedding --target on1x_rt_embed_ownership
ctest --test-dir build/embedding -R '^on1x\.runtime\.embedding_ownership$' --output-on-failure
```

## Trusted dynamic native module

`on1x_rt_engine_load_authorized_dynamic` takes an absolute UTF-8 path and a
separate pre-load authorization callback. The callback receives the requested
path and the measured lowercase SHA-256 of a sealed copy of the opened
artifact. Approve only a digest that matches a trusted external allowlist or
other independently authenticated expectation. Changing the path or its
contents during authorization cannot change the bytes passed to the OS loader.
Without approval, the loader is never called. A successful descriptor check
after loading is not a substitute for this authorization.

After artifact approval, `dlopen` may run native constructors or resolvers.
The loader resolves the ABI-1 accessor directly in the loaded image (not from
a dependency), then uses the existing authorized-AOT registration transaction.
Set the descriptor flag to `ON1X_RT_MODULE_AUTHORIZED_AOT` and provide
`On1xRT_EngineConfig.authorize_aot` for descriptor policy, as for linked AOT
modules. This policy runs after native loading but before module initialization.
The descriptor's `identity_sha256` is provider metadata, separate from the
measured artifact digest. Neither check confers foreign-binding safety
admission. Name, dependency, export, and signature checks are unchanged.

The profile uses Linux/glibc, `memfd` sealing and `/proc/self/fd`, and does not
fall back to loading the original pathname. Limits: 4,096 path bytes, 64 MiB
per regular-file image, and 256 MiB of retained images per engine, in addition
to engine module/handle limits. `ON1X_RT_BUILD_TRUSTED_DYNAMIC=OFF` disables
the adapter; `on1x_rt_feature_status` distinguishes a compiled adapter from an
unsupported build but does not probe host kernel or loader restrictions.
This profile does not bound native execution or attest transitive native
dependencies. The host must trust the complete native dependency and loader
environment; `$ORIGIN`-relative deployment from the original filename is not
supported. Providers that call On1xRT must resolve the host's public runtime
symbols (for example, via CMake `ENABLE_EXPORTS` on an executable) rather than
linking a second runtime into the DSO.

Release functions and instances before unloading. A failed shutdown keeps the
handle and loader reference live for retry. After successful shutdown (or a
failed registration), the engine drops its loader reference and sealed-image
fd. The OS may still keep the image mapped (e.g. `NODELETE` or other owners).
Module code must not leave background work, registered callbacks, or other
state pointing into the library after shutdown. This is not a compiler
generator for On1xRT artifacts and does not export the private generated C ABI.
