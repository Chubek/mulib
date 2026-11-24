# Cephyr FFI and module prerequisites — 2026-11-21

## Current result — 2026-11-23

The generated clamp adapter now passes the executable boundary probe on
native AMD64 Linux. At MuLIB baseline `49de89ac` and selected LibStone
`b5c962721a32df18b2fb22de136c1f2ef8b6390d`, the old failure reproduced
before editing. The scalar MuLIB header only uses `<stdint.h>`; removing
its unrelated includes and having the adapter include `<limits.h>` for
`CHAR_BIT` resolved the failure without changing Cephyr or its headers.
The annotated contract identifies the new header digest and remains unreviewed.

In a Cephyr-enabled On1x build, run:

```sh
ctest --test-dir build/resume-on1x --output-on-failure -R '^on1x.cephyr.clamp_abi$'
```

Result: **1/1 passed** with GCC 16.2.1. Running the same probe with
`--cc clang` also passed. Each run executed the 2,670-comparison oracle
three times: with host-compiled adapter and kernel, embedded adapter and
host kernel, and independently embedded adapter and kernel objects. Each
link includes exactly one implementation TU. The wrong-prototype control
still rejects. This is scalar ABI evidence only; other targets and the
Cephyr-backed trusted module loader are unverified.

## Historical result — 2026-11-21

**Milestone C was blocked for the generated clamp adapter.** With the bundled
Cephyr standard library, the real MuLIB declaration header fails preprocessing
before any adapter assertion runs. **Milestone D is unverified:** the host
On1xRT tests pass, but no Cephyr-produced On1xRT module was loaded or invoked.

## Reproduce the C blocker

Run from the MuLIB repository root. The tested host was x86-64 Linux/glibc,
GCC 16.2.1 (`cc -dumpmachine`: `x86_64-pc-linux-gnu`), CPython 3.14 and
Cephyr 0.1.0. MuLIB baseline: `b5b4f0453b98a3e53e8651d611220eecf68f36ee`.
Use the project-selected LibStone checkout, including its build dependencies,
Archon resources and target assembler. Use the same CPython major/minor as
the embedded library. An unrelated installed `cephyr` executable is not needed.

```sh
stone="$PWD/on1x/third_party/libstone"
test "$(git -C "$stone" rev-parse HEAD)" = 248ad1777c7b3023cd04902213e456631b4cd8fb
mkdir -p build/C-D-compat/tmp
export TMPDIR="$PWD/build/C-D-compat/tmp"
# Build prerequisite if build/A-cephyr/libcephyr.so does not exist:
cmake -S "$stone/cornerstone/Cephyr" -B build/A-cephyr -DPython3_EXECUTABLE=/usr/bin/python3.14
cmake --build build/A-cephyr --target cephyr_compiler -j4
/usr/bin/python3.14 on1x/tests/cephyr_ffi_compat.py build/A-cephyr/libcephyr.so
```

The reproducer uses the selected checkout's Python wrapper to call `libcephyr`
in process with `--target=x86_64-linux-gnu --stdlib=cephyr`. Three controls
passed: a Cephyr-compiled scalar caller linked and ran against the real
`contract_abi.c` fixture; a deliberately wrong prototype was rejected by its
`_Generic` assertion; and GCC linked the production-generated clamp adapter to
one `MULIB_IMPL_CLAMP` translation unit and passed all **2,670** oracle
comparisons. The fourth check, compiling that same adapter with Cephyr, exited
**1**:

```text
kernels/mulib-clamp.h:23: error[cephyr.preprocessor.error]: file 'math.h' not found
```

**Suggested upstream fix:** add and package a target-correct `math.h` in
Cephyr's bundled standard library. Test header inclusion against installed
resources as well as the source tree, and compile/link/run the functions or
classification macros the header claims to support. An empty header or a host
header injected via `-I` would conceal the resource gap. After an independently
reviewed pin update, rerun the reproducer: removing the first error does not
establish that the remaining ABI checks or link will pass.

## Host FFI and runtime controls

The following commands used two **existing** On1x build trees. Substitute
matching configured and built trees on another machine. The FFI tree had
`ON1X_WITH_EXOML`, `ON1X_WITH_EXOML_EXTRACT`, `ON1X_WITH_EXOTRACT` and
`ON1X_WITH_EXOTRACT_LUA` enabled; it needs `on1xc`, `on1x-bind`, Exotract,
libclang/Clang resource headers and OpenSSL. `ON1X_WITH_CHISEL` was off:
the contracts test checks refusal when its Chisel gate is unavailable. The
runtime tree had the extension SDK and `ON1X_RT_BUILD_TRUSTED_DYNAMIC` enabled
on Linux/glibc, with the host and shared-module fixtures built. Keep `TMPDIR`
on a writable filesystem with free space; the first FFI run exhausted the
system temporary directory.

```sh
ctest --test-dir build/on1x-workstream4-gcc \
  -R '^on1x\.(kernel_review_sdk|contracts_sdk)$' --output-on-failure
ctest --test-dir build/on1x-memgraph-gcc \
  -R '^on1x.runtime.trusted_dynamic$' --output-on-failure
```

Results: FFI **2/2 passed**, trusted dynamic runtime **1/1 passed**, no skips.
The KernelBind test uses an ephemeral test review authority; it does not
approve a production import. The runtime test exercises On1xRT's existing
host-authorized, digest-pinned loading path.

Four additional host executables were built directly from the current sources:

```sh
set -e
for t in abi lifecycle negative fault; do
  cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -Ion1x/runtime-sdk/include -Ion1x/runtime-sdk/tests \
    on1x/runtime-sdk/src/On1xRT.c on1x/runtime-sdk/tests/fixture_module.c \
    "on1x/runtime-sdk/tests/$t.c" -pthread -o "build/C-D-compat/$t"
  "build/C-D-compat/$t"
done
```

**4/4 passed.** These verify host ABI and lifecycle prerequisites only.
Cephyr's additive context API links independent translation units for shared
output; neither that API nor a successful scalar object test demonstrates
reviewed FFI admission, a target-compatible runtime link, or authorization
and lifetime handling for a compiler-generated module.
