# On1x profile reference

This reference describes the executable boundaries at MuLIB
`167286b84830f019e3c19fc9d9eef8b9e3ce6c59`, with LibStone pinned at
`ac95160900398cb864a91b134fc6d716b29c0a28`. These are source revisions, not a
new language or wire version. A feature accepted by the frontend is not
necessarily admitted by a backend, the project driver, or an embedding API.

## Source, checked HIR, and execution

| Layer | Admitted profile and boundary |
| --- | --- |
| [Source](language.md), [scanner and grammar](../grammar/README.md) | The language reference describes implemented syntax; the generated Aurocks and fallback scanners tokenize it. The C++ parser and checker own source grammar and semantics. Macro expansion, authorized PPX, and staging do not replace checking. |
| [Checked HIR](../hir/README.md) and [direct C11](architecture.md) | Direct HIR-to-C11 is the default and the behavioral reference for [lexical loans](core-references.md), [finite recursive owners](recursive-owning-adts.md), and [function-scoped defer and typed `Result` propagation](defer-errors.md) in their documented profiles. Deferred calls run in LIFO order on return, including a propagated `Err`; terminal traps do not promise unwind. [Cooperative concurrency](concurrency.md) is separately bounded. |
| [Portable MIR v1–v4](../mir/README.md) | v1 covers checked `i64`/bool, direct calls, CFG, and local slots; v2 adds `f64` and tuples; v3 adds unsigned widths and `f32`. v4 carries specialization directives, not new executable operations. A consumer verifies and specializes directives for its product/target before executing or emitting the executable subset. The C++ decoder and verifier, not the grammar, own admission. |
| [Evaluator](backend-mir.md) and [MIR-to-C11](../backends/mir-c11/README.md) | `--run-ir` is a bounded reference evaluator; `--emit-ir-c` emits C11 and does not compile or run it. [Admitted scalar lexical loans](ownership-backends.md) resolve to existing typed slots; they do not introduce runtime pointers, owner operations, general borrowed calls, or defer/error cleanup into MIR. |
| [VMWeave](../backends/mir-vm/README.md) | Executes verified O1VM bytecode. Its source path also projects bounded copyable records, sums, arrays/slices, and selected first-order closures into VM products. This is not general portable MIR admission, managed-value support, or native FFI. |
| [Archon](../backends/mir-archon/README.md) | Optional generated native backend: verified MIR to target-specific assembly, then assembly/link against the appropriate ABI and runtime. The [bounded native timer source path](../backends/mir-archon/TIMERS.md) is a separate checked-HIR entry point, not portable MIR or SDK timer admission. |

Read the [compiler capability matrix](capability-matrix.md) by profile and
configuration; its older test counts are historical. The [compiler SDK 2](../sdk/README.md)
has its own [machine-readable matrix](../sdk/CAPABILITY_MATRIX.json) and
preserves a distinct ABI 1 extension surface. An SDK view or registered provider
does not establish execution authority for a specific artifact.

## Foreign interfaces and application runtime

- **Build-time recipes:** [`import foreign stdffi.<kernel>`](ffi-migration.md)
  selects [FFIHandler](../hir/FFIHandler/README.md) `ffispec 1` recipes already
  validated into the compiler. Cephyr supplies build-time evidence for bounded
  signed-`i64` bodies; selected recipes lower to ordinary On1x operations rather
  than native foreign calls at execution time. The search path cannot add code
  to a built compiler. Catalogue entries and signatures are not callable recipes.
- **Native bindings:** [KernelBind](kernel-bind.md) distinguishes discovery,
  supported direct calls under lexical `unsafe`, and admitted safe calls backed
  by [independent review](kernel-review.md). Approval covers the exact supported
  binding and linkage; it does not transfer from static to shared linkage.
  Registration, generated descriptors, and test review keys are not production
  approval. [Immutable C-string admission](ffi-strings.md) is a separate bounded
  profile, not a general pointer or string ABI.
- **Projects:** [On1xfile v1](../stdlib/on1xfile/README.md) selects `c11`,
  `mir-c11`, `mir-vm`, or `mir-archon` with no silent fallback. For a
  runtime-dependent C11 build use `--cc cc` for explicit host stages: the
  [default native project tools](native-project-toolchain.md) do not compile
  the full runtime at this pin. VM output needs its matching runner; Archon
  output needs the configured target and native toolchain.
