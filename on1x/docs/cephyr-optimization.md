# Native optimization and diagnostics

`--emit-cephyr-object` on native AMD64 Linux accepts the options below. With
none selected it still uses the installed On1x baseline configuration: no
QML profile or passes, FactIR explicitly off. `--emit-c` is the independent
reference. Neither the project directory's `Cephyr.toml` nor the On1x
release profile opts an application into native QML or FactIR.

```sh
build/A-enabled/on1xc --emit-cephyr-object on1x/examples/hello.on1x \
  --cephyr-profile scalar --cephyr-factir on --cephyr-sarif build/hello.sarif \
  -o build/hello.o
```

| Option | Behaviour |
| --- | --- |
| `--cephyr-profile scalar` | Select the packaged native `canonicalize`, then `strength_reduce` passes. |
| `--cephyr-qml-pass NAME` | Append one Cephyr QML phase. Repeat to specify order. |
| `--cephyr-trusted-qml-file PATH` | Load an explicitly selected trusted rule fragment. Relative paths resolve from the caller's directory. Repeat for multiple files. |
| `--cephyr-qml-fuel N`, `--cephyr-qml-param PHASE.fuel=N` | Set positive global or phase-specific rewrite fuel. The pinned driver validates phases, parameters, and exhaustion. |
| `--cephyr-factir on\|off` | Independently enable or disable the native FactIR stage after QML. |
| `--cephyr-factir-facts NEWFILE`, `--cephyr-factir-smtlib NEWFILE` | With FactIR enabled, request Cephyr's committed facts or SMT-LIB evidence, respectively. These are create-only, distinct files. A unit without an applicable rewrite emits no SMT query. |
| `--cephyr-sarif FILE` | Atomically write a SARIF 2.1.0 report after Cephyr returns success or a compile error. A failed invocation never publishes a new object. A bad On1x source, invalid target, or missing fragment can reject before any Cephyr report exists. Use the invocation status when consuming artifacts. |
| `--diagnostics sarif` | With On1x Syren enabled, emit one SARIF document on stderr for the frontend and Cephyr compile stages, as separate runs. Can be combined with the report file. |

Cephyr's primary location remains the compiler's reported location in a C
header, a rule file, or the logical `INPUT.on1x.c`. For a generated C line in
an emitted HIR statement or function declaration, a `relatedLocations` entry
can identify the enclosing On1x origin. This includes imported declarations
and PPX-generated statements whose recorded source point is consistent with
a loaded source snapshot. The related line is **not** a precise source token:
no C column is copied to the original file. Runtime guards, synthesized
helpers, unknown paths, and unavailable snapshots have no invented origin.
The map is collected separately; diagnostic collection does not insert or
remove statements in the canonical generated C.

FactIR evidence, SARIF and the object are individually published files, not
one transaction. For example, an object publication failure can leave a
valid compile-stage report or evidence file; a previous object can still
exist after a failed build.

`memgraph-bounds` remains an independent checked-HIR decision before C11
emission. Native QML handles a limited scalar MachineIR subset; user rule
fragments are trusted compiler inputs and may change semantics. FactIR's
narrow wrapping add-zero proof does not justify removing an On1x checked
operation. Neither stage authorizes ownership, loan, or FFI rewrites. No
timing or performance conclusion follows from an applied rule; the tested
Cephyr package had `CEPHYR_ENABLE_MOONSTONE=OFF`.

### Verification

```sh
ctest --test-dir build/A-enabled --output-on-failure \
  -R '^on1x\.(cephyr\.(native|options|optimization)|memgraph\.)'
```

On MuLIB `b5b4f045` and LibStone `248ad177`, the driver differential runs
four real programs in all eight MEMGraph × QML × FactIR combinations:
volatile foreign call order and unique-owner cleanup, checked overflow,
invalid division, and dynamic bounds. It compares status and both output
streams to linked host C11 references and checks loan rejection. The options
test covers a real custom QML file, invalid configuration, FactIR evidence,
generated-only diagnostics, an imported C-signature conflict inside a
PPX-expanded call, and preservation of prior artifacts. These are host
AMD64 tests against the On1x runtime, not cross-target equivalence evidence.
