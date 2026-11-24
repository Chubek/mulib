# Native tools for On1xfile projects

The C11 stages of a hosted `c11` or `mir-c11` project default to **Cephyr**,
**SandAS**, and **Yellowstone ld**:

```text
compiler cephyr
assembler sandas
linker yellowstone
linker-arg "--gc-sections"
```

The directives are permitted only in a hosted executable or complex project's
root `On1xfile`. Dependencies, nested manifests, `On1xfile.m`, and `On1xfile.i`
cannot select tools or pass tool arguments. `on1xc aux build` and `on1xc exec
build` accept `--compiler`, `--assembler`, `--linker`, `--assembler-arg=ARG`,
and `--linker-arg=ARG`. Each argument directive or flag supplies one literal
argument. Root arguments precede CLI arguments, preserving order within each
stage. Each CLI stage selection overrides a root selection for that stage.

For the existing, system-libc build (runtime bridges, MuLIB archives, host
DWARF, or sandboxed builds), explicitly set all three stages to `host`, or
pass `--cc cc` to select host for stages not otherwise set:

```sh
on1xc aux build ./hello --cc cc --assembler-arg=--noexecstack \
  --linker-arg=-z --linker-arg=relro
```

`--cc /path/to/driver` can also select a specific trusted host driver. The
existing single-file helper remains host-by-default and accepts the same stage
options for ordinary C11 executable builds. MIR-Archon, VM, ArchVirt, and the
separately selected FFI and KernelBind profiles keep their own tool contracts.

Tool arguments are restricted to options that do not choose executables,
output files or load scripts, plugins, or response files. For SandAS, use
`--feature NAME` or `-F NAME` as separate arguments. For a host assembler,
common warning, `-g` and `--noexecstack` switches are accepted. The linker
allowlist includes strip-debug, section GC and no-undefined options; the
host linker also accepts safe `-z` values and `-rpath PATH` for dynamic
system-runtime links. The selected tool may reject an option it does not
implement. There is no shell expansion or comma splitting, including in a
host rpath.

## Availability and ABI

On native Linux AMD64 LP64, put an installed `cephyr` on `PATH` or pass
`--cephyr-prefix /path/to/installation`. On1x uses the Cephyr CLI with an
explicit configuration and reads the AMD64 startup, static libc, SandAS
executable and AMD64 AT&T bank, and `qobjld` from that installation's
`share/cephyr/native-packages` (or the matching build tree beside `cephyr`).
This CLI path does not require `ON1X_WITH_CEPHYR`; that CMake option enables
On1x's separate embedding APIs and kernel helper, not the project pipeline.

Cephyr produces assembly, the selected assembler produces AMD64 ELF
objects, and the selected linker produces a static executable against the
**same Cephyr startup and libc**. Cephyr can use a host assembler or linker
explicitly without switching libc. Mixing a host C compiler or system libc
with the LibStone project stages is not qualified. In particular, native
Yellowstone does not implement all hosted-libc ABI features (including ELF
symbol versions, IFUNC and TLS); On1x does not invoke its external bridge.
`on1xc aux plan` lists the selections, arguments, runtime, missing inputs,
and fingerprints of discovered files. Presence is not a compilation probe.

The pinned Cephyr supports a limited AMD64 C profile. A scalar program with no
On1x runtime imports can build with all three native tools. A program that
imports the On1x runtime needs its actual translation unit; the pinned Cephyr
rejects that unit's AMD64 by-value aggregates (`on1x_regex_capture_v1`,
`on1x_regex_count_v1`, and `on1x_regex_index_v1`). This blocks even `println`
through the full runtime. Native bridges and host-linked archives remain
available through explicit host selection; no stub, host object, system CRT,
or alternate linker is substituted.

Cephyr's C stage uses its baseline `-O0`, without DWARF, for the regular
built-in development/release profiles. On1x HIR/MIR optimization settings
remain separate. An explicit `debug` profile or incompatible custom Lua C
profile is rejected. The external native package is not currently mounted
under `--sandbox`; use the host stages for that build mode.