- **Embedding:** [On1xRT ABI 1](../runtime-sdk/README.md) is distinct from the
  private generated-program runtime and the compiler SDK. It exposes bounded
  thread-confined native embedding with synchronous callbacks. The ownership
  fixture keeps owners and loans private and exports only scalars. Native
  callbacks run with host-process authority; this is not sandboxing, public
  concurrency, or a guest ownership-transfer ABI.

[Scalar MIR](../examples/mir.on1x) and [scoped loans](../examples/scoped_loans.on1x)
are small execution witnesses. The [forecasting integration](../tests/integration/forecasting/check.py)
exercises a two-module native C11 application with synthetic backtesting and
review-gated kernel calls; it is neither a portable MIR example nor a production
pricing policy.

## PPX and bootstrap

[PPX protocol/Unit v1](metaprogramming.md) uses the
[macro-expanded schema](../grammar/PPX.schema.json); the compiler independently
validates output and checks the reconstructed program. Authorization to run a
processor does not sandbox it. The [standard PPXs](../stdppx/README.md) include
Python references and native On1x processors. The [self-hosting fixture](selfhosting.md)
compiles representative processors with C++ `on1xc` and a C11 toolchain, then
uses the native identity processor while re-emitting its own source. This is
tool bootstrap, not a self-hosted compiler. The fallback scanner is also a
bootstrap option, not a compiler written in On1x.

## Verification commands

Commands below are reproduction recipes, not newly run results. From the
MuLIB root on native Linux AMD64, set `LIBSTONE_ROOT` to the pinned source
checkout with its required dependencies and `CEPHYR_LIBRARY` to a build-host
shared library from that revision with `emit_mir` support. Recipe generation
requires it even without the optional in-process Cephyr backend.

```sh
: "${LIBSTONE_ROOT:?set the pinned LibStone source path}"
: "${CEPHYR_LIBRARY:?set the absolute path to build-host libcephyr.so}"
b="$PWD/build/on1x-profile-reference"
configure_profile() {
  cmake -S on1x -DBUILD_TESTING=ON \
    -DON1X_LIBSTONE_ROOT="$LIBSTONE_ROOT" -DON1X_REQUIRE_LIBSTONE=ON \
    -DON1X_STDFFI_CEPHYR_LIBRARY="$CEPHYR_LIBRARY" \
    -DON1X_BUILD_EXTENSION_SDK=ON -DON1X_BUILD_NATIVE_PPX=ON "$@"
}
configure_profile -B "$b"
cmake --build "$b" --parallel 4
ctest --test-dir "$b" --output-on-failure --no-tests=error \
  -R '^on1x\.(mir|mir_hir|mir\.directives|mir\.wire_grammar|ownership_backends|mir_borrow_hir|backends\.mir_c11|vm\.(opcodes|source)|vm_data)$'
ctest --test-dir "$b" --output-on-failure --no-tests=error \
  -R '^on1x\.(stdffi\.(cephyr_generator|recipe_compiler|mir_directives|checked_recipes)|project_(build|aux|aux_init)|runtime\.embedding_ownership|selfhost|ppx|stdppx|stdppx_protocol)$'
```

For optional [Archon](../backends/mir-archon/TOOLCHAIN.md) verification, set
`ARCHON_PREFIX` to the generated Linux AMD64 SysV package's install prefix:

```sh
a="$PWD/build/on1x-profile-archon"
configure_profile -B "$a" -DCMAKE_PREFIX_PATH="$ARCHON_PREFIX" \
  -DON1X_BUILD_MIR_ARCHON=ON -DON1X_ARCHON_ENABLE_SDK_ADAPTER=ON \
  -DON1X_ARCHON_TARGET_TRIPLE=x86_64-unknown-linux-gnu
cmake --build "$a" --parallel 4
ctest --test-dir "$a" --output-on-failure --no-tests=error \
  -R '^on1x\.(backends\.mir_archon(_native_(build|differential))?|ownership_backends)$'
```

KernelBind needs libclang and LibStone's Lua/Kaguya. The forecasting test also
needs `clang` with its resource headers, `openssl`, and `ar`:

```sh
k="$PWD/build/on1x-profile-kernelbind"
configure_profile -B "$k" -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON \
  -DON1X_WITH_EXOTRACT=ON -DON1X_WITH_EXOTRACT_LUA=ON
cmake --build "$k" --parallel 4
ctest --test-dir "$k" --output-on-failure --no-tests=error \
  -R '^on1x\.(kernel_review_sdk|integration\.forecasting)$'
```

Record the test inventory, configuration, and skips. Unregistered tests and
unavailable optional providers are not passes for those profiles.
