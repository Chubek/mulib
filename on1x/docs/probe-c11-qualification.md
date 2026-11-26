# C11 language qualification for HTTP Probe

Probe needs immutable text in nominal records and a typed `void` effect such
as `fn(ProbeResult<M>, C) -> void`. On1x previously supported closures,
lexical borrows, and generic nominal types individually, but rejected `string`
and `fn` fields in nominal types. The qualified extension admits immutable text
and checked closures in nominal fields and type arguments, and shared
`borrow self` receivers on named places. It uses the existing closure retain
and release runtime, and lowers a borrowed receiver to a direct named loan.
See [nominal data](nominal-adts.md) and [intrinsic implementations](core-impl.md).
The composed modules also exposed redundant compound type allocation during
inference. Nominal annotation layouts and unchanged portions of specialized
signatures are now shared without raising the existing inference budgets.

Qualification configuration: MuLIB baseline `fa514d281b541a0aab2b217ef80b426fbbd17ae2`,
with the compiler changes in this tree, LibStone
`76d24a7f8b19294bab2b713dc69f799e412b62d2`, native Linux x86-64,
Clang 22.1.8, Unix Makefiles, Release and the fallback scanner. The build-host
Cephyr library is needed for FFI recipe generation even in this build with
runtime Cephyr disabled:

```sh
cmake -S on1x -B build/on1x-probe -G 'Unix Makefiles' \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DON1X_USE_FALLBACK_SCANNER=ON -DON1X_BUILD_NATIVE_PPX=OFF \
  -DON1X_BUILD_EXTENSION_SDK=OFF -DON1X_WITH_SYREN=OFF \
  -DON1X_WITH_WETSTONE=OFF -DON1X_WITH_PROJECT_QMR=OFF \
  -DON1X_STDFFI_CEPHYR_LIBRARY=/path/to/build-host/libcephyr.so
cmake --build build/on1x-probe --target on1xc on1x_core_impl_tests on1x_inference_tests
ctest --test-dir build/on1x-probe --output-on-failure \
  -R '^on1x\.(inference(_engine)?|nominal_adts|owning_adt|core_impl(_hir)?|closures|tuples|core_ref|recursive_owning_adts|data|prelude|defer_errors)$'
```

All 13 selected tests passed. The generated C11 tests exercise named void
functions and generic capturing procedures stored in records and enum
payloads, and verify that the capturing environment is released. The
receiver fixtures cover shared loan conflicts, move while borrowed,
temporary receivers, capture rejection, and forged checked HIR. The inference
engine fixtures check repeated specialization under a tightened node limit
and rejection when a cached layout is used beyond the depth budget. These
results do not establish safe foreign admission, persistent or cross-thread
closure ownership, structural aggregates of closure fields, borrowed
callbacks, `ref self` / `move self`, or support for aggregate receivers on
MIR. No additional C ABI for function values was introduced.
