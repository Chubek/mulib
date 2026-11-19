# SDK 2 verification record

## Baseline

On 2026-11-19, before SDK 2 changes:

```sh
cmake --build build/on1x-sdk-gcc-make \
  --target on1x_sdk on1x_sdk_test_registry on1x_sdk_test_analyzer \
  on1x_sdk_test_backend on1x_sdk_test_security on1x_sdk_test_c_abi \
  --parallel 4
ctest --test-dir build/on1x-sdk-gcc-make --output-on-failure \
  -R '^on1x\.sdk\.'
```

Result: 8/8 tests passed, including installed consumer, isolated dynamic plugin,
linked compiler provider, registry, analyzer, backend, security, and strict C
ABI tests. This establishes only the existing ABI-v1 profile.

## SDK 2 acceptance

Each public facility needs positive, negative, limit, lifecycle and ownership
tests. Cross-process facilities additionally need malformed protocol, stale
handle, capability-denial, cancellation and worker-failure tests. Candidate IR
facilities need rejection by authoritative verification and stale-revision
tests. Build/package facilities need installed and relocated consumers.

## Integrated SDK 2.0.0

Implementation revision: `8339d319dbe495b4632e56df347ccc69d12d8a7b`.
Eight original worker revisions and their narrower historical reports are
listed in [IMPLEMENTATION_INVENTORY.json](IMPLEMENTATION_INVENTORY.json).
See [the machine-readable integration report](catalogue/integration-report.json)
for the aggregate results.

Environment: Linux x86-64 `6.18.52-1-lts`, GCC 16.2.1, Clang 22.1.8,
CMake 4.4.3, Python 3.14.7, LibStone
`3399047f6c6328b8f07788795c610699e66c060a`. Tests run from the repository root
with external build directories. The commands below use Bash.

| Profile | Result |
|---|---|
| GCC Debug, generated scanner, native PPX, complete On1x regression suite | 167/167 passed |
| Clang Release, fallback scanner, SDK/backend and affected regressions | 75/75 passed |
| Clang Debug, fallback scanner, ASan/UBSan with leak detection | 40/40 passed |
| GCC, pinned Novostone source provider, analysis and relocated consumer | 2/2 passed |
| Standalone CDD oracle, allocation failure and C++ linkage | All four GCC/Clang normal and ASan/UBSan profiles passed |
| Root MuLIB regression, GCC | 722 passed, 2 failed: stale dependency strata and MuLL header registration |

