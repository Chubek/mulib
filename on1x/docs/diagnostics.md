# Diagnostics, scrutiny, and repair proposals

On1x has a LibStone-independent diagnostic catalog and bounded registry in
`include/on1x/diagnostic.hpp`. Optional components add capabilities:

- Wetstone runs a conservative static-analysis prelude when
  `ON1X_WITH_WETSTONE=ON` and its public headers are available.
- Moonstone Syren provides JSON/SARIF presentation and source-repair ranking
  when `ON1X_WITH_SYREN=ON`.

Both default on only when the corresponding vendored SDK is present. Nothing is
downloaded or patched. A build without either component still compiles source,
validates catalogs and PPX records, and prints core diagnostics as text.
Generated applications do not link LibStone or the C++ compiler.

```sh
cmake -S on1x -B build/on1x -DBUILD_TESTING=ON
cmake --build build/on1x
build/on1x/on1xc --check --diagnostics json input.on1x
build/on1x/on1xc --check --diagnostics sarif --scrutiny strict input.on1x
build/on1x/on1xc --repair input.on1x -o proposed.on1x
```

## Scrutiny policy and precedence

The embedded catalog declares `off`, `standard`, and `strict`:

| Level | Checks | Effect |
|---|---|---|
| `off` | none | Semantic checking still runs; the Wetstone prelude does not. |
| `standard` | arithmetic and assertions | Definite findings are warnings. This is the catalog default. |
| `strict` | arithmetic, assertions, and the supported ownership scan | Warnings are escalated to compilation errors. |

Configuration precedence is:

1. `--scrutiny LEVEL`;
2. the `@scrutiny LEVEL;` directive in the source file that owns a function;
3. `scrutiny LEVEL` in the root `On1xfile`, forwarded as
   `--project-scrutiny LEVEL`;
4. the selected catalog's `default_scrutiny`.

The source directive is file-local after module loading; it does not silently
change imported modules. Duplicate directives and unknown levels are errors.
`--scrutiny-work N` accepts `1..10000000` bounded work units.
`--no-analysis-repairs` disables Wetstone's advisory repair pass; it does not
disable ordinary Syren source-repair candidates.

If Wetstone is unavailable, requested analysis produces no invented result and
does not reject an otherwise valid program. The API reports
`wetstone_available=false` and `complete=false` when the effective fallback policy
is enabled. Availability is not evidence that every function was analyzed.

## Admitted static-analysis subset

The bridge analyzes only one-block, straight-line checked `i64`/`bool` HIR:
scalar parameters and locals, comparisons, safe constant-folded checked
arithmetic, division/remainder, direct scalar calls, returns, and intrinsic
assertions. It deliberately stops or skips:

- branches, loops, short-circuit operators, and joins;
- aggregates, nominal layouts, closures, resources, foreign ownership, and
  concurrency;
- floating-point and unsigned bit-pattern semantics;
- dynamic checked add/subtract/multiply that Wetstone's mathematical interval
  domain cannot model with On1x trap semantics.

Division and assertion findings are therefore useful only for this translated
prefix. `Statistics::complete` is false when a function or suffix is skipped,
the work budget is exhausted, or an analysis throws. This is not a full Oppen-
style proof system, borrow checker, overflow proof, or whole-program verifier.
Wetstone repair text is retained only as an **unverified advisory note**. The
catalog labels both its rationale and proposed change as not compiler-revalidated;
no Wetstone note becomes a source-edit candidate or validated repair.

## Declarative catalog

`stdlib/diagnostics/catalog.json` owns the new subsystem's diagnostic identities,
levels, message templates, categories, phases, guidance, remediation, provider
labels, scrutiny mappings, repair policies, PPX declarations, presentation labels
and command-failure wording. CMake runs `tools/generate-diagnostic-catalog.py`
to validate it before producing the embedded C++ payload. There is no second
handwritten bootstrap table. A host may select a complete replacement with:

```sh
on1xc --diagnostic-catalog /path/to/catalog.json --check input.on1x
```

The replacement is data, not executable rules. JSON is the accepted wire format;
YAML is not currently parsed. The strict `on1x.diagnostic-catalog/v2` parser:

- requires full JSON consumption and rejects duplicate/unknown fields;
- accepts strict UTF-8 with valid surrogate pairing and no NUL strings;
- limits input to 1 MiB, depth to 64, nodes to 65,536, entries to 512, and
  individual strings according to the public parser limits;
