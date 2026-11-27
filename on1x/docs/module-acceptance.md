# Module integration acceptance

Run the acceptance suite against a compiler built from the same checkout:

```sh
mkdir -p build/module-acceptance/tmp
export TMPDIR="$PWD/build/module-acceptance/tmp"
python3 -B on1x/tests/test_module_acceptance.py \
  --on1xc /absolute/path/to/on1xc --cc cc --work-dir build/module-acceptance
python3 -B on1x/tests/test_interfaces.py --on1xc /absolute/path/to/on1xc --cc cc
```

The module suite compiles and runs actual generated C11. It checks named and
colon-selector module and program imports next to quoted legacy imports,
visibility after aliasing and field projection, source-kind and name
validation, program entry-point isolation, fail-closed program capability
admission and legacy PPX, multiple manifest fragments, deterministic
dependency reporting, and signature enforcement on the actual build. Failure
checks verify that neither an existing emitted file nor a published project
artifact is replaced. It also runs the portable backend matrix below on both
the legacy and sealed-module versions of the energy-budget example; keep
`on1x-vm` from the same build next to `on1xc`. The legacy interface suite is
separate: its v1 contract checks are not an export allowlist.

## Backend smoke matrix

To rerun only the two-variant matrix, append
`Modules.test_energy_budget_legacy_and_sealed_portable_backends` to the module
suite command above. Both variants must match `expected.txt` with no runtime
diagnostics, pass bytecode verification, and match the same output when
evaluated from saved MIR. To manually exercise just the legacy project, run
from the repository root with a fresh work directory under `build/`:

```sh
set -eu
COMPILER=/absolute/path/to/on1xc
WORK="$PWD/build/module-acceptance/energy"
mkdir -p "$WORK/tmp"
export TMPDIR="$WORK/tmp"
cp -R on1x/examples/energy-budget "$WORK/project"
for route in c11 mir-c11 mir-vm; do
  python3 -B on1x/tools/on1x-project.py build "$WORK/project" \
    --on1xc "$COMPILER" --cc cc --backend "$route" --output-dir "$WORK/$route"
  "$WORK/$route/energy-budget" >"$WORK/$route.stdout" 2>"$WORK/$route.stderr"
  cmp "$WORK/project/expected.txt" "$WORK/$route.stdout"
  test ! -s "$WORK/$route.stderr"
done
"$(dirname "$COMPILER")/on1x-vm" --verify "$WORK/mir-vm/energy-budget.o1vm"
```

Report each route separately. C11 and MIR-to-C11 qualify their respective C
toolchain routes. The VM wrapper and bytecode verifier qualify the portable VM
route. None of these results qualifies an optional native MIR backend.
Likewise, declaration-only checks of opaque signature types do not qualify an
opaque runtime ABI. A backend or signature form not tested should be reported
as unqualified rather than inferred from another route.

## Native ASDL schema artifacts

Use the `asdlc` binary from the pinned LibStone build. Pass relative input
paths so that source paths recorded in the artifacts are reproducible.

```sh
ASDLC=/absolute/path/to/asdlc
SCHEMAS="$PWD/build/module-acceptance/schema"
mkdir -p "$SCHEMAS"
(
  cd on1x/grammar
  for schema in On1x PPX; do
    "$ASDLC" --check --schema "$SCHEMAS/$schema.json" --dot "$SCHEMAS/$schema.dot" \
      "$schema.asdl"
    dot -Tsvg "$SCHEMAS/$schema.dot" -o "$SCHEMAS/$schema.svg"
  done
)
```

See [module coordination](module-coordination.md) for the standalone IPC
acceptance command; it does not need a compiler build and uses isolated
session IDs.
