# On1x verified MIR → Archon native MachineIR

This is a real, target-pinned native assembly backend, **not a text emitter,
C11 fallback, Gated SSA adapter or stub**. `Bridge.cpp` lowers verified On1x MIR
v1, v2 and v3 into Archon's language-neutral MachineIR v1 and calls the
**generated native** `Archon::Backend` C API. The generated Archon backend
produces assembly; ordinary assemblers/linkers and the On1x runtime are
required to produce an executable.

**Repository integration is intentionally explicit.** The original package at
`eeedc2aed7db341be86c6c281582d7f46b1a564b` lacked SDK native admission.
The integrated SDK now admits this compiler-linked provider's target-bound
assembly through an explicitly selected native path. External plugins gain no
native artifact authority. The standalone `on1x-archon-mir` tool also works with
the SDK disabled. See [INTEGRATION.md](INTEGRATION.md) and [DRIVER.md](DRIVER.md).

## Coverage and contract

- Fully verified MIR v1–v3, using On1x's authoritative `mir::verify`.
- `i64`, `bool`, `u8/u16/u32/u64`, `f32/f64` bit-pattern scalars, nested tuple
  values, block parameters, direct calls, recursion, stack slots, and the
  source's checked/IO/effect sequencing.
- Exact On1x checked signed integer semantics, including remainder and unary
  negation, by calling the existing `on1x_*_i64` runtime helpers.
- All allowlisted MIR v3 numeric primitives lower to native integer operations
  or `on1x_archon_*` integer-register ABI wrappers of the existing On1x C
  runtime. `ScalarABI.c` has 142 deterministic, generated wrappers; use
  `generate_scalar_abi.py --check` for reproducibility.
- Runtime printing/assertions use the On1x runtime; floats cross the native
  scalar ABI as integer bit payloads so no undeclared FP-register calls occur.
- Tuples use bounded, immutable stack-backed flattened words and hidden
  result-buffer arguments for tuple-returning functions. This avoids escaping
  pointers to the callee's frame. Tuple-bearing CFG edges are split: every
  source tuple is first staged, then copied into a dedicated destination phi
  slot. This preserves simultaneous swaps and loop-carried values without
  aliasing dynamically reused temporary stack storage.
- No unchecked host-side signed arithmetic in the bridge. MachineIR's ordinary
  wrapping division/remainder semantics are never substituted for On1x's
  trapping operations.
- ABI parameters map to Archon's initial SSA IDs; On1x entry-block parameters
  do **not** become Archon block parameters. Non-entry block parameters and
  edges are translated with dense per-function IDs and exact types.
- No assembly publication on error. `Tool.cpp` uses an atomic POSIX temporary
  file and rename for `-o FILE`.

## Build prerequisite: generated Archon backend

See [TOOLCHAIN.md](TOOLCHAIN.md) for exact paths and real-toolchain evidence.

From the repository root, provision the bundled LibStone canonical Linux
AMD64 SysV backend, run its native scalar GAS tests, and install it:

```sh
python3 on1x/backends/mir-archon/provision_toolchain.py \
  --work-dir /mnt/andromeda/tempdir/opencode/on1x-archon-toolchain --jobs 4
cmake -S on1x -B /mnt/andromeda/tempdir/opencode/on1x-archon-build \
  -DCMAKE_PREFIX_PATH=/mnt/andromeda/tempdir/opencode/on1x-archon-toolchain/install \
  -DON1X_BUILD_MIR_ARCHON=ON -DON1X_ARCHON_ENABLE_SDK_ADAPTER=ON \
  -DON1X_ARCHON_TARGET_TRIPLE=x86_64-unknown-linux-gnu -DBUILD_TESTING=ON
cmake --build /mnt/andromeda/tempdir/opencode/on1x-archon-build --parallel 4
ctest --test-dir /mnt/andromeda/tempdir/opencode/on1x-archon-build \
  -R 'on1x.backends' --output-on-failure
```

The provisioning script accepts `--libstone-root` for an explicitly selected
checkout, performs no fetch or checkout, and records its revision and install
prefix in `on1x-toolchain.json`. Its upstream tests report unavailable
cross-target runners as skipped; AMD64 execution requires an AMD64 Linux host.

Equivalent manual generation from `libstone/cornerstone/Archon`:

```sh
python3 -m archon compile stdspec/amd64/root.arch -o amd64.archdb
python3 -m archon irgen gated --archdb amd64.archdb \
  --profile native --abi SysVAMD64 --no-oracle -o generated-amd64
cmake -S generated-amd64 -B build-amd64
cmake --build build-amd64 --parallel
cmake --install build-amd64 --prefix "$HOME/.local/archon-amd64"
```