- validates unique diagnostic IDs, progressive scrutiny ranks/check sets,
  provider/level/repair references, Wetstone rule/level/adapter mappings, required
  bootstrap roles, unique PPX processor declarations and repair-policy fields.

The v2 top-level records are:

| Field | Data owned by the catalog |
|---|---|
| `levels` | Names, numeric transport kinds, failure behavior, SARIF levels and bounded numeric ANSI colors |
| `providers` | Names, numeric compiler/Wetstone/PPX adapter kinds and origin labels |
| `repair_policies` | Names and numeric edit/advisory mechanics; candidate IDs, descriptions, notices, punctuation end marker, type replacement lists and edit-distance limit, advisory templates |
| `presentation` | Text labels, repair status labels, validation/command-failure templates and SARIF tool name |
| `scrutiny`, `default_scrutiny` | Names, ranks, enabled state, check sets and explicit level overrides |
| `diagnostics` | Presented `id`, legacy compiler `match` when applicable, message template, metadata and references; upstream mapping or PPX `processor` declaration |

Numeric kinds are a closed implementation interface, not user-visible names.
For example, changing a scrutiny/check/provider name requires updating its
references in JSON, not recompiling C++. Message templates accept the bounded
substitutions `{message}`, `{expected}`, `{found}`, `{replacement}` and `{count}`.
Schema keys, output-format names, bounds and enum control flow remain code.

Bootstrap roles are numeric: 1 selects the source fallback, 2 catalog failure,
3 scrutiny failure, 4 PPX diagnostic protocol failure and 5 reporting-limit
failure. On malformed external
input, parsing keeps the trusted embedded catalog active and reports its role-2
entry; partially parsed external bootstrap text is never used. Build-time
validation failure stops configuration by exit status and CMake's own command
failure, not a handwritten diagnostic bootstrap. An impossible embedded-parser failure
terminates rather than inventing an implementation-literal diagnostic. Oversized
legacy message expansion uses the trusted role-5 diagnostic instead of throwing
recursively while handling the original error.

The selected catalog is shared by compiler presentation, Wetstone mappings and
PPX registration. `match` separates a legacy compiler event from its presented
`id`, so a replacement can rename codes as well as text and levels. Compiler
source errors still reject compilation even if their presentation is downgraded.
Analysis/PPX failure behavior follows the selected level's `failure` flag.

**Legacy migration limit:** existing compiler phases still construct many
literal legacy error IDs and messages; they have not all been converted to
declarative events by this change. The catalog can override mapped presentation
and routes unmapped events through role 1, whose shipped `{message}` template
preserves the legacy message. The AST `Error` default has no literal diagnostic
ID. This is a data-owned new diagnostics subsystem, not a claim that every old
compiler, runtime, SDK, project-driver or operating-system error is data-defined.

## PPX diagnostics

Selected PPXs receive a compiler-created descriptor 3 and
`ON1X_PPX_DIAGNOSTICS_FD=3`. A matching catalog `processor` declaration selects
`ON1X_PPX_DIAGNOSTIC_ID`; a sole PPX entry can also serve as the default
declaration for a differently labeled external processor. Client failure wording
is supplied as `ON1X_PPX_DIAGNOSTIC_FAILURE`. The sideband is newline-delimited
S-expressions:

```text
(register "ON1X-PPX-DIAGNOSTIC")
(emit "ON1X-PPX-DIAGNOSTIC" (Loc "file.on1x" 0 1 1))
```

A process must register an entry whose catalog provider has the PPX numeric kind
before emitting it. IDs above illustrate the shipped catalog, not native source
constants. **An emit record has no message or metadata fields.** The host resolves
all diagnostic text and metadata from the selected catalog; the old free-form
message record is rejected. The host validates canonical locations and registry
limits. The transport
allows at most 64 KiB and 256 records per PPX invocation; the native runtime
limits each write to 32 KiB. Malformed, unregistered, oversized, partial, or
trailing records fail closed with `ON1X-DIAGNOSTIC-PROTOCOL`.

PPX locations do not grant filesystem authority. Source columns are derived
only from immutable snapshots already present in the loaded module graph; an
arbitrary PPX path is reported as a path but is never opened for presentation.

`stdlib/diagnostics/ppx.on1x` exposes the correctly spelled
`basis_diagnostics_ppx_register` and `basis_diagnostics_ppx_emit` APIs.
The emit signature is `(id:string, location:sexpr)->bool`. Native diagnostics
obtains the ID with `basis_diagnostics_ppx_declaration()`; replacement catalogs
change its emitted code/message/level without rebuilding the native executable.
Outside protocol v1 writes return `false`; they do not fall back to stderr.
This sideband authenticates only catalog membership within that compiler
invocation. It does not sandbox or prove a PPX program.

