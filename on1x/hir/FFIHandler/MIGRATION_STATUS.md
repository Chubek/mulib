# On1x FFI migration status

**Release state: in progress.** No migration gate, native target, or safe-call
approval is established by this initial record. The reference native path and
its independent review policy remain in place.

## Baseline (G0)

- Initial MuLIB HEAD: `8a94d9a88480b7e1b31196e81f4f3596351e97c2`.
  `git status --short` listed only the untracked, user-provided
  `on1x/MIGRATION_OF_FFI.md`.
- Coordinator directive commit: `659899782d8361639b176753e5e2fff3aa5fe652`.
  The user then provided the tracked `on1x/stdffi/clamp.ffi` at
  `d890abb6e024091b776050964d25ed0f117c4a14`; this is the common worker base.
  The integration checkout was clean at that revision.
- MuLIB's Git link and the clean selected LibStone checkout both resolve to
  `e10ac69eed7baf91b06d15c4e0d58360097cb5c4`. The checkout contains
  C11Interface v1. No pin change was needed. `git submodule status --recursive`
  printed the expected top-level revision then failed (128) on LibStone's
  unmapped historical gitlink `.decomissioned/lapislazuli/.info/binutils`.
- The seed descriptor is not generated. Its path differs from the
  `on1x/stdlib/ffi/mulib/clamp.ffi` input selected by baseline CMake. Preserve
  this distinction when comparing unmodified and restored-reference tests.
- Host: Linux x86-64. Available tools are CMake/CTest 4.4.3, GCC 16.2.1
  (20260810), Clang 22.1.8, Perl 5.42.2, Python 3.14.7, GNU Make 4.4.1,
  GNU binutils 2.47, OpenSSL 3.6.4 and Git 2.55.0. Exact configured compiler
  selections, options and test results belong with the corresponding runs.
  No AArch64 or RV64 executable runner was found on `PATH` at baseline.

### Baseline SHA-256 inputs

These are byte identities, not executable verification or trust evidence.

| Path (Cephyr paths relative to `cornerstone/Cephyr`) | SHA-256 |
|---|---|
| `MANIFEST.yaml` | `4b3f4bd042e15a1e37b691ac05a1fd2b1f38348d96dbf2b4be8fc3f56168ce01` |
| `include/mulib-clamp.h` | `cf3d0cf6e27b0d2844e25ea1e5bf7637a34508d29c42411aa855dfe97a1205e0` |
| `include/mulib-media-timebase.h` | `f9fe27e1ee9526254d7aab986c6ba71d6c1af5a50fd2ac8384ce39a82535a248` |
| `include/mulib-histprice.h` | `3f8eec2e0812e85d6649508f51f49bb9f800a9840d8c5b042bb7eb4cae0557d6` |
| `include/mulib-priceopt.h` | `29e4759e09e4c6378f1d7affde01a7e20a4915e4e007dcf36a88c29fb16a757f` |
| `on1x/stdffi/clamp.ffi` (user-provided seed) | `0035f73017d4c64e94fdba89c870091c6ee923b57c464ef6bf501a9ad96d3a99` |
| `on1x/hir/FFIHandler/FFIHandler.llsynth` (baseline) | `d1875f67d313d23076723fee9d68a2a1696a8193cdb9b1c5df7a82d204843d14` |
| `docs/C11Interface.md` | `b4d84f14cf8c3b3c54ff0e8baa1c62e3be443e8cd890e4f8d8bb3e7c0eb22378` |
| `src/interface.cpp` | `8b0406e952d2742b1502099ff10430d161f615c67bf2f39e6f721523216536ca` |
| `src/interface.hpp` | `3be47e68e00ffebcdc51b21bb06eda8767571e35c826b109ef4a4b5f83201797` |
| `tests/test_c11_interface.py` | `7f95bdc9051990a22402cbc72e11ba1b0428d73733e83ca46df7384c428d2a37` |
| `EMBEDDING.md` | `f653d6d6e4e29e15b9f17d8482a33fe033d512dd8e3061f97d42a1b19ceffac3` |
| `include/cephyr/Cephyr.h` | `1cf2c3d10ecfb08fc0d059588a93c2f73d1c50dc3bfce65443d92d7ce2374648` |
| `include/cephyr/Cephyr.hpp` | `3cc7ae5cfeb75279556cdc99d0f2fb7c04666bf707902d2af2bb2782efa828d0` |

