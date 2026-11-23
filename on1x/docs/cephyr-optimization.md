# Native optimization and diagnostics

`--emit-cephyr-object` on native AMD64 Linux accepts the options below. With
none selected it still uses the installed On1x baseline configuration: no
QML profile or passes, Blackstone, FactIR and SandSLAB explicitly off. `--emit-c` is the independent
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
| `--cephyr-blackstone on\|off` | Select the matching On1x-owned Cephyr configuration. Blackstone simplifies block-local integer MIR after QML and SDK rewrites, before FactIR. |
| `--cephyr-factir on\|off` | Independently enable or disable the native FactIR stage. Earlier passes may consume candidates before FactIR sees them. |
| `--cephyr-sandslab on\|off` | Shorten eligible AMD64 constant materializations after lowering. Independent of the MIR passes. |
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
wrapping add-zero and known-Boolean double-negation proofs do not justify
removing an On1x checked operation. None of these native stages authorizes ownership, loan, or FFI rewrites. No
timing or performance conclusion follows from an applied rule; the tested
Cephyr package had `CEPHYR_ENABLE_MOONSTONE=OFF`.

### Verification

At MuLIB baseline `e8089ce7` / LibStone `4c350776`, the AMD64 Linux
build used the installed Cephyr headers, triples, native packages and
installed Archon specifications. GCC 16.2.1 and Clang 22.1.8 each linked
and ran Fibonacci, checked overflow and invalid division using reference C,
baseline objects, Blackstone objects and SandSLAB objects. Both switches
changed the Fibonacci object; neither changed its observed result.
The `std.cephyr` consumer also emitted assembly with an empty `PATH`.

```sh
ctest --test-dir build/sync-on1x --output-on-failure \
  -R '^on1x\.(cephyr\.|stdlib.cephyr|memgraph.c11)'   # 6/6 passed
ctest --test-dir build/sync-cephyr --output-on-failure \
  -R '^cephyr\.(sandslab|blackstone|c-api|cpp-api|factir|qml-driver|python-api)$'  # 7/7 passed
ctest --test-dir build/sync-disabled --output-on-failure \
  -R '^on1x.memgraph.c11$'  # 1/1 passed, Cephyr and extension SDK disabled
```

The Cephyr C API test needs the fixture's cross-linker on `PATH`. The
Cephyr package build used `CEPHYR_PREBUILT_RUNTIME_ROOT`; the On1x native
test links against the host On1x runtime rather than the bundled libc.
The earlier Fibonacci evidence assertion expected no FactIR candidate;
the current compiler emits an UNSAT obligation for the known-Boolean
double-negation rewrite instead.

Earlier QML/FactIR baseline:

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