## Reports and color

`--diagnostics text|json|sarif` writes diagnostics to stderr. Successful
compiler artifacts and status output remain on stdout. `--color
auto|always|never` affects text severities only; JSON and SARIF never contain
terminal escapes. Auto mode requires a terminal and honors `NO_COLOR`.

The JSON schema identifier is `on1x.diagnostics/v1`. Reports contain a bounded
ordered list and an omitted-record count. A diagnostic includes its ID,
severity, message, byte offset, line, byte column, origin, phase, check,
remediation, notes and candidate edits.

SARIF 2.1.0 percent-encodes artifact URIs and declares
`utf16CodeUnits`. Source columns remain byte-based internally. When no matching
valid UTF-8 snapshot is available, SARIF omits the column rather than inventing
one. Invalid bytes in JSON strings are escaped as `\u00XX`. Candidate data is
retained under `properties.on1xReport` and is deliberately not advertised as a
certified SARIF fix.

Text output renders every untrusted field on one physical line. C0/C1 terminal
controls, line breaks, bidi controls, and malformed UTF-8 bytes are escaped;
the only emitted terminal control sequences are compiler-owned severity colors.
Source snapshots are individually limited to 2 MiB and retained under a 16 MiB
aggregate reporting budget. Once exhausted, later SARIF columns are omitted
rather than retaining unbounded duplicate source copies.

Exit status is 1 for source/strict-analysis errors and 2 for command-line or
ordinary I/O failures. A CLI failure outside the diagnostic model may remain
plain text; selecting JSON is not a promise that arbitrary operating-system
errors become fabricated source diagnostics. An invalid scrutiny selection or
work budget is a cataloged role-3 failure (status 1). Invalid diagnostic-option
syntax uses embedded catalog command-failure wording before external selection.

## Repair boundaries

Ordinary source suggestions are bounded punctuation insertion/replacement and
catalog-selected primitive type spellings (the shipped distance is one). Token
allowlists, replacement lists and candidate identities are catalog data.
There is no coercion insertion,
binding rename, mutation rewrite, multi-error search, or arbitrary synthesis.
Normal diagnostics leave suggestions in `candidate` status.

`--repair` requires a new output path and never edits input, dependencies,
existing files, or symlinks. Each candidate is independently reloaded through
an in-memory entry-file overlay; the complete graph is reparsed, macro expanded,
type checked, analyzed, and C-emitted. Source PPXs are not executed. Only one
unique validated result is published with exclusive creation.

**Validated means compiler-valid, not equivalent to user intent.** Different
validated outputs are ambiguous; rejected or inconclusive candidates authorize
nothing. Compiler exceptions are conservatively inconclusive rather than
classified by matching words such as “limit” in legacy messages. The API also
accepts explicit refutations/invalid-input outcomes from an independent host
validator. Wetstone's proposed text is never applied as a source edit.

## Tests

- `on1x.diagnostic_catalog_source` is unconditional, including no-Syren builds:
  source-lints feature and upstream codes, embedded level/provider/policy text,
  repair replacement policy, AST default IDs and native PPX signatures, then
  exercises trusted-header generation, non-BMP Unicode C++ compilation and
  malformed catalog rejection.
- `on1x.diagnostic_catalog_runtime` is also unconditional: replacement text/levels/
  metadata/colors, analysis activation/failure when available, native PPX
  declarations when built, and trusted bootstrap on malformed/unavailable input.
- `on1x.analysis_api` covers catalog rejection, registry limits, availability,
  scrutiny escalation/precedence, catalog copy/move ownership, budgets and repair
  controls.
- `on1x.diagnostics_api` covers edits, validation outcomes, stale snapshots,
  malformed bytes, UTF-16 columns, URI escaping, aggregate snapshot bounds and
  terminal-safe text.
- `on1x.diagnostics` covers CLI formats, external catalogs, color/stderr,
  native and external PPX sidebands, malformed/bounded records, deterministic
  SARIF/JSON, runtime changes to code/message/level/check/scrutiny/repair/PPX data,
  forbidden PPX message payloads, single fatal PPX findings, whole-graph repair
  validation, and unchanged emitted C.
- `on1x.project_build` covers `On1xfile`/source/CLI scrutiny precedence.

The fallback scanner and no-Syren/no-Wetstone build are separate required
configurations; unavailable optional SDKs are not reported as exercised.
