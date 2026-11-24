# Independent FFI qualification

Use a fresh output directory for every run and keep it outside the source
tree. Qualification evidence is tied to the configured source revision, CMake
cache, selected tool paths, and host. A successful C invocation below is not
evidence that an On1x import is callable or safe.

## CTest inventory and regression suites

After configuring and building the exact source snapshot, run:

```sh
python3 -B on1x/hir/FFIHandler/tests/integration/ctest_matrix.py \
  "$BUILD" "$LOGS/ctest" --jobs 4
```

The driver runs `ctest -N` for every group and saves the CTest JUnit output, the
compiler/feature options, and the selected/pass/fail/skip/not-run counts.
Groups overlap and must not be summed. `--group` is repeatable. An empty
required group is an error. For features deliberately compiled out, document
each absence explicitly, e.g. `--absent 'native=ON1X_WITH_CEPHYR=OFF'`.
This also asserts that the filter finds no cases; it does not qualify the
absent feature. `--list-only` is discovery, not execution. The `trust-dso`
group currently names the legacy review/DSO tests so their retirement cannot
silently count as a pass.

Use separate build trees for GCC, Clang, and fallback scanner with optional
optimization/integration features disabled. Enabling the legacy KernelBind
path in a reference configuration also requires
`ON1X_WITH_EXOML`, `ON1X_WITH_EXOML_EXTRACT`, `ON1X_WITH_EXOTRACT` and
`ON1X_WITH_EXOTRACT_LUA`. An embedding run additionally requires
`ON1X_WITH_CEPHYR=ON` and `Cephyr_DIR` pointing to the built pin's installed
CMake package; a CLI binary alone does not exercise embedding.

## Projection ≠ executable ABI

```sh
python3 -B on1x/hir/FFIHandler/tests/integration/projection.py \
  "$LOGS/projection" --cephyr "$CEPHYR" --libstone-root "$LIBSTONE"
python3 -B on1x/hir/FFIHandler/tests/integration/native_boundary.py \
  "$LOGS/native-host" --cc gcc --other-cc clang
python3 -B on1x/hir/FFIHandler/tests/integration/native_boundary.py \
  "$LOGS/native-cephyr" --cc gcc --cephyr "$CEPHYR"
python3 -B on1x/hir/FFIHandler/tests/integration/native_boundary.py \
  "$LOGS/native-sanitized" --cc clang --sanitizers
```

The projection test invokes the selected upstream interface suite. It separately
projects actual clamp and container-core headers for AMD64, AArch64 and RV64,
checks type identity and linkage, and compares direct/preprocessed/C11Syntax
routes for clamp. `--reader-tools on1x/hir/FFIHandler/tools` additionally tests
the migrated decoder against those outputs and mutated wire data.
`--allow-media-missing-math` records the specific bundled-header limitation as
unsupported; it does not hide other diagnostics as skips. Neither the upstream
test nor these projections execute cross-target code.

The AMD64 probe compiles distinct C caller/wrapper and implementation TUs
against actual public headers, verifies a strict same-width wrong primitive
is rejected, and proves a missing native symbol fails to link. Deterministic
Python integer/GCD oracles check host, mixed compiler, header-only, and actual
DSO execution. The DSO test checks the dynamic symbol table and `DT_NEEDED`,
runs the executable, then checks a relocated copy without its adjacent DSO
fails at load time. This is not a test of trusted DSO admission.
`--sanitizers` instruments the test caller, C wrapper, implementation, and
host inline and DSO paths, not the On1x compiler or LibStone.

## Generation and input drift

With the integrated generator and a genuine built LLSynth parser:

```sh
python3 -B on1x/hir/FFIHandler/tests/integration/generation.py \
  "$LOGS/generation" --mulib-root "$PWD" --other-mulib-root "$SAME_REV_OTHER_ROOT" \
  --cephyr "$CEPHYR" --libstone-root "$LIBSTONE" --parser "$GENERATED_PARSER" \
  --tool-input "$LIBCEPHYR" --tool-input "$LIBSTONE/cornerstone/Cephyr/triples" \
  --tool-input "$LIBSTONE/cornerstone/Archon/stdspec"
```

Generates actual clamp in two checkout locations, compares the exact bytes,
checks that the audited interval rule survives, and verifies that prefix
matching alone never grants ownership. Negative controls reject an unexpected
generated file, a stale authored signature, and a transitive `stdint.h`
change; known-good output is unchanged on rejection. This is a one-module
test, not a substitute for the full-manifest `--all --check`.

## Real On1x and installed tool

```sh
python3 -B on1x/hir/FFIHandler/tests/integration/on1x_clamp.py \
  "$LOGS/on1x-clamp" --on1xc "$BUILD/on1xc" \
  --driver on1x/tools/on1x-build.py --cc gcc --mulib-root "$PWD"
```

This script builds real static and dynamic clamp applications, checks a
left-to-right single-evaluation trace even on guard exit 70, checks lexical
unsafe scope, and runs the published executables with no tools on `PATH`.
A generated candidate must not self-authorize. The script creates ephemeral
Ed25519 test authorities and rejects a wrong key, modified signed bytes, a
valid signature on the wrong target, and a valid signature with a stale DSO
digest. Rejected builds must not publish output.
`--legacy-exotract /path/to/exotract` selects the reference frontend only.
`--expect-safe-dso-unsupported` records the explicit rejection by an
incomplete migrated implementation; it is not a safe-DSO pass.

For an installed run, install to a disposable prefix, then pass
`--on1xc "$PREFIX/bin/on1xc"` and
`--driver "$PREFIX/share/on1x/tools/on1x-build.py"`. Omit `--mulib-root` to
test staged dependency discovery rather than silently referring to the
source checkout. Record that separate result.

## Gate interpretation

Retain the reference tests for legacy reviewed static/DSO builds (test keys
only), wrong-key/signature and stale artifact subjects, numerical and IPC
parity, project and installed tool. Positive C oracle results do not replace
the real Cephyr → generated `.ffi` → LLSynth → checked HIR → native On1x path.
For any removed profile or absent test, record its replacement or qualify it
as blocked. An all-skipped group is not a passing gate.
