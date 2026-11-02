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
| Failure atomicity | `tests/test_text_allocations.c`: successive allocation failures in parsing, persistence, normalization/casing/names/boundaries/bidi, enumeration, glyph copies, alpha conversion and SDF; failed recompute preserves prior boxes and borrowed line pointers |

Allocation injection covers MuLIB-owned allocations, not allocations inside
uninstrumented native runtimes. Deterministic mutations are bounded regressions,
not exhaustive fuzzing. Native libraries remain responsible for their font and
Unicode data parsers. Only trusted runtime paths should be loaded: loading a
shared library executes native code.

## Remaining boundaries

* Unicode behavior/data versions follow installed ICU; official full UAX
  conformance corpora are not bundled or exhaustively run here.
* The fixed-font layout adapter still needs application script/fallback
  itemization and a paragraph-preserving line-bidi pipeline for full typography.
* Layout is block flow, not Pango parity or browser CSS. It stores source and
  geometry, not a persistent glyph cache, renderer or pagination engine.
* Work budgets bound candidate count only. Callbacks and native font parsing
  need application-level resource policy when processing untrusted input.
* ABI/reference checks run on the build host. Windows/macOS, 32-bit ABIs,
  legacy bitmap fonts, color/SVG fonts and variable-font collections need
  dedicated platform/font fixture coverage before claiming those paths verified.
* ICU/Fontconfig module pinning intentionally retains native process-global
  caches; wrapper-owned contexts/results are released. Global cleanup is not
  called because it could invalidate other runtime users.

Repository-wide inventory/strata and combined builds remain independently blocked
by existing public references to OCI/imgtracker headers in `.deprecated/`.
The focused checker does not hide or modify that unrelated inventory problem.