### Coordinator baseline checks

The pinned LLSynth generated the real v0 Python parser. Running the baseline
`ffic.py` against the supplied seed in two separate output directories
produced byte-identical headers and descriptor manifests. The latter included
three `representation_only` functions, a guard over slots 1 and 2, and
`grants_safe_admission: false`. This verifies the supplied grammar input only,
not Cephyr extraction or an On1x native build. To reproduce from the common base:

```sh
handler=on1x/hir/FFIHandler
llsynth=on1x/third_party/libstone/cornerstone/LLSynth/scripts/llsynth.pl
out=/absolute/path/to/empty/seed-test-dir
mkdir -p "$out/a" "$out/b"
perl "$llsynth" --mode=interpret -T python3 -o "$out/FFIHandler.py" "$handler/FFIHandler.llsynth"
for run in a b; do
  python3 -B "$handler/tools/ffic.py" --parser "$out/FFIHandler.py" \
    --header "$out/$run/on1x_ffi_rules.hpp" --manifest "$out/$run/ffi-manifest.json" \
    on1x/stdffi/clamp.ffi
done
cmp "$out/a/on1x_ffi_rules.hpp" "$out/b/on1x_ffi_rules.hpp"
cmp "$out/a/ffi-manifest.json" "$out/b/ffi-manifest.json"
```

The parser SHA-256 was
`1956ec505def1418037c63764e17a7da6faee2f894692e81182b29e26f5e388b`.

An independent `-std=c11 -Wall -Wextra -Wpedantic -Werror -fsyntax-only`
probe of the real PriceOpt header passed under both GCC and Clang (2/2).
Both report `mulib_priceopt_demand_kind_t` and
`mulib_priceopt_objective_t` as compatible with `unsigned int` using
`_Generic`; the latter is 4 bytes with alignment 4. The selected Cephyr's
`src/semantics.cpp` (`NK_ENUMERAL_TYPE`) chooses `int` and preserves the enum
tag. Do not collapse the two observations into one identity or infer mixed
ABI compatibility from size/alignment. Exact named enum declarations,
conversion domains, and provider-specific ABI and artifact checks are
separate requirements.

Native **MuLIB-only** baseline (not FFI admission or On1x parity):

| Configuration | Cases run | Outcome |
|---|---|---|
| GCC 16.2.1, strict C11 | `mulib_test_histprice`, `mulib_test_priceopt`, `mulib_test_ipc_extensions`, `mulib_media-integration-test`, `mulib_kernelbind_inventory`, `mulib_kernelbind_generator` | 6 passed, 0 failed, 0 skipped |
| Clang 22.1.8, `MULIB_SANITIZERS=ON` (ASan/UBSan) | Same four native MuLIB fixtures, without the two Python registration-tool tests | 4 passed, 0 failed, 0 skipped |

For each compiler, configure the repository root with `-DMULIB_BUILD_MUW3=OFF
-DMULIB_BUILD_SHIRAZ=OFF -DMULIB_BUILD_VIRTIO=OFF` and the explicit
`CMAKE_C_COMPILER` / `CMAKE_CXX_COMPILER` pair. Build the four native test
targets listed above, then run the selected tests with
`ctest --test-dir "$build" --output-on-failure --no-tests=error -R
'^mulib_(test_(histprice|priceopt|ipc_extensions)|media-integration-test|kernelbind_(inventory|generator))$'`
(omit the last alternative for the sanitizer run). Logs are under
`build/coordinator/{mulib-gcc,mulib-clang-san}/` in the evidence workspace.

## Worker isolation

Twelve implementation workers, W01–W12, were launched together on branches
`ffi/w01` through `ffi/w12`, respectively. Each worker owns only its paths
from [the directive](../../MIGRATION_OF_FFI.md#6-exactly-twelve-workers).
Worktrees, private `build/wNN` directories, and shared handoff evidence are
under `/mnt/nvme/opencode-worktrees/mulib-ffi-e10/`. The coordinator integrates
on `ffi/integration-e10`. An older `ffi/migration` worktree was not changed.
