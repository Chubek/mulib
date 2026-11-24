# Cephyr evidence in FFIHandler

The original declaration-only LibStone revision `e10ac69eed7baf91b06d15c4e0d58360097cb5c4`
exports `cephyr_parse_c11(request, CEPHYR_AST_SEXP, result)` / `--emit-ast=sexp`
and `cephyr_emit_interface(request, result)` / `--emit-interface=sexp`. Project
from the **same syntax document** with `-x c11-ast-sexp`, retaining the same
target and compile environment.

The v1 AST is a generic parser tree plus preprocessed source and presumed
diagnostic locations. Cephyr validates the schema and reparses preprocessed
source to check syntactic equality and rebuild parser bindings when consuming
the AST. Its line-map entries are diagnostic labels, not header provenance.
Parser body presence is not a typed/control-flow fact and successful
declaration projection does not validate or lower bodies.

The resolved v1 interface includes composite types, effective linkage and
definition, written storage, calling convention, parameter names, and
`(specifiers [inline] [noreturn])`. `noreturn` is the union of written
`_Noreturn` specifiers across declarations, not a verified behavioral or
exception classification. The pin has no `--emit-semantics` or equivalent API
for body call/CFG/effects, errno/result conditions, pointer
ownership/retention/aliasing, or taint. MIR/FactIR do not expose a stable
C-source semantic contract for these purposes.

## Generated declaration directives

The generator captures C11Syntax S-expressions, obtains C11Interface by
replaying those **same bytes**, then checks the target and compares the exact
file-scope function names, redeclaration counts and presence of definition
bodies. It never examines body statements to classify effects. A discrepancy
in the paired documents aborts publication. Unsupported translation units or
projection-budget failures remain explicit, not synthesized declarations.

A generated `ffi 2;` or `ffi 3;` module can carry evidence after its function
entries, for example:

```text
evidence c::mulib_example_stop cephyr_c11_v1 (declared_noreturn);
evidence c::mulib_example_default cephyr_c11_v1 (inline_only syntax_body);
```

| Fact | Evidence | Does not establish |
|---|---|---|
| `declared_noreturn` | Cephyr projected a written C `_Noreturn` specifier on at least one declaration. | All paths are nonreturning; termination, `longjmp`, exceptions, error status, or unwinding. |
| `inline_only` | The selected TU has a C inline-only definition per the resolved C linkage rules. Requires `syntax_body`. | An external symbol or a callable native implementation. |
| `syntax_body` | The replayed parser tree has a file-scope compound statement for the function, consistent with the interface definition state. | Body validity, same object artifact, verified semantics, or trusted origin. |

Facts are a closed, sorted, duplicate-free set, attached to one named
entry of the same module. Unknown or incompatible facts are rejected.
Where the descriptor retains definition or specifier fields, FFIHandler
cross-checks them. Noreturn and inline-only facts cannot describe a
representation-eligible scalar binding. Evidence is part of the generated
index and descriptor shard reports; FFIHandler checks that both copies match.
It does not affect scalar foreign IDs, native fingerprints, ownership,
runtime descriptors, optimization or admission. The raw AST may contain
absolute paths. The portable index records the original C source input digest
and a digest of sorted `(name, declaration count, body present)` facts, not
the raw AST or a claim of body identity.

## Analysis model 1 reports

LibStone `6a7d52e866db8eb441bb7a4f86ce91253732e740` adds analysis model 1 to
C11Interface v1. `ffgen` verifies that a Cephyr selected at this revision
reports the model, and checks the projected hosted mode against the generation
profile. Legacy projections without the model still work with legacy Cephyr.

Generated `ffi 2;` modules may report the following after their declaration
evidence:

```text
analysis cephyr_c11_analysis_v1 {
    target-properties (8 little 64 twos-complement true);
    analyze c::mulib_example_access call-contract (1 false unknown)
        requires-array-access (0 0 5) memory-safety (unknown);
    analyze c::mulib_example_mask call-contract (1 false unknown)
        memory-safety (proved closed-unsigned-expression-v2 this-definition 0 satie)
        body-proof (closed-unsigned-expression-v2 this-definition none-by-construction satie)
        return-dependencies (may-influence) return-policies (8 proved 16 proved 31 proved);
}
```