For RV64IM use `stdspec/riscv/rv64im.arch`, `RISCV64IM` and `LP64`;
for AArch64 use `stdspec/aarch64/root.arch`, `AArch64` and `AAPCS64`.
The native backend is generated *once per target*, not once per source file.
`Archon-IRGen.hpp` is the **host-side generation** facade; it is not the
code-generation interface used by `Bridge.cpp`. The latter consumes the
installed generated `Backend.h` and `Archon::Backend`.

The opt-in `on1x/backends/CMakeLists.txt` wiring is included. Configure On1x with
`-DCMAKE_PREFIX_PATH="$HOME/.local/archon-amd64"` and build. This directory
adds `on1x_archon_bridge`, `on1x_archon_scalar_abi` and `on1x-archon-mir`.
The optional SDK adapter requires explicit target configuration and trusted
native artifact admission.

## Standalone use

```sh
on1xc --emit-ir hello.on1x -o hello.mir
on1x-archon-mir --ir-input hello.mir -o hello.s
# Optional: --pic; --no-entry builds a library without a C main.
```

Compile/link `hello.s` with **both** `on1x_archon_scalar_abi` and
`on1x_runtime`, along with the On1x runtime's platform dependencies. For
example, for a compatible Linux executable, a build script may assemble the
GAS source and link against the two C static libraries. Use `-lm` if required
by the On1x runtime; link ordering matters for static archives. The generated
assembly deliberately does **not** embed runtime C code or invoke external
programs from the compiler callback. The `on1x_archon_scalar_abi` library is
linked into the produced executable, **not** into the `on1xc` compiler merely
because the backend is enabled.

Native MachineIR assumes the generated target's documented scalar LP64 ABI.
Floating-point On1x runtime functions are currently qualified on Linux
AMD64/SSE2 only; the bridge rejects such MIR for other generated native
targets, rather than quietly publishing assembly with unsatisfiable runtime
imports. No FP/vector MachineIR type or cross-ABI FP call is assumed.

## Limits and diagnostics

- All On1x verifier budgets apply. Tuple flattening is bounded to 4096 words
  per tuple and locals to 32 KiB individually. The actual generated native
  backend may reject a function earlier due to its 64 KiB fixed-frame budget,
  `ARCHON_MIR_MAX_PARAMS` ABI limit (128 for the pinned generated backend,
  including any hidden tuple-result pointer) or other target-specific constraints.
- One Archon native backend build serves one target/ABI. The SDK adapter checks
  the requested triple against its configured triple and generated ISA family.
- Assembly output defaults to at most 16 MiB; generated work is further
  bounded to 65536 MachineIR instructions per module and per function to
  8192 SSA values, 32768 MachineIR instructions and
  32 KiB of explicit local storage. Callers can tighten these limits through
  `Options`; cancellation is forwarded to `ArchonBackendOptions`.
- `Product.assembly` is source text, not an executable, ELF object or JIT code.
  Neither the bridge nor the SDK adapter executes generated code.
- `ARCHON_BACKEND_PIC` is supported only if the generated Archon target supports
  it; unsupported flags cause a diagnostic, not a fallback.
- `MachineModule::view()` is borrowed and is valid only while the owning
  `MachineModule` lives. Do not retain its pointers beyond the call.
- Do not enable unsafe pure-instruction scheduling on this bridge: verified
  On1x effect order is preserved in the incoming MachineIR instruction stream.

## Tests

See [scalar ABI qualification](ABI.md) for runtime signature coverage,
cross-translation-unit native calls, exact traps and sanitizer scope.

`tests/TestBridge.cpp` exercises MIR v1 loops/slots/calls, v2 nested tuple
returns and float values, v3 unsigned/binary32 operations and deterministic
assembly. `tests/TestTuplePhi.cpp` exercises cyclic loop-carried tuple phi
values, edge splitting and simultaneous swaps. `generate_scalar_abi.py --check`
checks generated wrapper reproducibility. `ctest` enables these when
`BUILD_TESTING` is set.

### Source revisions used for this implementation

- `Chubek/Mulib`: `eeedc2aed7db341be86c6c281582d7f46b1a564b`
- `Chubek/libstone`: `af00e39efb8959a72969907704f0d473e72365b7`

All new files are designed around those interfaces. A future Archon MachineIR
schema bump, On1x MIR opcode addition or SDK policy change must trigger a
re-review; consult `AGENTS.md`.
