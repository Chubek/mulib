# Standard MuLIB FFI

`mulib/` is the generated catalogue for `x86_64-linux-gnu` (LP64, SysVAMD64).
The exact MANIFEST key names the file (`media-timebase.ffi`), while the
canonical On1x alias names the module (`mulib.media_timebase`). A complete
catalogue has one file for every non-reserved MANIFEST registration and one
`index.json`. Do not edit generated files. Change the canonical header, or use
an audited semantic delta under `overrides/mulib/`; overrides cannot invent
declarations or grant admission.

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

Cephyr's semantic projection is discovery, not C symbol ownership or proof of
linkage, lifetime, effects or safe use. A prefix match by itself does not select
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

## Regeneration and CI

Use `on1x/hir/FFIHandler/tools/ffgen.py` with an explicit Cephyr binary, the
selected LibStone revision and resource inputs, and a genuine FFIHandler
LLSynth parser. The selection
flags are `--all` for a complete refresh and `--kernel <exact-manifest-key>` for
an incremental update. Run a full refresh after inventory or shared generator
inputs change. `--check` regenerates and validates a private stage, then
compares it without modifying the catalogue. Other targets require a different
`--output` directory and cannot replace this catalogue.

From the MuLIB checkout root, use the following CI entry point with the same
explicit projection and parser options used for generation:

```sh
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
python3 -B on1x/hir/FFIHandler/tests/check_stdffi.py --mulib-root . -- \
  --cephyr "$cephyr_build/cephyr" --parser "$work/FFIHandler.py" \
  --libstone-revision "$(git -C "$stone" rev-parse HEAD)" \
  --scratch-root "$work/scratch" \
  --system-include-dir "$stone/cornerstone/Cephyr/stdlib/include" \
  --tool-input "$cephyr_build/libcephyr.so.0.1.0" \
  --tool-input "$stone/cornerstone/Cephyr/triples" \
  --tool-input "$stone/cornerstone/Archon/stdspec" --jobs 4
```

To publish instead, use `python3 -B "$ffi/tools/ffgen.py" --mulib-root .
--output "$PWD/on1x/stdffi/mulib" --target x86_64-linux-gnu --all` with the
same explicit provider/parser arguments after the CI command's `--`.

For a quick check without Cephyr, replace the `--` and everything after it with
`--inventory-only`. This checks the source inventory, reconciled counts and
package hashes; it does not rerun projection or establish authenticity. Run
the full check against at least two different absolute checkout locations
when qualifying a profile. A reproducible check needs the same tool binaries
and resources, not just the same reported version.

Publication validates a private sibling stage and rechecks the generation
inputs before atomically renaming or exchanging the directory on Linux.
Unexpected validation, input or rename failure leaves the previous output
untouched. A complete refresh removes stale generated modules from the same
transaction. Do not treat `index.json` and `.ffi` files opened independently
across a concurrent refresh as one snapshot: use a stable checkout, or verify
one index's file digests as a set. No generation tools run in applications.
