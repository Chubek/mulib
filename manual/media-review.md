# Bounded media and compute kernel review

This is the bounded C11 integration/review surface for the exact 193-header
inventory maintained in `tools/media_scope.md`, plus the deliberately added
`mulib-audio-core.h` shared PCM helper (194 new headers total). Each header has a
`manual/mulib-<family>-<operation>.md` page covering its actual algorithm,
units, caller storage and limitations. `media-kernels.json` is a generated
machine-readable list, **not a safety-admission list**.

## Reproduce the checks

From the repository root:

```sh
python3 tools/media_inventory.py --check
python3 tools/check_media.py --report /tmp/opencode/media-review.json
python3 tools/check_media.py --compiler clang --fuzz-runs 5000
python3 tests/test-media-tools.py
```

The standalone runner requires GCC and Clang by default. Repeat `--compiler`
to select particular compiler executables. It uses strict C11 warnings and
`-O2` for ordinary builds (`-O1` for sanitizer builds),
assertions enabled, temporary files, per-process timeouts and no external
downloads. `--no-sanitize` is an explicit opt-out from ASan/UBSan, not an
equivalent validation result.

Checks include:

- Every new header independently and all 194 together.
- A declaration-only aggregate alongside canonical fraction, linalg, cgeom,
  floatgraphics, GLX pixel format, audio, bytecode, raytracing and color APIs.
- Portable timebase arithmetic with `__SIZEOF_INT128__` undefined.
- Separate C translation units linked through explicit wrappers.
- An **expected** failing link demonstrating that static-inline definitions
  do not automatically supply an external foreign-callable symbol.
- Every `tests/media-*-test.c` found when the runner starts, including worker
  review suites. Both ordinary and ASan/UBSan builds run.
- A synthetic CPU image/grade/audio/timebase example, with disjoint buffers.
- Parser truncation, CRC corruption, failed append atomicity and 2048
  deterministic malformed-input probes.
- Optional `tests/fuzz_media_parsers.c` libFuzzer runs, limited to 4096-byte
  inputs and 32 iterator steps per parser. No corpus coverage claim is made.
- Additive inventory-generator regressions.

The JSON report records compiler commands, outputs, expected link failure,
and source SHA-256 hashes. Changing a bundle header during the run fails
validation so the check must be repeated after shared-tree edits settle.

## Maintaining inventory and manuals

```sh
python3 tools/media_inventory.py --write-docs
```

The generator reads `tools/media_scope.md`, insists on 193 unique
headers, then adds only the explicitly reviewed PCM helper. It takes actual
`#include` dependencies and callable signatures from
source, and combines those with per-header `CONTRACTS` in the tool. It
refuses to overwrite an unfamiliar existing manual. Update those contracts
when workers change semantics; regeneration alone cannot review semantics.
Manual reference sections use `tools/manuals.py`'s standard paired
`manual-reference` convention.

Manifest registration is a separate intentional operation:

```sh
python3 tools/media_inventory.py --sync-manifest
```

It adds only missing bundle modules as `implementation: static-inline`,
`status: bounded`, with scope/limitations, dependency, manual, shared-example
and shared-test fields. It updates total/bounded counts without replacing
existing entries or changing `project_files`. Existing non-bounded entries,
duplicate module names or headers fail instead of being replaced. Auxiliary
support headers outside the inventory and the one explicitly named PCM helper
need their own review; they are not silently added to this bundle.

After intentional registration, the integration owner should refresh and
check existing repository metadata as appropriate:

```sh
python3 tools/sync_inventory.py
python3 tools/stratify.py
python3 tools/manuals.py --write
python3 tools/manuals.py --check
python3 tools/media_inventory.py --check
```

The root CMake build includes the shared example, separate-TU check, inventory,
generator regressions and dynamically found review tests. The Makefile includes
the C fixtures in `check`, the example in `check-examples`, and the full
standalone gate as `make check-media`.

Capacity additions deliberately change some of the new, pre-integration C
signatures. Use current per-header manuals and examples, not the historical
smoke programs. On1x's exact registration/ABI boundary is described
in [the integration note](../on1x/docs/kernel-media.md).

## Boundaries of this evidence

No test here admits safe On1x imports, proves memory safety, supplies an
FFmpeg/native audio/GPU provider, or validates a real editor/GUI/export
workflow. Restricted framing tools are not codecs; host compute buffers are
not device allocations; GPU attachment adapters are not native backends.
The tests do not establish C++ compatibility, cross-platform support,
professional audio/color quality, full fuzz coverage or performance.

Continue independent ownership/ABI review, canonical representation
bridging, allocation-failure and concurrency tests, real-media/provider
integration, durable recovery, numerical oracle comparisons and bounded
corpus fuzzing before production admission.
