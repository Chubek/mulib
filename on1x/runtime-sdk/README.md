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
