# Static checked-C11 FFI on RV64IM

This is an **unsafe, static cross-build profile**, not a MIR/Archon FFI
profile or a safe admission. It uses the normal compiler-owned descriptor
and checked HIR, the generated C11 caller and adapter, and real RV64IM
machine code. The independent C probe runs under explicitly selected
QEMU-user before the application is published. The application itself is not
run by the driver. Only bindings with a qualified probe are callable; the
executed example uses the three canonical `mulib.clamp` functions.

The descriptor identity is `riscv64-linux-gnu` / `LP64` / `LP64` / `lp64`.
The first `LP64` is the C data model, the second the RISC-V integer (soft-float)
ABI. `riscv64-linux-gnu` by itself does **not** identify that calling
convention; the usual GNU RV64 glibc cross toolchain may be LP64D despite
reporting the same triple. The C11 witness and adapters check the ABI macros
and exact prototypes. The driver checks each object and link output for
little-endian `EM_RISCV` ELF64 and `e_flags == 0`, not an LP64D or RVC object.
The link fixes Clang's RV64IM LP64 code-generation flags and uses the explicit
LLD executable with pinned Cephyr static CRT/libc. There is no fallback to a
system sysroot, host linker or different FFI provider.

## Build and run

Start with an AMD64 Linux host, trusted absolute paths to Clang, LLD and
`qemu-riscv64`, and an installation of pinned LibStone Cephyr
`e10ac69eed7baf91b06d15c4e0d58360097cb5c4` containing the RV64IM package.
The package includes `share/cephyr/stdlib/include` and
`share/cephyr/native-packages/RISCV64IM/{crt0.o,libc.a}`. Use a fresh, separately
generated `riscv64-linux-gnu` FFI catalogue, not the checked-in AMD64 one.
For this example, project only `--kernel clamp` with `ffgen.py`, selecting
`mulib_clamp_add_i64`, `mulib_clamp_i64`, and `mulib_clamp_sub_i64` in the
target-specific override. The `1 <= 2` guard for `mulib_clamp_i64` binds its
RV64IM LP64 native fingerprint
`8c8801dcf37730919a7a81b1b8ae650f86133d2b83886a5c88675a3d7e4da2ec`.

```sh
cmake -S on1x -B "$BUILD" \
  -DON1X_LIBSTONE_ROOT="$LIBSTONE" \
  -DON1X_FFI_CROSS_C11=ON -DON1X_FFI_TARGET=riscv64-linux-gnu \
  -DON1X_FFI_CATALOG="$RV64_CATALOGUE" \
  -DON1X_WITH_CEPHYR=ON -DCMAKE_PREFIX_PATH="$CEPHYR_PREFIX"
cmake --build "$BUILD" --target on1xc --parallel 2

python3 -B on1x/tools/on1x-build.py on1x/examples/kernel/clamp.on1x -o "$OUT" \
  --on1xc "$BUILD/on1xc" --no-prelude --cc "$CLANG" \
  --ffi-provider cephyr --ffi-adapter-provider host \
  --ffi-rv64-toolchain "$CEPHYR_PREFIX" --ffi-rv64-linker "$LLD" \
  --ffi-cross-runner "$QEMU_RISCV64"
"$QEMU_RISCV64" "$OUT"
```

`--ffi-provider host` denotes compilation with the selected Clang driver; it
does not mean emitting AMD64 objects. Either host/Clang or embedded Cephyr
can provide the static clamp implementation and the scalar adapter. Do not
run QEMU on untrusted code relying on it for containment; it is an emulator,
not an On1x sandbox.

The driver explicitly rejects safe imports (including review-candidate
requests), DSO imports, extra C flags, external libraries, and native Basis
companions requiring libm. No signature, declaration, ABI probe or successful
link confers safe admission. The probe allowlist deliberately does not grant
execution of arbitrary projected scalars, rich layouts, or callbacks.

## Verification and remaining boundaries

```sh
python3 -B on1x/tests/ffi_rv64_toolchain.py --work "$WORK" \
  --cc "$CLANG" --linker "$LLD" --runner "$QEMU_RISCV64" \
  --cephyr "$CEPHYR_PREFIX/bin/cephyr" \
  --headers "$CEPHYR_PREFIX/share/cephyr/stdlib/include" \
  --runtime "$CEPHYR_PREFIX/share/cephyr/native-packages/RISCV64IM"
python3 -B on1x/tests/ffi_rv64_workflow.py --work "$WORK" \
  --on1xc "$BUILD/on1xc" --cc "$CLANG" --linker "$LLD" \
  --runner "$QEMU_RISCV64" --cephyr-prefix "$CEPHYR_PREFIX"
```

The toolchain test uses a hand-written ABI oracle, separate from both
compilers, checking integer register placement, a binary64 passed and returned
in integer registers, unsigned 32-bit register extension, 16-byte stack
alignment, and the first stacked argument. It also checks real LP64D ELF
rejection and confirms that linker section GC cannot remove an unresolved
rooted prototype relocation. The workflow test exercises all four provider
pairs, a failing caller guard and unsupported modes. The binary64 oracle
checks the toolchain ABI; it is **not** a general floating-point foreign-call
admission.

Both commands passed on Linux AMD64 with Clang and LLD 22.1.8,
QEMU-user 11.1.3, and Cephyr 0.1.0 built from the pinned revision above. The
target catalogue used only the single `clamp` module. The generated caller
printed the reference clamp vector for each provider pair, and a reversed
bounds call exited 70 before calling C.

Pinned standalone Archon can generate its integer RV64IM/LP64 backend:

```sh
# Run from LibStone cornerstone/Archon, with an external WORK directory.
python3 -B -m archon compile stdspec/riscv/rv64im.arch -o "$WORK/rv64im.archdb"
python3 -B -m archon irgen gated --archdb "$WORK/rv64im.archdb" \
  --target RISCV64IM --profile native --abi LP64 --no-oracle -o "$WORK/archon-lp64"
```

Repeating the second command with `--abi LP64D` fails at that revision:
`archon: error: compiled target does not support the requested native ABI`.
Enabling LP64D would require real upstream ABI facts including the hardware
floating calling convention, Cephyr target/headers and lowering, and separate
cross-compiled and executed witnesses. No vendored sources were changed.
Independently of LP64D, On1x's HIR→MIR lowering rejects checked foreign calls
until MIR can carry and verify typed foreign descriptors and effects.
`--backend mir-archon` does not accept `--ffi-provider`, nor does this
checked-C11 profile change `--emit-cephyr-object` or the target coverage of
`on1x-native-build.py`.

As a separate non-FFI control, `on1xc --emit-ir` followed by
`on1x-archon-mir --ir-input` emitted RV64IM assembly for
`fn main() { println(42); }`. Compiling it with
`clang --target=riscv64-linux-gnu -march=rv64im -mabi=lp64 -mno-relax -c` and
statically linking it with the same Cephyr CRT/libc and On1x runtime printed
`42` under QEMU. That control does not establish MIR foreign-call support.