- `target-properties` lists byte width, byte order, pointer width, integer
  representation and C hosted mode. `call-contract` lists fixed argument count,
  presence of a variadic tail and the declared normal-return rule. Unknown
  argument count is not zero; `forbidden-by-declaration` is not a proof of
  termination or an exception classification.
- `requires-array-access` lists a zero-based declaration ordinal, a zero-based
  parameter position and an evaluated minimum number of elements required of
  the caller. It is not a check of any particular buffer. The report omits
  nonconstant minimums; omission does not discharge an obligation. Presumed
  source labels are neither published by this directive nor used as origin.
- `memory-safety` is either unmodeled (unknown) or a bounded result for this
  definition, with a count of modeled access sites. Only the closed model for
  unsigned expressions and reads of fully initialized fixed local arrays is
  covered, not general pointer, alias, or lifetime safety. A `body-proof`
  requires proved memory safety in that model. Its effects claim is only
  about observable memory outside that definition; it is not a claim about
  linked or interposed implementations or machine-level loads and stores.
- Return dependencies are in parameter order. The width checks bound the
  result for a specific numeric policy; even a proved eight-bit output can
  depend on untrusted input. They do not classify sources or sinks, grant
  universal sanitization, or establish the absence of taint. Unknown or
  absent facts say nothing about errno, exceptions, heap effects or taint.

FFIHandler checks the closed form, symbol identity and compatible declaration
facts. The validated `c11_analysis` report lives at module scope, not on a
runtime binding. Both package publication and registry indexing require exact
agreement with the report in `index.json`. It does not change candidate
selection, native fingerprints, IDs, runtime effects, or reviewed admission.
An unavailable module does not carry function analysis.

The legacy `projection_sha256` remains a raw-byte digest. For a successfully
decoded extended interface, `projection.digest_kind` is
`on1x.cephyr-c11-interface-analysis/v1`, and `projection_sha256` hashes the
validated typed data with the presumed per-site file and line removed.
Declaration order, parameter positions and semantic facts are retained. This
is a relocatable semantic identity, not a hash of the exact producer bytes;
those bytes remain local. The index separately records input and provider
provenance and the SHA-256 of the exact generated `.ffi` file.

## Remaining limit: general typed body semantics

Reproduce with:

```sh
printf '%s\n' 'static inline int bad(void) { return undeclared; }' |
  "$CEPHYR" --target=x86_64-linux-gnu --stdlib=none -nostdinc \
    --emit-ast=sexp -x c - > bad.sexp
"$CEPHYR" --target=x86_64-linux-gnu --stdlib=none -nostdinc \
  --emit-interface=sexp -x c11-ast-sexp - < bad.sexp
"$CEPHYR" --target=x86_64-linux-gnu --stdlib=none -nostdinc \
  -S -x c11-ast-sexp - < bad.sexp
```

The syntax and interface commands succeed and report a definition; the
third command rejects the undeclared identifier. An upstream implementation
should add a separate, versioned projection after body type checking,
addressed by the same resolved declaration identity and exact target/profile.
The first independently reviewable slice is a typed per-function CFG
projection of call edges (resolved direct callee or explicit indirect or
unknown target) and normal-return edges. Include per-function body coverage
(definition absent, fully checked, unsupported), reject semantic errors
before facts are published, and keep opaque extern calls explicitly unknown.
Its tests should distinguish a declared-only `_Noreturn` function, a direct
call, an indirect call, and a conditional return. The invalid-body
reproducer above must fail in the **new** stage while the existing two stages
stay compatible. FFIHandler would consume it only after a new LibStone pin,
schema validation and a wrong-source/wrong-target rejection test. Do not infer
errno/result, lifetime or taint contracts from these proposed CFG edges alone.

## Qualification

Generate the Python parser with pinned LLSynth as in
[stdffi regeneration](../stdffi/README.md#regeneration-and-ci). Then run:

```sh
python3 -B on1x/hir/FFIHandler/tests/qualify_cephyr_evidence.py \
  --cephyr "$cephyr_build/cephyr" --parser "$work/FFIHandler.py" \
  --scratch "$work/scratch" --tool-input "$cephyr_build/libcephyr.so.0.1.0" \
  --tool-input "$stone/cornerstone/Cephyr/triples"
```

For LibStone `6a7d52e8`, run the equivalent
`tests/qualify_cephyr_analysis_report.py` with the same arguments and new tool
inputs. It covers exact report identity, target and hosted mode, negative
validation, absence of new grants, and path-independent semantic hashing.
