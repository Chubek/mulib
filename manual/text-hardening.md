# Text stack solidification

This is a focused engineering review of the ten Unicode/font/page modules, not
a claim of complete Unicode conformance, a native-parser sandbox, or a completed
cross-platform security audit.

## Reproducible checks

```sh
python3 tools/check_text.py --require-backends
CC=clang CXX=clang++ python3 tools/check_text.py --sanitizers --require-backends
```

The first command uses the selected C/C++ compilers with strict warnings as
errors. The second adds ASan/UBSan, retaining leak detection. Both run examples,
independent module/dependency builds, separate-TU/reverse-order implementation
linking, C++ declarations and optional native ABI/result comparisons. ICU,
HarfBuzz, FreeType and Fontconfig are required by the focused tests when
`--require-backends` is passed. Native comparisons additionally need development
packages discoverable by pkg-config; unavailable comparisons report a skip.
Arabic/Devanagari comparisons explicitly report missing font coverage rather
than treating matching `.notdef` glyphs as a script-shaping success.

## Regression coverage

| Area | Evidence |
| --- | --- |
| Native layouts and aliasing | `tests/native/text_backends.c`: ABI sizes/offsets, direct glyph/position/outline comparisons; wrapper copies native structure records instead of dereferencing incompatible redeclarations |
| Font input/options | `tests/test_font_stack.c`: invalid collection face, nonalphabetic script, nonfinite variation values, bad UTF-8, empty and RTL runs |
| Native output validation | `tests/test_text_edgecases.c`: controlled native slots with negative pitch, insufficient pitch, NULL bitmap storage, NULL tags, incomplete contour endpoints |
| Unicode | `tests/test_text_edgecases.c`: thousands of scalar/UTF offset round trips; `tests/test_unicode_text.c`: malformed UTF, deterministic bidi partition/boundary invariants, normalization idempotence for all four forms, names and locale casing |
| Distance fields | `tests/test_distfield.c`: independent brute-force reference, every 3x3 binary mask, deterministic random masks, padded stride, empty/filled masks and extreme spreads |
| CSS | `tests/test_pglayout.c` and `tests/test_text_edgecases.c`: specificity/importance/inheritance, malformed numbers/colors, invalid inherited state, deterministic syntax mutations |
| Layout | Source-model depth limit and JSON/YAML round trips, invalid schema/NUL fields, grapheme-safe wrapping, hidden flow, image ratio extremes, unwritten measurement output and candidate-work budget |
| Paragraph shaping | `tests/test_text_shaping.c`: preserved RTL paragraph direction on Latin-only continuation lines, multiple paragraphs, supplementary boundaries, mixed Latin/Arabic/Devanagari fallback, source clusters, cached final runs and failed-recompute retention; native tests compare every subrange of an isolate sample with ICU line extraction |
| Failure atomicity | `tests/test_text_allocations.c`: successive allocation failures in parsing, persistence, normalization/casing/names/boundaries/bidi, enumeration, glyph copies, alpha conversion and SDF; failed recompute preserves prior boxes and borrowed line pointers |

Allocation injection covers MuLIB-owned allocations, not allocations inside
uninstrumented native runtimes. Deterministic mutations are bounded regressions,
not exhaustive fuzzing. Native libraries remain responsible for their font and
Unicode data parsers. Only trusted runtime paths should be loaded: loading a
shared library executes native code.

## Remaining boundaries

* Unicode behavior/data versions follow installed ICU. Official normalization,
  grapheme and bidi-character corpora have a separate opt-in runner (below);
  they are not bundled with MuLIB. Native parity is not full UAX conformance.
* The legacy `font_measure` adapter still resolves isolated directional runs.
  The new font-set path (`shape_line`/`compute_shaped`) supplies whole-grapheme
  fallback, script itemization, paragraph-aware line bidi, and cached glyphs.
  It uses Script-property/adjacent-neutral itemization, not every language's
  Script_Extensions/paired-script tailoring policy, and requires explicit fonts
  at a common size. Missing coverage is an error, not synthetic glyph output.
* Layout remains documented block flow, not browser CSS or complete Pango parity.
  Source/geometry and optional final glyphs are retained, not a raster atlas,
  compositor or pagination engine.
* Work budgets bound candidate count only. Callbacks and native font parsing
  need application-level resource policy when processing untrusted input.
* ABI/reference checks run on the build host. Windows/macOS, 32-bit ABIs,
  legacy bitmap fonts, color/SVG fonts and variable-font collections need
  dedicated platform/font fixture coverage before claiming those paths verified.
* ICU/Fontconfig module pinning intentionally retains native process-global
  caches; wrapper-owned contexts/results are released. Global cleanup is not
  called because it could invalidate other runtime users.

## Official Unicode corpora

Supply local data, or explicitly authorize downloading it to an external cache:

```sh
python3 tools/check_unicode_conformance.py --data-dir /tmp/opencode/mulib-unicode-17 \
    --download --version 17.0.0 --compare-native
```

The corpus version must match the installed runtime's Unicode data version,
printed by the runner. Normalization tests all 20 prescribed transformations
per record; grapheme tests compare every boundary; bidi-character tests compare
paragraph levels, resolved non-X9 levels and visual scalar order. Native
comparison, when requested, independently compares every bidi wrapper result
with directly linked ICU. It requires pkg-config/ICU development files and does
**not** waive normative failures. Corpus headers retain upstream licensing;
no downloaded data is committed.

On the verified ICU 78.3 / Unicode 17.0.0 host:

| Corpus | Cases | Normative failures |
| --- | ---: | ---: |
| NormalizationTest | 20,034 | 0 |
| GraphemeBreakTest | 766 | 0 |
| BidiCharacterTest | 91,707 | 31 |

The strict command intentionally returns nonzero for bidi. The mismatches
include explicit overrides adjacent to isolates/paired brackets (e.g. corpus
lines 61–64, 67–82); direct ICU reproduces them. They must not be described as
passing Unicode bidi conformance. Updating/replacing that backend requires
rerunning both corpus and native parity checks rather than masking failures.

## Repository integration

Public OCI/imgtracker compatibility entry points now forward to the preserved
implementations in `.deprecated/`, repairing still-live public dependencies.
Keep that sibling directory when distributing this compatibility group. The
archived implementations were not removed or overwritten. The stray numeric
token in the reserved YANTLR header was removed so it remains comment-only.
Inventory synchronization, dependency strata, combined builds and the complete
CMake suite can now run normally.

The completion pass built all examples and passed all 531 CTest entries on the
host, plus focused Clang ASan/UBSan/leak and native parity checks. GCC inventory
validation passed all 421 examples, 84 focused tests and 1,256 isolated header
checks. One aggregate GCC compile transiently aborted inside `cc1` with heap
corruption; recompiling that exact declarations-before-implementations source
in isolation succeeded. This was a compiler failure, not a skipped library test.
