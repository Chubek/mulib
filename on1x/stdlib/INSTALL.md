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

## C/ABI requirements

C11, `int64_t`, `_Bool`, NUL-free `const char *` and POSIX filesystem/time
APIs are used. The adapter is tested on Linux. The C11 library compiles with
GCC and Clang and links against the C math library (`-lm`). Non-POSIX ports
must replace the platform operations before claiming portability.

Array handles are process-local and generation-tagged; they are never raw
pointers, but are still explicitly managed, unshareable resource tokens.
Caller discipline, not the existing On1x type system, enforces cleanup.
