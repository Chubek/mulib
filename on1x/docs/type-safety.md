# Nerotypical structural type verification

On1x can independently check elaborated HIR with the real
`moonstone::nerotypical::structural::TypeGraph`. It generates constructor/equality
constraints and calls `TypeGraph::solve`, using Satie EUF and Nerotypical's
constructor decomposition, clash and occurs checks. Only `WellTyped` permits
compilation to continue. On1x's existing inference, specialization, staging and
ownership checks run first.

## Build

The pinned Satie source has two integration obstacles: its EUF headers require
the compiled `MemoryResource` implementation, and its full CMake project
unconditionally configures D frontend files absent from the checkout. The
standalone `Nerotypical::Sequent` target does not export the structural solver.

`upstream/satie-euf-target.patch` is an **On1x-proposed upstream CMake patch**, not
an upstream release. It adds an opt-in `satie::euf` target using the actual
`src/SatieMemory.cpp` and bundled MemTKX allocator headers, without configuring
the unrelated frontends. No solver or allocator implementation is replaced.
The SDK preparation tool copies the required source subset, applies this explicit
patch and records original file hashes, patch hash and patched CMake hash. It
refuses existing destinations and never fetches or changes the submodule.

From the MuLIB root:

```sh
python3 on1x/tools/prepare-type-sdk.py --output /tmp/opencode/on1x-type-sdk
cmake -S on1x -B /tmp/opencode/on1x-types \
  -DON1X_WITH_NEROTYPICAL=ON \
  -DON1X_SATIE_ROOT=/tmp/opencode/on1x-type-sdk -DBUILD_TESTING=ON
cmake --build /tmp/opencode/on1x-types --parallel 3
ctest --test-dir /tmp/opencode/on1x-types --output-on-failure
/tmp/opencode/on1x-types/on1xc --check-types on1x/examples/type-safety.on1x
python3 on1x/tools/on1x-build.py on1x/examples/type-safety.on1x --verify-types \
  --on1xc /tmp/opencode/on1x-types/on1xc -o /tmp/opencode/typed-example
/tmp/opencode/typed-example
```

The example prints `42`, `true`, `42`. Use a fresh destination if the SDK path
already exists. A custom LibStone checkout also needs the corresponding
`prepare-type-sdk.py --source PATH/third_party/satie` and `ON1X_LIBSTONE_ROOT`.

`ON1X_WITH_NEROTYPICAL` defaults OFF and is independent of Chisel, ExoML,
Novostone and scanner choice. An unpatched SDK fails configuration with explicit
preparation instructions. The minimal SDK is a source integration, not an
installed/relocatable CMake package. `on1x-build-info.txt` records the LibStone
revision, selected Satie root and structural/EUF/CMake fingerprints. The SDK's
`on1x-sdk-provenance.json` contains the full copied-source fingerprint catalog.

## CLI and compiler API

- `--check-types INPUT [-o REPORT]`: verify and emit a deterministic count report.
- `--verify-types`: require verification before source compilation, including
  source-to-MIR modes. It composes with `--memory-check` in a Chisel-enabled build.
- `on1x-build.py --verify-types`: require verification when building a program.
- `on1x::type_safety::verify(const Unit&, Limits)`: compiler-side entry point in
  `include/on1x/type_safety.hpp`, linked through `on1x_type_safety`.

Rejected source/HIR returns 1. Disabled features and incompatible inspection or
saved-MIR modes return 2. Exhaustion/unknown returns 75. A failed check does not
publish a report or overwrite an existing output file. Reports are informational;
they are not consumed as cached proof certificates. Successful verification does
not modify HIR or generated C11, and generated applications do not link Satie,
Nerotypical, MemTKX or the compiler's C++ runtime.

## Supported boundary

| HIR feature | Structural obligation |
|---|---|
| `i64`, `bool`, strings, void results | Distinct nominal ground constructors |
| Locals, mutable assignment | Lexical lookup, annotation agreement, mutability |
| Operators and conditions | Exact operand/result constructors |
| Functions and direct/indirect calls | Arity-indexed arrows and parameter/result equality |
| Immutable lexical closures | Arrow shape, return paths, transitive capture names/types |
| Tuples and destructuring | Arity-indexed products and recursive pattern shapes |
| `code<T>` after staging | Distinct code constructor; quote thunk/run result agreement |
| JSON, `sexpr`, buffers | Distinct nominal types and intrinsic signature checking |
| Monomorphized generic functions/lets | Check concrete executable instances; skip non-emitted prototypes |
| Raw externs | Check declared On1x call types only; C implementation/ABI remains unchecked |
| Explicit forall packages/projections or unlowered syntax | Reject until a quantifier-aware adapter exists |

The API expects HIR elaborated by `on1x::check`. It independently reconstructs
typing obligations from expressions, lexical bindings, signatures and statement
structure rather than merely comparing cached annotations with themselves.
Tests mutate annotations and signatures **after** ordinary checking to exercise
this distinction. Non-emitted prototypes, universality proofs and inference
correctness remain frontend obligations; this is not a replacement inference
engine or a proof of all frontend transformations.

Function and tuple constructors are injective through Nerotypical's actual
decomposition driver. Different ground/resource constructors cannot unify.
Higher-order arrows are invariant, matching On1x's structural equality rules.
Intrinsic signatures for data APIs share the canonical `data_primitive` registry;
special JSON/printing/operator constraints are constructed explicitly.

Type verification does not establish ownership, FFI contracts, allocation extent,
cleanup, effect purity or target ABI. Chisel's resource checking remains a separate
composable gate. This integration uses neither Nerotypical's demo AST nor its
call-by-need Sequent optimizer and does not change strict On1x evaluation.

## Bounds and tests

Default unit limits are 1,024 interned type terms, 4,096 user equalities, 262,144
construction work units and 64 unification rounds. HIR nesting is capped at 256;
type depth at 32 and expanded type size at 4,096. The construction-work limit
does not count EUF internal steps. Term/equality/shape/round limits bound the
solver problem; this is not a wall-clock deadline. An upstream unknown status is
never upgraded to well-typedness. API callers may lower the limits.

`on1x.type_safety_hir` checks post-sema corruptions of literals, locals, returns,
conditions, function signatures, tuples and closure metadata; malformed control
flow; independent budgets; and 64 ground/nested-constructor oracle cases.
`on1x.type_safety` checks deterministic output, executable closure/tuple/staging/
JSON/scalar examples, the build helper, macro expansion, raw extern signatures,
unsupported universals and output preservation on rejection/exhaustion.
See [verification evidence](../VERIFICATION.md) for executed configurations.
