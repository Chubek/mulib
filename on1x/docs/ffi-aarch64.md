# Checked-C11 FFI on Linux AArch64

The target is little-endian Linux `aarch64-linux-gnu`, LP64, AAPCS64. The
frontend uses one target-matched, compiler-owned catalogue. It validates the
same HIR declarations, uses, unsafe context and guard plan as AMD64. The
generated C caller checks descriptor guards after evaluating its operands
left-to-right, once each. The adapter checks the exact target macros, C
primitive layout and prototype before forwarding. The native build then
links a separately compiled prototype witness, adapter and implementation,
checks the target ELF objects and runs an independent numerical probe.

The initial executable profile is **unsafe static scalar imports only**.
The driver still requires an independent probe for every selected export:
the three integer clamp wrappers and the existing saturating counter fixture
are the only probe shapes. A target-valid catalogue entry alone does not
qualify a build. Safe AArch64 imports, including review-candidate publication,
and `[dynlib]` are explicitly closed. The existing AMD64 independent
signed-review path is unchanged. The AArch64 Linux binary64 language runtime
is not qualified here; do not infer an executable floating-point or aggregate
profile from the C compiler's ABI support.

## Tool selection and cross qualification

Use a genuine `ffgen` AArch64 catalogue, with the supported clamp exports
explicitly selected. For the pinned clamp header, the distinct AAPCS64
guard fingerprint is `3abc94c6a56223e19accc4bbd1832df7566e7d0e81d556d398834218db94dd4a`
for `mulib_clamp_i64`, with `requires 1 <= 2`. The native parameter and
return identity is C `long`, not merely a 64-bit integer. The index must be
generated and validated normally even for a one-module qualification build.

On Linux AMD64, configure a frontend that deliberately emits AArch64 FFI C:

```sh
cmake -S on1x -B "$BUILD" \
  -DON1X_FFI_TARGET=aarch64-linux-gnu -DON1X_FFI_CROSS_C11=ON \
  -DON1X_FFI_CATALOG="$CATALOGUE" \
  -DON1X_LIBSTONE_ROOT="$PINNED_LIBSTONE" \
  -DON1X_FFI_AARCH64_TEST_CC="$CC_AARCH64" \
  -DON1X_FFI_AARCH64_TEST_QEMU="$QEMU_AARCH64"
cmake --build "$BUILD" -j2 --target on1xc
ctest --test-dir "$BUILD" --output-on-failure -R '^on1x\.ffihandler\.aarch64_c11$'
```

For the embedded Cephyr combinations add `-DON1X_WITH_CEPHYR=ON`
and `-DCephyr_DIR="$CEPHYR_DIR"` to configure, and build
`on1x-cephyr-kernel` as well. Use the embedding ABI 1 library from the
same pinned LibStone revision, installed with the `AArch64` native package,
`aarch64.symas` bank, and Sandstone assembler. Cephyr compiles the chosen
binding objects; the final program and runtime still use the specified GNU
target toolchain. The test runs the GNU/GNU, GNU/Cephyr, Cephyr/GNU and
Cephyr/Cephyr implementation/adapter combinations when requested. It also
tests target mismatch, unsafe context, guard failure and absence of safe or
DSO publication. No result silently skips an explicitly requested tool.

```sh
python3 on1x/tools/on1x-build.py input.on1x -o program \
  --on1xc "$BUILD/on1xc" --ffi-provider host --cc "$CC_AARCH64" \
  --ffi-cross-runner "$QEMU_AARCH64" --mulib-root "$PWD"
"$QEMU_AARCH64" ./program
```

Both cross-tool paths must be absolute. `--ffi-provider host` means the
explicit GNU C driver in `--cc` here, **not** the AMD64 compiler. `--cc`
must report the target triple and find the AArch64 Linux GNU static headers,
CRT and libraries without ambient compiler overrides. `--ffi-cross-runner`
selects an operator-trusted QEMU user executable for the driver-owned probe;
it never admits a safe import and the driver never runs the application.
Cross outputs are static `EM_AARCH64` executables with no interpreter.
Native AArch64 instead configures with its catalogue and without
`ON1X_FFI_CROSS_C11`, and does not pass a cross runner.

## Why not MIR/Archon or general Cephyr emission?

Pinned LibStone Cephyr has a real AAPCS64 native object provider. This path
uses it through `on1x-cephyr-kernel --object INCLUDE RUNTIME aarch64-linux-gnu`;
the general `on1xc --emit-cephyr-object` profile remains unchanged.
On1x's Archon bridge recognizes `AArch64/AAPCS64`, but
`on1x/hir/lower_mir.cpp::function_profile` rejects checked foreign declarations
before the native backend (`checked foreign calls require typed foreign MIR
descriptors (direct C11 only)`). The executable Archon driver additionally
qualifies only AMD64. Selecting the pinned AArch64 Archon package does not
transport a foreign descriptor, verifier state or admission into MIR. This
change neither removes those checks nor claims Archon FFI availability.
