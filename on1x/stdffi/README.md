# Standard MuLIB FFI

The generated catalogue lives in `mulib/<target>/`, keyed by the full target
triple. The supported projection targets are `x86_64-linux-gnu` (SysVAMD64),
`aarch64-linux-gnu` (AAPCS64), and `riscv64-linux-gnu` (RV64IM, soft-float
LP64, **not** LP64D or LP64F). Missing target directories are errors, not
requests to reuse the AMD64 catalogue or the old flat layout.
The exact MANIFEST key names the file (`media-timebase.ffi`), while the
canonical On1x alias names the module (`mulib.media_timebase`). A complete
catalogue has one file for every non-reserved MANIFEST registration and one
`index.json`. Do not edit generated files. Change the canonical header, or use
an audited semantic delta under `overrides/mulib/<target>/`; overrides cannot
invent declarations or grant admission. The existing flat `overrides/mulib/`
files are an AMD64-only compatibility input if that target has no override
directory. `--overrides` selects an explicit directory; the declarations
inside must still match the selected target and module.

In On1x source, `unsafe import foreign stdffi.<alias> as Local;` is a
contextual spelling of `unsafe import kernel mulib.<alias> as Local;`.
The suffix is the canonical On1x alias (for example, `ipc_framed`, not
`ipc-framed`). The parser normalizes the prefix before lookup; manifests,
descriptors, source request reports and review identities still use `mulib.*`.
The corresponding `import foreign stdffi.<alias>` requests safe admission; it
does not fall back to unsafe exports. Calls through unsafe aliases still need
`unsafe { ... }`.

The static [framed IPC adapter](../docs/kernel-framed.md) is the available
unsafe-only resource profile. The generated driver has no resource ABI:
neither an unsafe import nor a pointer/aggregate in the catalogue establishes
declaration provenance, a valid native ABI or an On1x runtime representation.

`MANIFEST.yaml` and the planner behind `on1x-mulib.py` define the canonical
header, dependency closure, feature macros and native recipe. Generation
includes only the canonical public header and uses no implementation switches.
Static data, static inline, CDD dependency groups, alternate providers and
multi-header/native companion recipes retain their distinct inventory facts.
Companion files are not automatically exports of the requested public module.
The index records MANIFEST and planner hashes, and a conservative hash
inventory of the entire MuLIB include root (including unregistered detail
headers) and the archived compatibility headers in `.deprecated/`; it does
not claim an exact transitive include trace.

Cephyr's [paired syntax and declaration projection](../docs/ffi-cephyr-evidence.md)
is discovery, not C symbol ownership or proof of binary linkage, lifetime,
effects or safe use. A prefix match by itself does not select
an export. An unavailable description records a reason without fabricating
declarations. A representable function is not necessarily callable. Actual
unsafe and reviewed-safe admission depend on the compiler, native artifact and
explicit policy, not on the index or a successful regeneration.

The index separates registered and attempted modules, accepted projections,
and explicit failure, discovery-only and unsupported outcomes. Its function
counts separate all projected functions from the smaller candidate set, audited
ownership and emitted scalar candidates. Included dependency functions and
ambiguous exports do not become callable just because Cephyr saw them.
Projection failures are also distinct from valid projections with no eligible
exports. For example, the bundled Cephyr header profile cannot project
`media-timebase` because it lacks `math.h`; generation does not replace it with
host libc or a synthetic declaration.

## AMD64 coverage

The checked-in catalogue accounts for 2,053 registrations: all were attempted
and 751 projected successfully. The 2,053 module outcomes are:

| Outcome | Modules | Reason |
|---|---:|---|
| Projection failed | 1,302 | `cephyr_failed` under the bundled header profile; no declarations emitted |
| Discovery only | 634 | Ownership was not established for emitted entries |
| Unsupported after projection | 115 | 114 `no_owned_exports`; 1 `data_only_module` (`lprowse`) |
| Scalar candidates | 2 | Three `clamp` functions and `container-core`'s audited static-inline function |

There are 11,606 projected function occurrences across declaration units
(not distinct library exports): 9,127 non-candidates and 2,479 candidate
outcomes. Of those candidates, 2,358 are ambiguous, 115 have unrepresentable
identifiers, 2 are reserved to other modules, and 4 are explicitly selected
and emitted. These are **not** unsafe or reviewed-safe admission counts.
The existing tracked v1 seed is not an input to this generated catalogue.