```bash
builds=/mnt/nvme/opencode-builds
export TMPDIR="$builds/tmp" PYTHONDONTWRITEBYTECODE=1
mkdir -p "$TMPDIR"
common=(-DBUILD_TESTING=ON -DON1X_WITH_SYREN=OFF -DON1X_WITH_WETSTONE=OFF
        -DON1X_WITH_PROJECT_QMR=OFF)
linked=(-DON1X_LINKED_SDK_EXTENSION="$PWD/on1x/sdk/tests/linked_extension.c")

g="$builds/sdk2-final-gcc"
cmake -S on1x -B "$g" "${common[@]}" "${linked[@]}" \
  -DCMAKE_BUILD_TYPE=Debug -DON1X_REQUIRE_LIBSTONE=ON -DON1X_BUILD_NATIVE_PPX=ON
cmake --build "$g" --parallel 6
ctest --test-dir "$g" --output-on-failure --parallel 6

c="$builds/sdk2-final-clang"
cmake -S on1x -B "$c" "${common[@]}" "${linked[@]}" \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release -DON1X_USE_FALLBACK_SCANNER=ON -DON1X_BUILD_NATIVE_PPX=OFF
cmake --build "$c" --parallel 5
ctest --test-dir "$c" --output-on-failure --parallel 4 \
  -R '^on1x\.(sdk|backends\.|install$|contracts$|mulib$)'

s="$builds/sdk2-final-asan"
flags='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake -S on1x -B "$s" "${common[@]}" \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="$flags" -DCMAKE_CXX_FLAGS="$flags" \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined \
  -DON1X_USE_FALLBACK_SCANNER=ON -DON1X_BUILD_NATIVE_PPX=OFF \
  -DON1X_BUILD_DYNAMIC_SDK_PLUGINS=OFF
cmake --build "$s" --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:handle_segv=0 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir "$s" --output-on-failure --parallel 4 \
  -R '^on1x\.(sdk6\.|sdk\.(aggregate|core_v2|registry|catalogue_v2|plugin_v2|analyzer|backend$|security|c_abi|abi2_c|semantics_ir|frontend($|_c|_example|_scanner)|analysis_|target_v2|backend_v2|toolchain_v2|external_backend|builtin_providers_v2|native\.|project$|project_c$))'

n="$builds/sdk2-final-novo"
cmake -S on1x -B "$n" "${common[@]}" -DCMAKE_BUILD_TYPE=Debug \
  -DON1X_USE_FALLBACK_SCANNER=ON -DON1X_BUILD_NATIVE_PPX=OFF \
  -DON1X_WITH_NOVOSTONE=ON -DNOVOSTONE_PCC_USE_SATIE=OFF
cmake --build "$n" --parallel 4 --target \
  on1x_sdk_test_analysis_passes on1x_sdk_test_aggregate_c on1x_sdk_project on1x_rt_sdk
ctest --test-dir "$n" --output-on-failure \
  -R '^on1x\.sdk\.(installed_consumer|analysis_passes)$'

for cc in gcc clang; do
  case "$cc" in gcc) cxx=g++ ;; clang) cxx=clang++ ;; esac
  CC="$cc" CXX="$cxx" sh tests/run-cdd.sh
  CC="$cc" CXX="$cxx" CFLAGS="$flags" CXXFLAGS="$flags" \
    LDFLAGS=-fsanitize=address,undefined ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1 sh tests/run-cdd.sh
done

r="$builds/sdk2-mulib-regression"
cmake -S . -B "$r"
cmake --build "$r" --parallel 6
ctest --test-dir "$r" --output-on-failure --parallel 6
```

Checks include the frozen ABI-v1 C consumer and linked/isolated fixture,
ABI-2 prefix/status negotiation, all 126 SDK C headers and 24 runtime headers
as independent C11 and C++17 units, the C++17 wrapper, cross-facet umbrella
and linkage, released-package relocation, exported project symbols, Python
binding, and native On1x-to-C11 execution. Relocation tests build C and C++
consumers of the moved SDK, including a v1 consumer, and separately link and
run the On1xRT lifecycle fixture. The Novostone check exercises the pinned
source provider; the imported-package provider is not part of this matrix.

For the symbol inventory, compare the `on1x_sdk_*` declarations in
`sdk/include/*.h` against `nm -g --defined-only "$c/sdk/libon1x_sdk.a"`
and `nm -D --defined-only "$c/sdk/libOn1xSDKProject.so"`. All 502 SDK-provided
names were found; `on1x_sdk_linked_extension` is a provider entry point, not
an SDK export. All 51 `on1x_rt_*` declarations in `runtime-sdk/include/*.h`
were found with `nm -g --defined-only "$c/sdk/libon1x_rt_sdk.a"`. This
checks symbol presence, not behavior beyond the suites above.

The root failures are `mulib_dependency_strata` (`tools/stratify.py --check`)
and `mulib_mull_inventory` (`mudsl/scripts/check_mulib_inventory.py --include
include`). Their root scripts, build rules, manifest and headers are unchanged
from `bdbd79aedc806468195cb67df402591ae7209551`. The graphics and CDD gates
passed. No generated root inventory or unrelated graphics code was changed.

The ordinary GCC and Clang runs exercise fail-closed isolated-worker behavior,
not an independent sandbox audit. The Clang sanitizer profile excludes the
dynamic worker and unsanitized Python loading of the instrumented project
library; `handle_segv=0` preserves an intentional child fault in the shared
mapping test. C11 fixture runners that do not propagate compiler sanitizer
flags are supplementary plain-code checks, not separately sanitized tests.
CDD runs separately include 500 seeded cases, 24,576 exhaustive oracle cases,
allocation failure and C++17 linkage per profile. No TSAN, non-Linux or 32-bit
verification, or general safe FFI, JIT, WASM or native plugin certification is
claimed.
