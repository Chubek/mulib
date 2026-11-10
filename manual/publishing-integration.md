# Publishing additions: integration and scope

The publishing additions are incremental C11 single-header APIs, not a complete
TeX/LaTeX engine. The root `MANIFEST.yaml` includes 166 publishing modules
(165 requested names plus `typecore`) alongside the 802 previously registered
modules. `implemented` means that a module has an implementation of its documented
subset; it does **not** mean full format, algorithm, or standards conformance.
The `purpose` field and each module's manual describe that subset.

Existing upstream headers are retained. New publishing implementations require
`MULIB_IMPL_TYPECORE` exactly once in the program, plus each module's own
`MULIB_IMPL_<MODULE>` switch. Dependency implementations are not enabled implicitly.
For a complete example, see [build-publication.c](../examples/build-publication.c).
The individual examples remain in `examples/mulib-*.c`; their manifest `example`
fields record this location without moving or replacing upstream examples.

## Repeatable checks

From the repository root:

```sh
make -j4 check check-examples formats
python3 tests/test_tools.py
python3 tools/stratify.py --check
python3 tools/manuals.py --check
python3 mudsl/scripts/check_mulib_inventory.py --include include
python3 tools/check.py --no-isolation -j4
make docir-formats
```

`make check` runs the hyphen-named standalone API tests, including the cross-target
fixture; it is not the entire upstream test suite. `tools/check.py` additionally
checks upstream tests, examples, combined implementations, and C/C++ declarations.
Omit `--no-isolation` for per-header builds; use `--sanitizers` with an appropriate
compiler for AddressSanitizer/UndefinedBehaviorSanitizer checks. CMake discovers
both upstream and publishing module examples with `MULIB_BUILD_MODULE_EXAMPLES=ON`.

The generated MuLL bridge also registers the new headers and native functions.
After changing public headers, regenerate it with
`python3 mudsl/scripts/generate_mulib.py --include include` (regeneration requires
the Python clang bindings and libclang), then run the inventory check above.
Normal builds do not require those generator dependencies.

`make formats` generates PDF, DVI, EPUB, PNG, PalmDOC, HTML, and LaTeX fixtures and
checks them using independent Python standard-library readers. These structural
checks are not standards certification; use independent format validators and
normative suites before advertising a conformance profile.

## Deliberately incomplete capabilities

The [owned DocIR pipeline](docir-publishing.md) now provides cycle-safe ownership,
bounded snapshots, semantic validation, source/style metadata, normalized cloning,
versioned serialization and adapters into the existing LaTeX/HTML/EPUB modules.
This finishes that documented semantic subset, not the full compiler/layout roadmap.

- [Native fontshape](mulib-fontshape.md#native-latin-implementation) now executes
  a bounded, mark-free ASCII/Latin GSUB/GPOS profile. It is not a full OpenType
  or complex-script shaper. The partial native Unicode helpers do not replace
  ICU's normative algorithms or data.
- `pdfcanvas` emits one-page PDF 1.4 with ASCII Helvetica, not PDF/A or PDF/UA.
  `dvi` emits DVI ID=2 rules/empty pages, not font-based typesetting.
- `epubwriter` is a one-chapter EPUB 3.0 demonstrator with a fixed identifier and
  modification date; it is not a replacement for the upstream EPUB backend.
- `mobiwrite` emits PalmDOC, and `kf8write` emits FDST flow tables. Neither is a
  complete MOBI/KF8/AZW3 ebook writer.
- Generic registries, record collections, and rectangle placement helpers do not
  imply complete implementations of the algorithms suggested by their names.

The native typography replacements, full TeX semantics, document-aware layout,
broader allocator fault-injection coverage, fuzzing, and normative differential suites
remain separate work. Packaging these baselines does not complete those upgrades.
