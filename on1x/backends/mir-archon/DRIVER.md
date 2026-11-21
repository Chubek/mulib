# Native driver and executable build

The explicit compiler SDK route emits assembly from verified source or MIR:

```sh
BUILD=/mnt/andromeda/tempdir/opencode/on1x-archon-build
"$BUILD/on1xc" --sdk-emit mir-archon \
  --sdk-target x86_64-unknown-linux-gnu program.on1x -o program.s
```

`--ir-input` accepts serialized MIR. `--optimize-ir` and its existing bounded
`--opt-work`/`--opt-rewrites` pipeline are supported for this native request.
The provider is an explicitly linked trusted builtin; external `--plugin`
selection is rejected. Target selection is exact, not inferred from the host.
The compiler callback does not run an assembler, linker or generated program.
`--native-pic` requests PIC assembly; `--native-no-entry` omits the C `main`
wrapper. Both flags require the explicit native SDK target mode above and can
be combined.

For a complete native executable:

```sh
python3 on1x/tools/on1x-native-build.py program.on1x \
  --build-dir "$BUILD" --target x86_64-unknown-linux-gnu \
  --cc /usr/bin/cc -o program
./program
```

The helper explicitly uses the standalone pipeline:
`on1xc --emit-ir` → `on1x-archon-mir` → trusted GNU-compatible GAS assembler →
link with `libon1x_archon_scalar_abi.a`, `libon1x_runtime.a` and `libm`.
It never emits source C for the program, falls back to another backend, or runs
the result. The runtime and scalar wrappers must already be built.

The executable helper currently qualifies **Linux AMD64 SysV only**. It
checks the generated backend manifest, the standalone tool's configured exact
triple and actual generated ABI, the explicitly chosen compiler's target, and
the final ELF64 machine/type. `--toolchain-prefix` overrides manifest discovery
from `ArchonBackend_DIR` in the combined build's CMake cache. Trusted tools and
runtime archives can be selected explicitly by absolute paths; this is a
trusted host build, not an untrusted-tool sandbox.
The helper resolves and directly invokes the selected GNU-compatible assembler;
it pins the linker through a private `-B` directory, ignoring ambient GCC
tool-search overrides.
`--assembler /absolute/as` and `--linker /absolute/ld` select explicit tools.

Useful helper flags:

- `--optimize-ir`: run the canonical verified MIR passes.
- `--pic`: request native PIC and link a PIE.
- `--emit-only`: publish assembly instead of an executable.
- `--no-entry --emit-only`: omit the C `main` wrapper for library assembly.
- `--ir-input`: start from serialized MIR, still decoded and verified.
- Repeated `--on1xc-arg=VALUE`: preserve ordered frontend/module/PPX flags.
  For example `--on1xc-arg=-I --on1xc-arg=/absolute/module/path`.
  Output, backend, mode and target overrides are rejected.
- Repeated `--cflag=VALUE`: allowlisted optimization/debug/sanitizer flags.
  Target/ABI overrides and fast-math flags are rejected.
- `--timeout`, `--memory-mib`, `--max-assembly-bytes`, `--max-output-bytes`:
  finite wall-clock, address-space and artifact budgets.
- `--verbose`: print exact JSON argument arrays for each external invocation.

Each build uses a private staging directory beside its destination and publishes
only the complete artifact with an atomic rename. Invalid source/MIR, mismatched
target, compiler failure, limit exhaustion and cancellation preserve the previous
output. SIGINT/SIGTERM or deadline expiry terminate the external process group.
The standalone tool also checks its target, bounds MIR/assembly bytes, polls
cancellation, rejects nonregular MIR input and atomically publishes file output.

## Reproducible focused tests

```sh
python3 on1x/backends/mir-archon/tests/test_native_driver.py \
  --build-dir "$BUILD" --cc /usr/bin/cc --sdk
ctest --test-dir "$BUILD" --output-on-failure \
  -R 'on1x.backends.mir_archon_driver|on1x.backends.mir_archon_native_build'
```

The Python test compiles and executes actual generated code, compares C11 output,
tests optimized/PIC/MIR/library emission, and exercises target, malformed input,
unsafe flag, resource, deadline, signal-cancellation and atomic-output failures.
`TestDriver.cpp` separately checks private-MIR SDK dispatch, deterministic output,
schema/target checking, cancellation, size budgets and native option forwarding.
