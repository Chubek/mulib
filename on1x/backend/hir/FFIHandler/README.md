# MuLIB foreign recipe pipeline

FFIHandler uses one build-host pipeline for MuLIB kernels. It discovers
headers recursively in the ordered, colon-separated `MULIB_KERNELS_PATH`.
Empty components are ignored. If none remain, it scans the checkout's
`kernels/` directory. The first root providing a relative header path wins.

```sh
cmake -S on1x -B build/on1x \
  -DON1X_STDFFI_CEPHYR_LIBRARY=/absolute/path/to/libcephyr.so
MULIB_KERNELS_PATH="$PWD/kernels:/absolute/path/to/more-kernels" \
  cmake --build build/on1x
```

Use a build-host shared library whose Python binding supports `emit_mir`.
`CEPHYR_LIBRARY` or a CMake `Cephyr_DIR` selecting a shared installation also
works. A custom preprocessing environment can be supplied with
`ON1X_STDFFI_CEPHYR_CONFIG`; for example, additional system include paths can
provide headers absent from Cephyr's bundled C library.

Every build scans the configured roots. The generator reuses unchanged output
when it can fingerprint the header, its include dependencies and the Cephyr
inputs; it regenerates changed entries and removes its stale managed files.
The table compiler preserves the generated C++ header when its contents do
not change. To generate the same bundle outside CMake:

```sh
python3 on1x/hir/FFIHandler/ffispec-gen.py \
  --cephyr-library /absolute/path/to/libcephyr.so \
  --output-dir build/ffi
```

## What is generated

- `recipes/<kernel>.ffi`: executable `ffispec 1` for representable functions.
  Cephyr's C11 AST and semantic interface S-expressions must agree on the
  definition and signed `i64` signature. Translation checks that the
  supported, bounded body is defined across its input domain. No kernel is
  given a handwritten exception.
- `catalogue/`: non-executable entries for other headers. They preserve
  compiler evidence or diagnostics rather than pretending an unsupported
  signature, pointer, or effectful body is callable.
- `index.json`: header and output identities, diagnostics, and the executable
  recipe list consumed by FFIHandler.

Cephyr MIR is separate from On1x MIR. The generator uses selected evidence
in Cephyr MIR to assess optimization requests. It does not embed it as
executable On1x MIR or use directives as a substitute for proof.

An LLSynth-generated parser reads the executable recipes. FFIHandler
validates the header identity, expression types, arithmetic and directives,
then compiles immutable tables. Selectors, speculation bits, and function scope
are preserved as On1x MIR directives. MIR verifies rewrites against its own
instructions and facts before producing a backend artifact.

## Source import discovery

`import foreign stdffi.<kernel> [as <alias>];` reads generated executable
recipes from `MULIB_FOREIGNLIB_PATH` in order. If it is unset or has no
nonempty entries, On1x uses the build's `stdffi/mulib/recipes/` or the
installation's `share/on1x/stdffi/mulib/`.

The first matching file is authoritative. It can select exports already
compiled into the compiler's registry, but cannot introduce new executable
code or override a validated signature, guard, body, or directive. Build with
different `MULIB_KERNELS_PATH` roots to add new headers to the registry.

For project-local discovery, see the [`kernel-path` declaration and build-host
command](../../stdlib/on1xfile/README.md#build-host-cephyr-ffi-inputs). That
command generates the same bundle, not a separate import implementation.
