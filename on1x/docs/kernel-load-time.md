# Load-time clamp kernel

Hosted direct C11 builds can opt into load-time resolution of
`stdffi.clamp` version 1. The checked-recipe provider does not require a
native compiler at launch. The native provider is limited to glibc Linux
AMD64 (LP64). Use the ordinary source spelling:

```on1x
import foreign stdffi.clamp as Clamp;

fn main() {
  println(Clamp.i64(27, 0, 10));
}
```

`Clamp.i64(value, low, high)` clamps to an inclusive interval. Reversed
bounds are rejected at the call (exit 70). `Clamp.add_i64(a, b)` and
`Clamp.sub_i64(a, b)` saturate at the signed 64-bit limits. The compiler
pins each referenced export's type and checked guard/body; it does not
take a package as input or link in a clamp implementation. The runtime
resolves and validates all required exports before initializing the
application or executing On1x `main`. Startup rejection returns exit 1
with the kernel, export, and reason on stderr.

This is an explicit alternative to the existing portable foreign import
pipeline. It is not [build-time KernelBind](kernel-bind.md), reviewed
`mulib.*` imports, `[dynlib]` linkage, or a general shared-library FFI.
Other kernels and other On1x backends are outside this profile.

## Build and compare two providers

Use a compatible, installed LibStone Cephyr (native embedding library,
resources and Python bindings) from the build's pinned LibStone revision.
From the MuLIB repository root, set `cephyr` to that absolute prefix and
choose fresh build and install paths. The following configures a focused
On1x SDK with the bootstrap scanner; a generated-scanner SDK with the
same runtime support also works.

```sh
cephyr=/absolute/path/to/installed-cephyr
sdk="$PWD/build/kernel-load-sdk"
cmake -S on1x -B build/kernel-load \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_INSTALL_PREFIX="$sdk" -DBUILD_TESTING=OFF -DON1X_USE_FALLBACK_SCANNER=ON \
  -DON1X_WITH_CEPHYR=ON -DCephyr_DIR="$cephyr/lib/cmake/Cephyr" \
  -DON1X_CEPHYR_RESOURCE_ROOT="$cephyr/share/cephyr" \
  -DON1X_WITH_KERNEL_NATIVE=ON -DON1X_BUILD_NATIVE_PPX=OFF \
  -DON1X_WITH_SYREN=OFF -DON1X_WITH_WETSTONE=OFF -DON1X_WITH_PROJECT_QMR=OFF
cmake --build build/kernel-load --parallel 2
cmake --install build/kernel-load
```

Run the [example](../examples/kernel/load-time-clamp/) with Python 3, a
host C compiler and the GNU BFD linker. The demo builds the application
once with the installed `on1x-build.py --kernel-load-time` before
either package exists. It then runs the installed
`on1x-kernel-package build` twice against the canonical clamp header:
once for a portable recipe and once for a native-image package.
`--allow-demo-native` authorizes execution
of the image produced in this local example; do not use this as a
deployment approval procedure.

```sh
work="$PWD/build/kernel-load-demo"
python3 on1x/examples/kernel/load-time-clamp/demo.py \
  --prefix "$sdk" --cephyr-prefix "$cephyr" \
  --header kernels/mulib-clamp.h --cc clang --work-dir "$work" \
  --allow-demo-native
```

The demo prints the application SHA-256, the native image SHA-256, and
two runtime provider lines with distinct **package** SHA-256 values. It
checks that the executable's digest is unchanged after running each
provider. Both providers must match 26 lines from an independent C
oracle, including exact endpoints, both directions of saturation, and
clamp interval boundaries. A second invocation of the *same executable*
with an argument must reject reversed bounds under both providers, as the
oracle does. Reports and captured results are saved under `$work`.
For example, inspect the deployed native package without loading it:

```sh
"$sdk/bin/on1x-kernel-package" inspect "$work/deploy/native/stdffi.clamp.on1xkp"
```

In a project, add `kernel-link load-time` to the root `On1xfile` and build
with `on1xc exec build <project-root>`. This selects the hosted direct C11
build and cannot be mixed with static `kernel` directives or
`--mulib-root`. Configure the package roots and native policy at launch
in the same way as for a single-file build.

## Launch the installed application

Only `$work/deploy` is needed to run the example on a compatible host;
it contains the executable, one directory per package and the example
native policy. The qualified Linux executable depends on the system C,
C++ and GCC support runtimes (`libc.so.6`, `libstdc++.so.6`, and
`libgcc_s.so.1`), not on Cephyr, KernelRT or the SDK. The following launch
commands use an unrelated empty
working directory and a clean environment. `env` starts the executable
directly, without the SDK, toolchain, Python, or MuLIB source paths.

```sh
deploy="$work/deploy"
(
  cd "$work/empty-cwd"
  env -i ON1X_KERNEL_PACKAGE_ROOTS="$deploy/portable" ON1X_KERNEL_TRACE=1 \
    "$deploy/clamp"
  env -i ON1X_KERNEL_PACKAGE_ROOTS="$deploy/native" ON1X_KERNEL_TRACE=1 \
    ON1X_KERNEL_NATIVE_POLICY="$deploy/native.policy" "$deploy/clamp"
)
```

`ON1X_KERNEL_PACKAGE_ROOTS` is a colon-separated list of host-selected
directories. Use absolute paths for working-directory-independent
deployment. The runtime looks for `stdffi.clamp.on1xkp` in these roots,
not in the current directory, source trees, compiler search paths, or
`MULIB_FOREIGNLIB_PATH`. Do not pass both example roots at once: two
candidates for the same module are ambiguous, not a preference order.
A missing or incompatible package stops startup; there is no
build-pinned fallback. `ON1X_KERNEL_TRACE=1` reports the selected
provider and its actual package digest to stderr after successful
initialization, without changing application stdout.

The resolver admits at most 16 roots, 64 distinct packages, 256 required
exports, and 32 MiB of package bytes over the whole resolution. It validates
the exact-version dependency order and conflicts with MuLIB CDD before
staging. Cycles, conflicts, and unavailable dependencies include the
dependency chain in their diagnostic. FFIHandler's build-time catalogue
(`stdffi/mulib/index.json` and `catalogue/` in the build tree) continues to
record discovery-only kernels and rejection reasons; those entries do not
become loadable packages.

## Native policy and trust

The portable package contains a checked recipe, but no native image, so
it needs no native policy. A native package requires
`ON1X_KERNEL_NATIVE_POLICY` to name an explicit host policy file. Native
code runs in the application process, with its privileges. A package's
self-reported digest, a successful `on1x-kernel-package inspect`, or a
matching recipe does not establish native code trust. Keep package roots
and policies under operator control, and authorize an independently
reviewed image only. A missing or mismatched policy rejects startup
instead of switching to the embedded recipe.

An operator policy has the literal first line `on1x-native-policy-v1`.
The next line binds the target, ABI, module version and image digest:

```text
x86_64-unknown-linux-gnu sysv-clamp-v1 stdffi.clamp 1 <image-sha256>
```

Next come the rows for `add_i64`, `i64`, and `sub_i64` in that order.
Each line lists the export, arity, adapter symbol, interface SHA-256 and
behavior SHA-256. For example, the row for `add_i64` has this shape:

```text
add_i64 2 on1x_kernel_native_add_i64 <interface-sha256> <behavior-sha256>
```

Use single spaces and a newline after every line. The inspector reports
the concrete field values without loading the image. See
[kernel package v1](kernel-package.md) for the format and compatibility
rules.
