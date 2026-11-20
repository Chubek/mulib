# Installation, build, and checks

Build the compiler and its libraries together from the MuLIB checkout:

```sh
cmake -S on1x -B build/on1x -DBUILD_TESTING=ON
cmake --build build/on1x --parallel 4
ctest --test-dir build/on1x --output-on-failure
python3 on1x/tools/on1x-build.py on1x/examples/modules.on1x \
  --on1xc build/on1x/on1xc -o build/modules
build/modules
```

The build helper automatically links the Basis adapters when needed. With a
manual C compiler invocation, include the bridge translation units and `-lm`:

```sh
build/on1x/on1xc --emit-c on1x/stdlib/tests/test_io.on1x \
  -o build/basis-io.c
cc -std=c11 -I on1x/runtime build/basis-io.c \
  on1x/runtime/on1x_runtime.c \
  on1x/stdlib/runtime/basis_native.c \
  on1x/stdlib/runtime/basis_real.c -lm -o build/basis-io
build/basis-io
```

`cmake --install build/on1x --prefix PREFIX` installs a relocatable compiler,
libraries and tools. See [loader configuration](../docs/modules.md) for `-I`,
`--stdlib`, environment paths, prelude selection and PPX discovery.

## Building the standard-library source package

```sh
python3 on1x/tools/on1x-project.py build on1x/stdlib \
  --on1xc build/on1x/on1xc --output-dir build/on1x-stdlib
python3 on1x/tools/on1x-build.py on1x/examples/stdlib_extensions.on1x \
  --on1xc build/on1x/on1xc --stdlib build/on1x-stdlib/library \
  --mulib-root . -o build/packaged-stdlib-example
build/packaged-stdlib-example
```

The root `On1xfile` advertises `std`. Directory-local `On1xfile.m` files
classify every library module and selected raw runtime resources. The project
driver checks the modules against the staged `std` package, including its
pervasive module, rather than resolving missing sources in the checkout or
installed library. The output preserves the module paths and can be moved as
a unit. Use it with `--stdlib` or `--stdlib-path`, not as an ordinary project
dependency named `std`.

The package includes the Basis C adapters (`runtime/basis_native.c`,
`runtime/basis_real.c`, and `runtime/basis_native.h`) and the prettyprint and
terminal companions (`prettyprint/runtime.[ch]` and `tty/runtime.[ch]`). The
build helper resolves their `.c` files from the selected `--stdlib` root and
compiles them with the host C toolchain when needed. The prettyprint and
terminal bridges still need the MuLIB kernel planner and headers. Only select
a source package you trust as an SDK library: selecting it with `--stdlib`
permits those raw C files to be compiled into downstream binaries. The
`mulib/On1xfile` discovery manifest also ships, so `-I PACKAGE/mulib` can still
resolve `import mulib`.

The package also carries the native [container companions](README.md#ownership-and-scoped-containers)
for TLS, scoped file operations, shared owners and the Linux Pod fixture.
The build helper selects only the referenced native groups and uses one
generated MuLIB implementation translation unit for the application. The
fixed Pod worker is compiled as a separate static executable only with
`--enable-pod-fixture`; see the [host requirements](container/POD.md).

This is not an SDK installation or native archive. The build helper takes
the core On1x runtime, `_bridge/mulib_bridge.c` and its header, and the
`std.mulib` metadata runtime/planner from its SDK, not from the source package.
MuLIB headers and any selected native companions must be available to that
SDK; `--mulib-root` selects their checkout or installation. The package does
not carry compiler/PPX executables, project-policy helpers, the Basis
reference/catalog, or test fixtures. Use CMake installation for the full SDK.
The raw extern boundary and the existing partial Basis coverage are unchanged.

## C/ABI requirements

C11, `int64_t`, `_Bool`, NUL-free `const char *` and POSIX filesystem/time
APIs are used. The adapter is tested on Linux. The C11 library compiles with
GCC and Clang and links against the C math library (`-lm`). Non-POSIX ports
must replace the platform operations before claiming portability.

Array handles are process-local and generation-tagged; they are never raw
pointers, but are still explicitly managed, unshareable resource tokens.
Caller discipline, not the existing On1x type system, enforces cleanup.
