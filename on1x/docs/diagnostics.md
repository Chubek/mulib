# Syren diagnostics and repair proposals

Configure with `-DON1X_WITH_SYREN=ON` to build `on1x_diagnostics` and enable
the CLI adapter. The SDK comes from the existing `ON1X_LIBSTONE_ROOT`, normally
`on1x/third_party/libstone`. Nothing is fetched or patched. The default is OFF,
so bootstrap builds remain independent of Moonstone.

```sh
cmake -S on1x -B build/on1x-syren -DON1X_WITH_SYREN=ON
cmake --build build/on1x-syren
build/on1x-syren/on1xc --check --diagnostics json input.on1x
build/on1x-syren/on1xc --check --diagnostics sarif input.on1x
build/on1x-syren/on1xc --repair input.on1x -o proposed.on1x
```

## Language compatibility

This is compiler tooling, **not a language feature**. Grammar, accepted programs,
type rules, macro expansion, evaluation, runtime ABI and emitted C11 are unchanged.
Errors still reject compilation. No parser recovery silently inserts tokens.
The first text diagnostic line preserves the existing `file:line:column: error:
message` form; Syren adds IDs, guidance and suggestions on following lines.
Successful compiler output is unaffected by the selected diagnostic format.
No Syren or C++ runtime dependency is added to generated programs.

## Reports

`--diagnostics text|json|sarif` selects **source error** output on stderr.
Exit codes remain 1 for source errors and 2 for command-line/I/O failures.
CLI/I/O exceptions and optional analysis budget reports retain their existing
text interfaces; this flag does not promise that every stderr message is JSON.
Reporting remains fail-fast: one actual compiler error, not cascaded guesses.

The JSON schema identifier is `on1x.diagnostics/v1`. A diagnostic includes its
ID, severity, original message, file, byte offset, line, byte column, notes and
candidate edits. Edits are half-open byte ranges in that file's snapshot.
Known IDs include `ON1X-PARSE-EXPECTED`, `ON1X-PARSE-TYPE`, `ON1X-TYPE`,
`ON1X-SEMA`, and `ON1X-LEX-{TOKEN,NUL,LIMIT,ALLOCATION}`. Other existing errors
use `ON1X-SOURCE` unless their producer supplies a more specific ID.
The compiled-in Syren catalog matches structured compiler events, not source
text. No untrusted catalog or executable rule is loaded.

SARIF 2.1.0 uses percent-encoded artifact URIs and explicit UTF-16 columns.
Source columns stay byte-based internally. If a matching snapshot or a valid
UTF-8 prefix is unavailable, SARIF omits the column instead of inventing one.
Invalid UTF-8 bytes in JSON strings are escaped as `\u00XX`. Full edit/status
data is retained in the SARIF result's `properties.on1xReport`; candidates are
deliberately **not** advertised as certified SARIF fixes.

## Repair boundaries

Suggestions are bounded single-token punctuation insertions/replacements and
one-edit-distance spellings of existing primitive type names. There is no
semantic guessing, coercion insertion, binding renaming, mutation rewrite,
multi-error search or arbitrary source synthesis. Normal diagnostics leave
suggestions in `candidate` status.

`--repair` requires a **new** output path. It never edits the input, dependencies,
existing files or symlinks; exclusive file creation also protects against output
creation races. It repairs only an error in the entry source. Imported errors
retain their actual file/location and must be repaired separately. If the input
already compiles, it returns success without creating an output.

Each candidate is independently reloaded through an in-memory entry-file overlay
with the original loader options. The full import graph is reparsed, macro
expanded, type checked and C-emitted using the unchanged compiler. No program or
PPX is executed; source PPX declarations and CLI PPX/optional analysis options
are refused. Imported files are read normally, so this is not a hermetic build
or a certificate about a future compilation after dependencies change.

Syren classifies candidates as `validated`, `rejected`, or `inconclusive` from
the host check. Existing frontend limit failures are conservatively inconclusive.
Only a unique validated resulting source is published. Different validated
outputs are ambiguous and produce no file. Rejected/inconclusive candidates
never authorize an output; lack of a proposal returns 1.

**Validated means compiler-valid, not semantically equivalent or intended.** An
invalid source has no accepted ON1X semantics to preserve. Users must review the
separate proposal; the compiler never adopts it as the original program. Optional
ownership/ABI analyses are not certified by this mode.

The public adapter is `include/on1x/diagnostics.hpp`: `describe` takes an explicit
snapshot, `validate` runs a host Syren validator, and `text/json/sarif` serialize
the report. It rejects malformed/overlapping/duplicate edits before invoking the
validator. The adapter groups statuses before Syren ranking to avoid a mixed-
status comparator issue in the supplied SDK; the SDK itself is unmodified.

## Tests

`on1x.diagnostics_api` covers compiler metadata, edits, validation outcomes,
throwing/missing validators, stale snapshots, UTF-8/UTF-16, URI escaping and
1,000 deterministic malformed inputs. `on1x.diagnostics` covers CLI formats,
whole-graph validation, overwrite/symlink refusal, imported provenance, rejected
type errors, and unchanged acceptance/C output across formats. Both run with
the generated and bootstrap scanners, including ASan/UBSan builds.