## Regeneration and CI

Use `on1x/hir/FFIHandler/tools/ffgen.py` with an explicit Cephyr binary, the
selected LibStone revision and resource inputs, and a genuine FFIHandler
LLSynth parser. The selection
flags are `--all` for a complete refresh and `--kernel <exact-manifest-key>` for
an incremental update. Run a full refresh after inventory or shared generator
inputs change. `--check` regenerates and validates a private stage, then
compares it without modifying the catalogue. Both generation and CI select
`on1x/stdffi/mulib/<target>` by default. `--output` remains available for a
standalone directory (including existing flat fixtures outside the canonical
tree); it cannot select a different target's slot or replace the shared parent.

The pinned LibStone `e10ac69eed7baf91b06d15c4e0d58360097cb5c4` provides all three
targets through the same host `cephyr --emit-interface=sexp` binary. The
projection step needs neither a cross compiler, target libc/sysroot, nor an
emulator with the bundled profile below. It needs Linux `prlimit` and
`renameat2` (for publication), Python 3.10+, Perl for LLSynth, Cephyr and its
matching shared library, the pinned headers, target definitions and Archon
resource bank. Use a build that lists `--emit-interface=sexp` in `--help`; a
reported `Cephyr 0.1.0` alone does not establish which revision was built.
Generation probes the selected ABI before accepting any per-module failures.

From the MuLIB checkout root, generate a real parser and select the explicit
projection inputs:

```bash
ffi=on1x/hir/FFIHandler
# ABSOLUTE paths to an explicit LibStone source checkout and matching Cephyr build.
stone=/abs/path/to/libstone
cephyr_build=/abs/path/to/cephyr-build
work=/abs/path/to/ffi-work
mkdir -p "$work/scratch"
export TMPDIR="$work"
PERL_HASH_SEED=0 PERL_PERTURB_KEYS=0 \
  perl "$stone/cornerstone/LLSynth/scripts/llsynth.pl" \
    --mode=interpret -T python3 -o "$work/FFIHandler.py" "$ffi/FFIHandler.llsynth"
profile=(
  --cephyr "$cephyr_build/cephyr" --parser "$work/FFIHandler.py"
  --libstone-revision "$(git -C "$stone" rev-parse HEAD)"
  --scratch-root "$work/scratch"
  --system-include-dir "$stone/cornerstone/Cephyr/stdlib/include"
  --tool-input "$cephyr_build/libcephyr.so.0.1.0"
  --tool-input "$stone/cornerstone/Cephyr/triples"
  --tool-input "$stone/cornerstone/Archon/stdspec" --jobs 2
)
for target in x86_64-linux-gnu aarch64-linux-gnu riscv64-linux-gnu; do
  python3 -B "$ffi/tools/ffgen.py" --mulib-root . --target "$target" --all "${profile[@]}"
  python3 -B "$ffi/tests/check_stdffi.py" --mulib-root . --target "$target" -- "${profile[@]}"
done
```

For a quick check without Cephyr, replace the `--` and everything after it with
`--inventory-only`. This checks the source inventory, generator and grammar
digests, reconciled counts and package hashes; it does not rerun projection or
establish authenticity. Run the full check against at least two different
absolute checkout locations when qualifying a profile. A reproducible check
needs the same tool binaries and resources, not just the same reported version.

Publication validates a private sibling stage and rechecks the generation
inputs before atomically renaming or exchanging only the selected target
directory on Linux. Writers sharing `mulib/` are serialized by a parent lock;
run the loop sequentially. Separate `--output` parents can run concurrently.
Unexpected validation, input or rename failure leaves the previous output
untouched. A complete refresh removes stale generated modules from the same
transaction. Do not treat `index.json` and `.ffi` files opened independently
across a concurrent refresh as one snapshot: use a stable checkout, or verify
one index's file digests as a set. No generation tools run in applications.

On1x's CMake build selects the exact package for the Linux compiler target
(`ON1X_FFI_TARGET`). An explicit `ON1X_FFI_CATALOG` must point directly at the
matching package, not a root of packages. For a build directory configured
before the layout change, clear the old cached path with `-UON1X_FFI_CATALOG`.
Installation includes only the selected `.ffi` files and index under
`share/on1x/stdffi/mulib/<target>/`, with the generated registry report at
`share/on1x/stdffi/<target>/ffi-manifest.json`.
