# On1x SDK 2

SDK 2 is the C11 boundary around the canonical On1x compiler, immutable
frontend/semantic/IR views, analyses, transformations, backends, reviewed
foreign admission, projects, and developer tooling. It preserves the original
ABI-v1 extension profile as a distinct compatibility surface. The independently
versioned `On1xRT` package is the runtime embedding API for generated native
modules.

## Build and install

`ON1X_BUILD_EXTENSION_SDK` defaults to `ON`. To build and install just the SDK:

```sh
cmake -S on1x -B build/on1x -DBUILD_TESTING=ON
cmake --build build/on1x --target on1x_sdk on1x_sdk_project on1x_rt_sdk
cmake --install build/on1x --prefix "$PWD/build/on1x-install" --component On1xSDK
cmake --install build/on1x --prefix "$PWD/build/on1x-install" --component On1xRT
```

Consumers use `find_package(On1xSDK 2 CONFIG REQUIRED)`,
`target_link_libraries(my_tool PRIVATE On1x::SDK)`, and `#include <On1xSDK.h>`.
`On1x::Project` adds the shared project/tooling service, while
`On1x::SDKSemanticsIR` makes the canonical compiler-support dependency explicit.
Every C header is independently C11/C++17 compatible; `On1xSDK.h` includes the
complete C surface and `On1xSDK.hpp` supplies the covered C++17 project RAII
adapters.
Point `CMAKE_PREFIX_PATH` at the install prefix. The archive is implemented in
C++; a C-only consumer needs a compatible C++ linker/runtime. With CMake,
enable both C and CXX in the consuming project.
The runtime is separately discoverable with `find_package(On1xRT 1 CONFIG
REQUIRED)` and links as `On1x::RT`; it does not need the compiler SDK.

The data-only SDK utility is also available explicitly through the compiler:

```sh
on1xc sdk info
on1xc sdk validate catalogue /path/to/CAPABILITY_MATRIX.json
```

This dispatch occurs only for the `sdk` subcommand and never loads extension
code or grants capabilities.

## Using the SDK in `on1xc`

To discover external plugins, set `ON1X_PLUGIN_PATH` to a colon-separated list
of directories. The driver scans them when you request an SDK hook:

```sh
export ON1X_PLUGIN_PATH="$HOME/.local/lib/on1x/plugins:/opt/on1x/plugins"
on1xc --sdk-analyze mir-check hello.on1x
on1xc --plugin +mir-tools --sdk-emit mir-copy hello.on1x -o hello.mir
on1xc --plugin +./libmir-tools.so --sdk-analyze mir-check hello.on1x
```

Only regular, non-symlink `.so` files are candidates. Discovery is
non-recursive and deterministic: directories in path order, filenames in
byte order. Empty or nonexistent path elements are ignored; an empty element
does not mean the working directory. With repeatable `--plugin +…`, only
selected plugins are loaded. A bare selector matches a filename, stem, or
stem without `lib`; selectors containing `/` are paths and bypass discovery.
Ambiguous names and more than one provider for the same requested hook are
errors. Ordinary compiler commands do not inspect `ON1X_PLUGIN_PATH` or run
SDK plugins.

An external plugin exports `on1x_sdk_linked_extension()` (the same ABI used
for linked providers). For a C provider:

```sh
cc -std=c11 -shared -fPIC -I /prefix/include/on1x-sdk extension.c -o libmir-tools.so
```

SDK symbols are provided by the worker. Plugins must use only dependencies
that are already loaded by the worker (such as its C/C++ runtimes); extra
shared libraries and plugin-owned access to files, sockets, subprocesses and
threads are not supported.

Dynamic loading is supported on Linux x86-64 in unprivileged builds with
`ON1X_BUILD_DYNAMIC_SDK_PLUGINS=ON` (default). Keep the trusted
`on1x-plugin-worker` installed alongside `on1xc`. The worker uses MuLIB's
namespace, seccomp and framed IPC kernels. It enters private namespaces and
a private filesystem containing only the plugin image, closes inherited
handles except its control socket, drops capabilities, and installs syscall
and resource limits **before** `dlopen` (including before constructors run).
If isolation fails, the plugin is not loaded; there is no in-process
fallback. A plugin still controls its own diagnostics and emitted bytes;
these are not a security attestation.

To enable a trusted, **in-process** provider in your own `on1xc` build, configure
`-DON1X_LINKED_SDK_EXTENSION=/absolute/path/to/extension.c` (or `.cpp`). That
source defines `on1x_sdk_linked_extension()` and returns a static
`On1xSDK_Extension` descriptor. Its `register_all` callback adds the hooks.
An example provider is in [`tests/linked_extension.c`](tests/linked_extension.c).
Linked providers are used only when no external provider matches and no
explicit `--plugin` selector was supplied.

```sh
build/on1x/on1xc --sdk-analyze mir-check on1x/examples/hello.on1x
build/on1x/on1xc --sdk-emit mir-copy on1x/examples/hello.on1x -o hello.mir
```

These ABI-v1 commands invoke only the named analyzer or emitter, after the ordinary
On1x checker and MIR lowering. With `--ir-input`, they decode and verify
serialized MIR instead. Both routes supply read-only canonical serialized
MIR, not compiler object layouts; consumers must handle its own format
version. Diagnostic byte ranges describe those serialized bytes, not the
original On1x source file. Emitters return opaque bytes; neither the SDK nor
`on1xc` loads, executes, or attests to the resulting artifact. The default
C11 path and the existing PPX pipeline are unchanged.
Successful file output is published atomically on POSIX hosts; other hosts
must use stdout for SDK artifacts.

For verified MIR traversal instead of serialized bytes, configure the
[trusted linked ABI-2 analyzer](docs/trusted-linked-mir-analyzer.md) and select
it with `on1xc --sdk-analyze linked-v2:HOOK INPUT`. This profile does not load
external ABI-2 DSOs or run ABI-2 emitters or transformations.

Linked and standalone in-process callbacks run with the host's privileges.
Run only code you trust in those modes.

## Scope and lifetime

- The headers are the API contract. Initialize ABI-2 descriptors with
  `ON1X_SDK_ABI_VERSION_V2` and `sizeof(descriptor)`. The frozen
  `ON1X_SDK_ABI_VERSION` value remains `1` only for the exact legacy layouts.
  Check every returned status.
- Hosts own registries and program/artifact handles; release them through the
  matching SDK function. Descriptor data copied by the registry is independent
  of the caller's strings; userdata and callback code remain caller-owned.
  Do not retain callback-scoped views or pass a registry across threads.
- Standalone hosts can create source buffers and use source-phase analyzers
  and transformers. Any transformed bytes are still unverified source. The
  `on1xc` driver does not dispatch SDK source transformers, expose checked
  HIR, or accept extension-supplied MIR as verified.
- Contract, module, CLI option, toolchain and execution-environment descriptors
  do not admit foreign symbols, change module imports or add options to stock
  `on1xc`, resolve executables, launch commands, or enforce isolation.
  Standalone hosts can use the CLI option API to parse a registered `--flag`
  or `--name=value` token without shell interpretation.
