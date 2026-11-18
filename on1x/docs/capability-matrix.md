# On1x capability matrix

Current closeout baseline: MuLIB
`bf23bd1282d280f1935b9ca1898fcb2ab19a7ca4` plus documentation changes,
configured LibStone
`934b6ed029bfea28bbf23b5dc77c476154b0dd0d`.

Final combined validation (2026-11-19) is **98/98 passed** with the generated
scanner and **97/97 passed** with the fallback scanner. The standalone ChatGPT
addon passes **27/27**; focused Clang ASan/UBSan cross-feature validation passes
**15/15**. The earlier finite-owner profile also passed real
Chisel/Nerotypical fail-closed checks and strict Clang ASan/UBSan execution.
Later per-work-item totals are dated historical records and do not override
these current results.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Checked type/HIR admission hardening | `src/{type,infer,nominal,cliche,inherent,loader}.cpp`, `src/canonical/verify_values.cpp` | Existing C11 ABI unchanged; scalar MIR remains fail-closed for owners | Bounded type forms, owning-target traversal, generated signature/binder/constraint/identity matching and transfer-metadata validation | `on1x.type_checker_hardening{,_hir}`, `on1x.cliche_hir`, `on1x.core_impl_hir`, ownership suites; exact matrix in `../VERIFICATION.md` | EXECUTABLE | General effect/ownership MIR and full-profile sanitizer limits remain |
| Extended literals and interpolation | `grammar/On1x.g`, `include/on1x/{literal_scan.h,literals.hpp}`, canonical parser | Checked C11 scalar/buffer/text lowering | `char`, `b!` bytes/buffers, raw/triple strings, `$` interpolation, `format`, binary/octal; existing scientific notation | `on1x.literals_core`, scanner parity, formatter | EXECUTABLE | Existing text ABI cannot represent embedded NUL |
| First-class regular expressions | `src/regex.cpp`, scanner/parser, pinned `firestone/liblr` | Compiler-generated tables and a C11 matcher; no LibStone at runtime | UTF-8 whole matching, bounded exact captures | `on1x.core_regex`, differential oracle, scanner parity | EXECUTABLE | Variable-width capture groups and broader Perl syntax |
| Enum connective | Canonical parser, structural matcher, `SourceSumMatch` | Existing C11 match lowering | Known closed sum; `~ { … }`, `-` wildcard, guards, exhaustiveness | `on1x.enum_connective` | EXECUTABLE | Scalar MIR does not model sums |
| Borrowed reference parameters | Canonical parser, checker, HIR verifier and C11 emitter | Private `int64_t *` parameters; scoped loan validation | `ref i64` and `:=` on named places, no reference values | `on1x.core_ref`, `on1x.core_ref_hir` | EXECUTABLE | Other referents, escaping loans, concurrency and foreign/MIR ABI |
| Shared ownership in nominal ADTs | `src/{nominal,sema,codegen}.cpp` | Existing bounded `sexpr` RC, propagated through records/enums | Nonrecursive nominal fields and payloads can retain immutable `sexpr` | `on1x.owning_adt` | EXECUTABLE | General borrows/foreign resources; finite unique recursion is the separate profile below |
| Intrinsic core cliches | `src/{parser,loader,cliche,infer,sema,codegen}.cpp`, `include/on1x/{ast,cliche,clone}.hpp` | Deterministic ordinary C11 functions/nullary associated-value calls; no registry or runtime name lookup | Generic cliches/functions/implementations/members, associated values, bounds, qualified calls, imports, exact-over-blanket coherence and finite resolution | `on1x.cliche`, `on1x.cliche_hir`; source/HIR/import/coherence/limit/determinism/runtime cases | EXECUTABLE | Associated types, negative/specialization rules beyond exact override, dynamic dictionaries, separate interfaces, effect/ownership constraints and non-scalar MIR |
| Intrinsic nominal `impl` blocks | `src/{parser,loader,inherent,infer,sema,codegen}.cpp`, `include/on1x/{ast,inherent,clone}.hpp` | Deterministic private ordinary C11 functions and value calls; scalar-only elaborations continue through MIR; no registry/lookup ABI | Generic nominal targets and members, associated/receiver functions and values, hygienic member macros, module aliases/re-exports, enum receivers, cliche coexistence/ambiguity, orphan/duplicate checks and copy-safe value receivers | `on1x.core_impl`, `on1x.core_impl_hir`, `examples/core_impl.on1x`; generated/fallback GCC, Clang, ASan/UBSan, strict C11, scalar MIR differential, order/module/ownership/HIR/boundary/limit cases | EXECUTABLE | Borrow/move receivers, managed/resource receivers, explicit member type arguments, visibility modifiers, Unit-v1 PPX/interfaces, aggregate MIR and separate compilation |
| Recursive nominal ADTs with finite unique owners | `src/{type,nominal,infer,sema,codegen}.cpp`, `src/canonical/{values,verify_values}.cpp`, `runtime/on1x_runtime.*` | Opaque versioned `on1x_unique *`, bounded emergency cleanup registry and FIFO iterative destruction | `own<Named>`, `box`, explicit `move`, consuming `take`/`drop`/match; only owning indirection cuts cycles | `on1x.recursive_owning_adts{,_hir}`, scanner parity; GCC/Clang strict C11, ASan/UBSan, C/C++ ABI, module, fault, 50,001-node stress and real Chisel/Nerotypical rejection cases | EXECUTABLE | Typed ownership MIR, PPX/interface transport, borrow/receiver/FFI/capture/defer/staging/concurrency support and other targets |

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| On1x editor packages | `addons/{neovim,vscode,sublime,bat}` | Editor-local; no application ABI | Lexical highlighting for `.on1x` and `.on1xi`; no compiler conformance claim | See [editor-addon verification](../VERIFICATION.md#on1x-editor-addons) | EXECUTABLE | Native GUI editor coverage |
| On1x stdio language server | `addons/lsp` | Python stdio bridge to the existing `on1xc` | Compiler parse/import diagnostics on save; formatter for open `.on1x` buffers | See [editor-addon verification](../VERIFICATION.md#on1x-editor-addons) | PARTIAL | Type diagnostics without executing PPX, compiler symbol index, completion, navigation, and rename |
| On1x extension SDK | `sdk/`, `src/cli.cpp` | Versioned build-tool C ABI; isolated Linux x86-64 worker for external plugins; no generated application dependency | No new syntax; named verified-MIR analyzer and emitter hooks | See [extension SDK verification](../VERIFICATION.md#on1x-extension-sdk) | PARTIAL | Checked HIR/source hooks in the compiler driver, independent sandbox review, other worker platforms, stable compiler-IR API and safe FFI admission are outside this slice |
| Built-in backends via SDK | `backends/`, `src/cli.cpp` | Separate static adapter archives, no change to generated application ABI | Existing checked HIR and verified MIR; no new syntax or lowering | `on1x.mir`, `on1x.compiler`, `on1x.sdk.*`; [backend layout](../backends/README.md) | EXECUTABLE | Full alternate target build matrix and public HIR ABI are not provided |
| ChatGPT repository-knowledge MCP | `addons/chatgpt/{core,server,test_chatgpt}.py`, packaging and manifests | Tool-only, stateless Streamable HTTP JSON backend; no compiler or application ABI | Seven bounded read-only tools over an allowlisted startup snapshot, with path/line/hash citations and separate discovery/contract/implementation/admission states | `on1x.addon_chatgpt`; standalone **27/27** plus packaging/relocation and final generated/fallback suites | PARTIAL | Operator-hosted TLS/auth/publication policy and actual ChatGPT web interoperability remain unverified |

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Shared MuLIB registration and CDD discovery | `contracts-exoml/catalog.json`, `contracts-exotract/catalog.json`, `tools/{on1x-contracts,on1x-exotract}.py`, `stdlib/mulib/housekeeping.py` | MANIFEST-backed library inventory and target-specific CDD CDecl; no CDD archive or callable ABI | Existing KernelBind inspection syntax only | `on1x.contracts`, `on1x.cdd_discovery_sdk`; [recorded results](../VERIFICATION.md#mulib-contract-inventory-and-cdd-discovery) | PARTIAL | Checked CDD accessors for fixed arrays, explicit counts and borrowed lifetimes; semantic contracts and independent admission gates |

Sandbox KernelBind integration (M2 discovery slice): MuLIB
`681dacbdd419b71ae86042d0c307180cbd5dea5e` plus worktree changes, configured
LibStone `57113dc48d815359431dce63a6b865a21dd993cd`, Linux x86-64.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Sandbox kernel build and discovery | `tools/on1x-exotract.py`, `contracts-exotract/`, `contracts-exoml/sandbox/` | One native C11 archive and target-specific CDecl evidence for eight kernels, not a checked On1x runtime ABI | Discovery only; no safe or raw callable imports | `on1x.kernel_sandbox_sdk`; strict separate-TU client and negative SDK/admission cases | PARTIAL | Independent semantic review; affine process/descriptor/cgroup ownership, error cleanup, capability and callback checking; host enforcement separately gated |

Terminal standard-library slice baseline: MuLIB
`681dacbdd419b71ae86042d0c307180cbd5dea5e`, Linux x86-64, GCC 16.2.1 and
Clang 22.1.8. This remains a trusted raw scalar bridge, not safe FFI admission.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Bounded terminal I/O, events, modes and vterm | `stdlib/tty/{tty.on1x,runtime.[ch]}`, `tools/on1x-build.py` | Canonical one-TU `termutils`/`termkey`/`termstyle`/`terminfo`/`vterm` closure; kind/generation handles, normal-exit raw restoration, bounded immutable snapshots and lossless hex bytes | Existing scalar/tuple facade; explicit statuses, TTY/`TERM`/`NO_COLOR` policy, style/control, caller-clocked events and one-scalar-per-cell vterm | `test_tty.py`, `test_tty_runtime.c`, `test_tty_allocation.c`, compiled `examples/tty.on1x`; strict GCC/Clang and focused ASan/UBSan | EXECUTABLE | No reviewed safe FFI/resource-drop type, byte-string type, concurrent API, signal-crash restoration, Windows backend, wcwidth/graphemes or full DEC/Kitty protocol |

Static-analysis diagnostics continuation starting baseline: MuLIB
`681dacbdd419b71ae86042d0c307180cbd5dea5e`, configured LibStone
`57113dc48d815359431dce63a6b865a21dd993cd`, Linux x86-64. This row records
 the pre-change boundary; the row below records the implemented continuation
 without claiming whole-language proof.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Bounded diagnostics, scrutiny, PPX advisories and repair analysis | `src/{diagnostic,diagnostics,analysis,cli,ppx,sema}.cpp`, `include/on1x/{diagnostic,diagnostics,analysis}.hpp`, `stdlib/diagnostics`, `tools/generate-diagnostic-catalog.py`, build drivers | Compiler-side core registry; optional Syren/Wetstone; message-free native PPX descriptor-3 runtime ABI; generated non-PPX applications have no LibStone dependency | File-local `@scrutiny`, CLI > source > On1xfile > catalog precedence; conservative checked-i64/bool prefixes; build-validated JSON v2 owns new identities/presentation/policy/repair/PPX declarations, with trusted bootstrap and runtime replacement; text/JSON/SARIF and independently rechecked Syren source edits; Wetstone proposals remain advisory | `test_analysis.cpp`, `test_diagnostics.{cpp,py}`, unconditional `test_diagnostic_catalog_{source,runtime}.py`, adversarial PPX and project policy tests; generated/fallback, strict GCC/Clang and focused Clang ASan/UBSan | EXECUTABLE | Legacy compiler messages/IDs outside this subsystem remain literal; no CFG/loop/interprocedural/resource proof, complete dynamic checked-overflow model, incremental/LSP stream, cross-platform PPX sideband, or whole-suite sanitizer evidence |

Pre-v2 historical continuation results: generated-scanner GCC with Syren/Wetstone **67/69**
and fallback-scanner GCC without them **64/66**; only `on1x.contracts` and
`on1x.mulib` fail on the unrelated audited `manual/mulib-elvi-scrape.md`
fingerprint drift. Focused Clang ASan/UBSan passed **7/7**; strict
warnings-as-errors GCC and Clang selections passed **2/2** and **4/4**.

Catalog-v2 follow-up on the same MuLIB/LibStone revisions, Linux
6.18.52-1-lts x86-64, GCC 16.2.1 and Clang 22.1.8: current complete reruns are
**70/71 generated** and **67/68 fallback**. The sole failure in each is
`on1x.mulib` rejecting unrelated stale OptML profiles from `tools/embed_units.py`;
`on1x.contracts` now passes in this concurrently changing worktree. No unrelated
profiles/fingerprints were regenerated. Strict GCC/Clang focused selections
pass **4/4** and **6/6**; Clang ASan/UBSan with compiler, runtime, generated C11
clients and native PPXs instrumented passes **9/9**. Exact commands and limits
are recorded under “Catalog-v2 data ownership” in `../VERIFICATION.md`.

LuaEmbed work item starts at MuLIB `681dacbdd419b71ae86042d0c307180cbd5dea5e`.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Native GrammarEmbed | `stdppx/native/GrammarEmbed.on1x`, `stdppx/grammarembed/` | Build-host liblr C++ companion; emitted pure On1x runtime engines and tables | Embedded grammar/token/rule DSL, named refs/EBNF; NFA/DFA × LR1/LALR1/LL1; token spans/trees | `test_grammarembed.py`, independent liblr runtime oracle, bridge API and relocated install | EXECUTABLE | Documented byte grammar subset; no Raku actions/inheritance, GLR/PEG, incremental liblr construction budget or general foreign ABI |
| Native LuaEmbed and `llembed` | `stdppx/native/LuaEmbed.on1x`, `stdppx/luaembed/` | Trusted synchronous build-host scalar bridge to canonical Mune, one planner implementation TU; no application Lua dependency | Existing explicit carriers; captured runtime-position stdout/text, schema-checked expressions/statement streams/programs, finite chained expansion | `test_luaembed.py`, runtime/allocation C tests, relocated installation, both scanners, strict GCC/Clang C11 and ASan/UBSan; see `VERIFICATION.md` | EXECUTABLE | No OS sandbox, raw-Lua core lexer, automatic external PPX authorization, general safe FFI, binary-NUL application strings or cross-platform certification |

M5/M7 tooling work item, starting checkout: MuLIB
`f520a7da8cd29c282f335ed3d0fdca1599c2ff30`, configured LibStone
`57113dc48d815359431dce63a6b865a21dd993cd` (different from the parent
gitlink; left unchanged), Linux x86-64, GCC 16.2.1 and Clang 22.1.8.
The table records the current state of this work item.

Git-locking continuation starting checkout
`681dacbdd419b71ae86042d0c307180cbd5dea5e`, configured LibStone
`57113dc48d815359431dce63a6b865a21dd993cd`, Linux x86-64, Git 2.55.0
and Python 3.14.7.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| On1x source formatter | `src/cli.cpp`, `stdlib/prettyprint/on1x/formatter.cpp`, canonical lexer/parser, PrettyDoc/PrettyEmit | Compiler-only planned C11 kernel linkage; existing raw adapter also exports a verbatim document constructor | `.on1xfmt` with bounded layout search and Allman braces, paired preservation and target directives, CLI and On1xfile plain/LaTeX/troff, CLI `--inplace`/`-i` for plain source; conserves tokens and dot adjacency | `tests/test_formatter.py`, `prettyprint.mk`, in-place publication/rejection, original/formatted strict C11 execution and markup compiler tests | EXECUTABLE | Conservative wrap points; no syntax recovery, unchanged source locations, or cross-platform publication verification |
| On1xfile project builds and locked Git inputs | `tools/on1x-project.py`, `stdlib/on1xfile` | Existing C11 builder and canonical MuLIB native plans; object-only local/HTTPS Git acquisition; optional QMR compiler on Mune; Linux bubblewrap | Line-oriented declarations; full commit, branch and lightweight/annotated tag selectors resolved by canonical `On1xfile.lock`; offline/locked/frozen modes | `test_on1x_project.py` (14 cases) and `test_project_fetch.py` (11 cases); local bare repositories, smart-HTTPS loopback, ref movement, atomic concurrent cache reuse, frozen C11 builds and negative transport/ref/cache cases | EXECUTABLE | No registry/version solver/transitive fetching, authenticated/SSH remotes, portable atomic publication or pinned complete toolchain |
| On1xfile project lifecycle | `src/{cli,discovery}.cpp`, `tools/on1x_project.py`, `stdlib` | No new application ABI | New stdlib manifests with legacy discovery compatibility; executable, complex and library scaffolds | `on1x.project_driver`, `on1x.install`, `on1x.discovery` | EXECUTABLE | More platforms; no expansion of SML Basis conformance |

Package D continuation: MuLIB `2da654cae071226ad246415d2fad4ba43f6f9ec1`,
LibStone `9c44917b6518c03ef33c544c0ef93b2d864fe4fa`. See
[the numerical profile](kernel-numerics.md) and [verification](../VERIFICATION.md).

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Bounded numerical KernelBind | `include/on1x/kernel.hpp`, `src/{loader,infer,sema,codegen}.cpp`, `tools/{on1x-build,on1x-exotract}.py` | Histprice EMA/Holt and PriceOpt optimize/candidates; exact Linux x86-64 C11 signatures; one implementation archive | Immutable snapshot slices; named models, constraints, enums and `Result` with `?`; no exposed pointers | SDK differential/probes, independent C pricing fixture, drift/type/failure rejections and ASan/UBSan | EXECUTABLE | No production review, other pricing kernels, general spans/owned resources, JSON I/O or Shylock parity |

Package C safe-scalar continuation baseline: MuLIB
`2da654cae071226ad246415d2fad4ba43f6f9ec1`, configured LibStone
`9c44917b6518c03ef33c544c0ef93b2d864fe4fa`, Linux x86-64/glibc 2.44,
GCC 16.2.1 and Clang 22.1.8. The safe-scalar trust boundary is documented
in [KernelBind](kernel-bind.md).

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Signed-review scalar KernelBind | `src/{loader,sema,codegen,cli}.cpp`, `tools/{on1x-build,on1x-exotract}.py`, `contracts-exoml/clamp.exocontract.json` | Exact clamp i64 ABI, one-TU archive, generated adapter, atomic reviewed bundle | Native safe namespace calls after independent operator-selected Ed25519 authorization; unapproved builds reject | `tests/test_kernel_review_sdk.py`, `examples/kernel/clamp_safe.on1x`; ephemeral test authority only | EXECUTABLE | Independently issued production review; resources/numerical shapes; independent checker/MIR profiles |
| Selected-contract build planning | `stdlib/mulib/housekeeping.py`, `tools/on1x-contracts.py` | Validate selected core contract and its closure; unrelated stale source pins no longer block selection | Build helper only | Real-SDK KernelBind discovery regression | EXECUTABLE | Whole-catalog verification still diagnoses unrelated stale profiles |

Measured continuation results: generated/GCC full suite **57 passed, 4 failed,
1 skipped**; fallback/Clang full suite **53 passed, 2 failed, 1 skipped**;
focused Clang ASan/UBSan **6/6 passed**. Remaining failures stop at the stale
Elvi-scrape manual pin; see [verification](../VERIFICATION.md).

PrettyPrint layout continuation, MuLIB `db7330a875208752ff3b982a5a6474c4e7c8a8bb`:

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| PrettyPrint document rendering | `include/mulib-pretty{doc,emit,sexpr}.h`, `stdlib/prettyprint`, `examples/ppgc.py` | Single-TU C11 kernels and raw scalar companion; maps and incremental sessions on C side | Language-neutral `sexpr` DSL with semantic math and Knuth–Plass; `.ppg` v1 compiles tagged AST rules to Python | `prettyprint.mk`, `tests/check_prettyprint.py`, `tests/test_linebreak.c` | EXECUTABLE | No safe FFI admission, glyph justification, full Oppen, or arbitrary subtree reuse |

KernelBind continuation (Packages A/B), baseline MuLIB
`db7330a875208752ff3b982a5a6474c4e7c8a8bb`, read-only LibStone
`c7cfeca5a5d5086b0c25dad08264fc450c03f85b`. See [KernelBind](kernel-bind.md)
for the discovery profile and its limits.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| KernelBind import discovery | `src/{parser,loader,cli,ppx_ast}.cpp`, `tools/{on1x-build,on1x-exotract}.py`, `contracts-exotract/kernel-discovery-v1.lua` | ExoLib/Exotract declaration-only ExoML CDecl v2 | Native contextual imports and read-only source graph, pinned MuLIB discovery and unreviewed sidecars | `test_kernel_imports.py`, `test_kernel_imports_sdk.py` | PARTIAL | More profiles and independent safe authorization |
| KernelBind clamp v2 | `include/on1x/kernel.hpp`, `src/{loader,sema,codegen}.cpp`, `tools/{on1x-build,on1x-exotract}.py`, `contracts-exoml/clamp.exocontract.json` | All three canonical clamp functions, strict C11 on Linux x86-64, one implementation TU; safe path requires independent signed review | Exact `i64` arguments, scoped unsafe or review-gated direct calls; interval wrapper checks bounds | `examples/kernel/clamp{,_safe}.on1x`, C oracle, `on1x.kernel_imports`, `on1x.kernel_review_sdk` | EXECUTABLE | No production review is shipped; more general scalar shapes and alternative targets/backends |
| KernelBind safe calls and foreign resources | `KERNEL_DIRECT_USAGE.md` | No safely admitted symbols or resource-bearing shapes | Default imports fail closed | Explicit fail-closed compilation tests | ABSENT | Independent review, ownership/errors/effects, target execution and Shylock milestone |

Search-path/ecosystem work item (M5/M7), baseline MuLIB
`b11af815facb35a791d9721c4c654df21b877d94`, read-only LibStone
`c7cfeca5a5d5086b0c25dad08264fc450c03f85b`:

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Integrated stdlib MuLIB package | `stdlib/mulib`, build/install/extraction tools | Existing raw adapters and one implementation TU; no new ABI | Existing `std.mulib` imports | Library/install/extension/MuLIB and discovery suites | EXECUTABLE | No current fixture blocker; these remain raw adapters rather than general safe bindings |
| Library and stdlib search lists | `src/{discovery,loader,cli}.cpp`, `stdlib/On1xfile`, build helper | Build-time only; compiler-resolved Basis resources | Existing imports; `ON1X_LIB_PATH`, `ON1X_STDLIB_PATH`, matching CLI flags; declarative version-1 source roots | `test_discovery.py`, `test_modules.py`, `test_install.py`; compiled ordinary/stdlib/prelude clients, malformed/escape/budget rejection, ordered roots and explicit-path compatibility | EXECUTABLE | Linux-only evidence; compiler discovery does not itself resolve the project tool's lockfile; no separate compilation or persisted interfaces |
| PPX and stdppx search lists | `src/{discovery,ppx,cli}.cpp`, CMake, script second-line metadata | Existing bounded, explicitly authorized process transport; native bundle executable/schema paths | `ON1X_PPX_PATH`, `ON1X_STDPPX_PATH`, matching CLI flags, `--use-ppx`; native `On1xfile` and inline script manifests | `test_discovery.py`, PPX/modules/install suites; native/script C11 clients, authorization/metadata/path escape negatives, relocation, ASan/UBSan | EXECUTABLE | No process sandbox or implicit native-source compilation; trusted Category registry remains independent |
| MuLIB kernel search lists | Build/extraction/inspection helpers, `stdlib/mulib/housekeeping.py` | Existing MANIFEST-backed single-TU planning; exact selected header paths/hashes and archive checks | `ON1X_MULIB_PATH`, `--mulib-path`; no new foreign admission | `test_discovery.py`, optional real-SDK `test_kernel_paths.py`; multi-root kernel C11 execution, CLI/env precedence, source-pin/archive drift refusal | EXECUTABLE | Unknown unmanifested kernel recipes/ABI remain unsupported; metadata/companions require a real checkout |

Category evidence continuation baseline: MuLIB
`2ff743469c62b6a369b21bcbfb045e05b30d27f3`; configured read-only LibStone
`c7cfeca5a5d5086b0c25dad08264fc450c03f85b`. Work item: Category M2,
checked concrete evidence requests and deterministic post-inference planning.
Native processor version 2 now executes syntax and typed phases. Full scope,
commands, measured results and exclusions: `../VERIFICATION.md` and `category.md`.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Checked Category evidence | `src/{extensions,extension_evidence,loader,ppx,sema,codegen}.cpp`, `src/backend/lower.cpp`, `stdlib/ppx/category/{metadata,typed,witness}.on1x` | Existing scalar functions/C11; no new application ABI | Host-retained scoped member schemes, native typed feedback, independent concrete type/provenance/completeness/forwarding checks at check/C11/MIR ingress | `test_category.py`, `test_extension_evidence.cpp`; Eq/ordering/evidence executables, forged/stale plan and HIR mutation negatives | EXECUTABLE | Imported/persisted interfaces, generic evidence and constraint solving; full Category M2 remains partial |
| Category plan inspection | `src/{extensions,extension_evidence,cli}.cpp` | Host-side bounded S-expression output | `--emit-category-plan`, sorted checked choices and source/compiler/processor/schema/registry digests | Repeat/identity-PPX equality, source-change invalidation, alias identity and relocation in `test_category.py` | EXECUTABLE | Inspection only; no reusable proof/cache or target-ABI/effect certificate |

Cancellation/deadline continuation at `1f027c48bc6363dce5752c00057830777325ff47`,
configured read-only LibStone `55755485acaded7c4ed804c0946bc8c7363f6763`:

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Monotonic poll deadline | `src/{parser,infer,sema,concurrency,codegen,ppx_ast}.cpp` | Private timer-channel offer using existing atomic MuLIB select, CLOCK_MONOTONIC supervision; no kernel timeout change | One `after` per poll, timer-only choices, i64 milliseconds or ms/s units, optional default; located invalid-duration faults | `test_cancellation_deadlines.py`, `test_concurrency.cpp`, clock-jump/OOM fixtures, compiled example, scanner parity; exact evidence in `../VERIFICATION.md` | EXECUTABLE | No typed timeout/cancel join outcomes, effectful MIR or other-target validation |
| Taskgroup deadline | `src/{parser,infer,sema,concurrency,codegen,ppx_ast}.cpp`, `runtime/on1x_concurrency.*` | CSP ABI v4; existing supervisor, cancellation and cleanup | `taskgroup within duration { ... }`, checked i64 milliseconds or ms/s; cooperative terminal expiry after cleanup; ordinary affine obligations unchanged | `test_taskgroup_within.py`, CSP HIR, real and deterministic monotonic clocks, both scanners, strict GCC/Clang C11 and ASan/UBSan; see `../VERIFICATION.md` | EXECUTABLE | No typed recovery, general async or effectful MIR |
| Running/blocked fiber cancellation | `src/{concurrency,codegen}.cpp`, `runtime/on1x_concurrency.*` | Private cancel offer, explicit cleanup returns, shielded defers, descendant cancel/join, existing process-group cancellation/reaping | Consuming affine `cancel` before dispatch, while running or blocked; same-parent checks, no forced native preemption | `test_cancellation_deadlines.py`, `test_concurrency.cpp`; loop/call/defer/RC/nested/byte waits, observer reuse, OOM, strict C11 and ASan/UBSan | EXECUTABLE | General effects/ownership MIR, native workers, independent SDK models and general async suspension |
| Stackless async/await | Existing scalar MIR and CSP HIR | No admitted future frame/runtime ABI | Not admitted | Unsupported-profile coverage only | ABSENT | Typed verified suspension/frame lowering; not stackful launch sugar |

Linux 6.18.52 x86_64/glibc 2.44, GCC 16.2.1 and Clang 22.1.8.
Full fallback/GCC: **52 passed, 1 failed, 1 skipped**; full generated/GCC:
**53 passed, 1 failed, 1 skipped**. The failure is the unchanged
`tools/embed_units.py` fixture fingerprint; Category skips with native PPX off.
Focused ASan/UBSan: **4/4**; strict Release: **2/2**; scanner parity: **302**.
See [the profile](cancellation-deadlines.md). This request is only partially
implemented: stackless async/await and typed timeout recovery remain absent.

G4 stream-poll continuation (baseline `d4cc478df76bfe9b5b8e6485f42d56fde81e8a68`):
atomic `poll` now accepts captured process byte endpoints, including mixed i64/u8
offers. Configured LibStone remains `55755485acaded7c4ed804c0946bc8c7363f6763`.
The example previously failed with `expected sender<i64>, got sender<u8>`.
This task does not implement deadlines, cancellation unwinding or the full syntax
specification. See [the exact profile](stream-poll.md) and `../VERIFICATION.md`
for Linux x86_64/GCC/Clang commands and test results.
Final full suites: fallback/GCC **51 passed, 1 failed, 1 skipped**;
generated/GCC **52 passed, 1 failed, 1 skipped**. The unchanged failure is the
stale `tools/embed_units.py` fingerprint; Category skips in native-PPX-off
profiles. Clang ASan/UBSan focused **3/3**, strict GCC Release focused **2/2**,
scanner parity **301 cases**, native identity PPX and standalone strict Clang
C11 examples execute. These results do not complete G4 or the syntax specification.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Mixed i64/process-byte atomic poll | `src/{infer,sema,concurrency,codegen}.cpp` | Existing MuLIB atomic selection and private ABI v2; exact uint8_t staging in `runtime/on1x_concurrency.c`, explicit widening/narrowing | Existing source `poll`, exact i64/u8 payloads/binders; 1..16 named offers with mandatory close arms and optional default | `test_stream_poll.py`, `test_concurrency.cpp`, `fixtures/stream_poll_boundary.c`, compiled `examples/cooperative_stream_poll.on1x`; seeded full-byte-domain oracle, mixed offers, 0/1/7 capacity, losing offers, EOF/default/cancel, wire/PPX, C/C++ TU callers, malformed HIR, runtime range/direction/OOM, strict C11 and ASan/UBSan | EXECUTABLE | No general byte-channel constructor, deadlines, running cancellation, general ownership/effect MIR, other targets or full-spec conformance |

Earlier G4/v3 evidence (before the stream-poll continuation above): compiler-parsed backtick command literals and captured
process byte streams now execute as affine `sender<u8>`/`receiver<u8>`
capabilities. They extend the existing bounded direct-exec external fibers and
consuming process cancellation, plus internal cooperative fibers,
unidirectional i64 channels and bounded atomic `poll`, using canonical
source/HIR/C11 and MuLIB kernels. Re-inspected HEAD
`8996396506901f792c0b697ebbf8d6a2a233f0f6`, configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` (read-only). Linux
6.18.52 x86_64/glibc 2.44, GCC 16.2.1, Clang 22.1.8. Final fallback/GCC
suite: **50 passed, 1 failed, 1 skipped**; generated/GCC suite:
**51 passed, 1 failed, 1 skipped**;
fallback/Clang ASan+UBSan focused: **2/2**; scanner parity: **300** exact
cases. The sole full-suite failure is the pre-existing stale
`tools/embed_units.py` fixture fingerprint. Exact commands and unsupported
profiles are recorded in `../VERIFICATION.md`.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Internal cooperative fibers / task scopes | `src/{parser,infer,sema,concurrency,codegen}.cpp`, `include/on1x/concurrency.hpp` | `runtime/on1x_concurrency.[ch]`, existing MuLIB N:1 fiber scheduler, compiler-generated C11; one native thread | `taskgroup`, eager `launch { ... }`, affine `ifiber<i64>`, consuming `join` or immediate same-block pre-dispatch `cancel`, immutable i64/scoped endpoint captures | `test_concurrency.{cpp,py}`, compiled `examples/cooperative_channels.on1x`, repeated ready cancellation, negative ownership/HIR/wire, strict GCC/Clang, ASan/UBSan | EXECUTABLE | No cancellation after scheduling, deadlines, async/await or native worker boundary; no complete effect/ownership MIR or TSAN evidence |
| Typed uni channels | `src/{parser,infer,sema,concurrency,codegen}.cpp`, `grammar/On1x.g` | `runtime/on1x_concurrency.[ch]` and one-TU MuLIB channel/fiber/xthreads kernels; bounded FIFO/rendezvous | `->(1,1){payload:i64,capacity:0..256,mode:uni}`, consuming split, typed checked send/receive and close | `test_concurrency.{cpp,py}`, generated/fallback scanner parity, repeated rendezvous, buffered/drained close, closed-send result, deadlock/budget tests | EXECUTABLE | Only 1:1 i64 uni; no endpoint cloning/duplex, cancellation, monotonic deadline or general resource payload |
| Atomic `poll` / select | `src/{parser,infer,sema,concurrency,codegen}.cpp` | `runtime/on1x_concurrency.[ch]` calls MuLIB atomic multi-case select; 1..16 named uni/i64 offers | `recv`/`send`/mandatory `closed` and optional `default`, typed expression or statement arms; `after` rejected | `test_concurrency.py` compiled choice, rendezvous, closure, evaluation order and rejection; generated/fallback and ASan/UBSan focused tests | EXECUTABLE | No monotonic deadline, cancellation, general endpoint expressions or recoverable allocation errors; no effectful ownership/suspension MIR |
| External direct-exec `xfiber` / process cancellation | `src/{parser,infer,sema,concurrency,codegen,ppx_ast,type}.cpp`, `include/on1x/type.hpp` | `runtime/on1x_concurrency.[ch]` POSIX spawn, process group, nonblocking wait/reap, SIGTERM→SIGKILL escalation, 128 total launch budget; C11 generated program | `launch exec(...)` or single/triple backtick direct argv command (1..9 bounded words), affine `xfiber`, consuming `join` (signed POSIX status) or `cancel` (bool); process scoped to taskgroup | `examples/cooperative_external.on1x`, `test_concurrency.{cpp,py}`, scanner parity, quote/escape/operator negatives, cancellation ignoring SIGTERM, failure/budget and sanitizer tests | EXECUTABLE | No full shell expansion/operators/builtins, typed exit-status records or recoverable spawn errors; no running internal-fiber cancellation or effectful MIR |
| Captured process byte streams | `grammar/On1x.g`, `src/{lexer_fallback,parser,infer,sema,concurrency,codegen,ppx_ast,type}.cpp` | ABI v2 in `runtime/on1x_concurrency.[ch]`: three nonblocking POSIX pipes supervised through bounded MuLIB byte channels; per-write parent SIGPIPE mask restored; child SIGPIPE default | `launch capture` binds exactly `(xfiber,sender<u8>,receiver<u8>,receiver<u8>)`; checked stdin send/close and stdout/stderr `Option<u8>` receive; endpoints are affine and movable to reader fibers; byte `poll` rejects | `examples/cooperative_process_streams.on1x`, `tests/fixtures/process_stream_child.c`, `test_concurrency.{cpp,py}`; binary 0..255 round-trip, NUL, separate readers, capacity-0/1/7 backpressure, EOF/EPIPE, cancel/escalate/EOF cleanup, malformed HIR/wire/source, spawn/channel-budget failures | EXECUTABLE | Linux/glibc x86_64 only; no byte-channel constructor/select, deadline/cancellation-aware waits, typed stream/spawn errors, aggregate/effect MIR, full sanitizer/cross-platform matrix |
| Async/await, running/blocked internal cancellation, actors, native workers | `AGENTS.md`, `ON1X_CORE_SYNTAX_ADDITIONS.md` | No interruptible MuLIB channel waits or On1x cleanup edges for running internal cancellation | `cancel ifiber` rejects unless it immediately follows the launch in the same block | `test_concurrency.py` delayed/expression cancellation negatives | ABSENT | Effectful/suspending MIR, lifetime/memory model, deterministic C11 async frames and worker isolation |

M2 continuation: completed Go-style function-scoped defer and typed `Result` propagation
for the bounded C11 profile.
Re-inspected clean MuLIB `604db67c3160e7958345a3c1fefe757e5f499b95` and configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` (unchanged/read-only).
Linux 6.18.52-1-lts x86_64, glibc 2.44, GCC 16.2.1, Clang 22.1.8,
CMake 4.4.3, Python 3.14.7. The initial WIP baselines were GCC/generated
47/49 and Clang/fallback 47/48 (stale defer-rejection/scanner expectations).
Final GCC/generated **51/51**, Clang/fallback **50/50**, SDK **60/60**;
focused Release/fallback/native-off **10/10**. SDK ASan/UBSan **60/60**, including
generated programs, with no findings. Exact commands and previous failures are
recorded in `../VERIFICATION.md`.
See `defer-errors.md` for the selected semantics;
this does not authorize concurrency, general foreign owners or trap unwinding.

Historical M1 slice: nonrecursive nominal records and parameterized enums, through
the canonical frontend and C11 value representation. Baselines at the revisions
below: GCC/generated 47/47, Clang/fallback 46/46 (`results-adts-baseline.xml`).
The nonrecursive nominal slice now executes real source programs through C11;
its final combined-tree validation is recorded in `../VERIFICATION.md`.
Recursive/owning ADTs, lists, defer/error propagation and concurrency were not
admitted by that increment. The later
[finite unique-owner profile](recursive-owning-adts.md) supersedes only the
recursive-layout/unique-owner absence; its other boundaries remain explicit.

Nominal-data final combined-tree suites: GCC/generated **49/49**, Clang/fallback
**48/48**, SDK **58/58**, SDK ASan/UBSan **58/58** (including generated programs;
no findings). Focused Release/fallback/native-off **8/8**. Commands, historical
failures and unsupported profiles are preserved in `../VERIFICATION.md`.

Earlier value-loop continuation: re-inspected MuLIB
`7391d9e9d58e987ea234c64dbfd352c31f220537` and unchanged LibStone
`55755485acaded7c4ed804c0946bc8c7363f6763`. Scope: intrinsic expression
`loop`, typed `break with`, lexical targets and existing cleanup/C11/MIR paths.
Owning lists, defer/typed errors, concurrency and nominal ADTs remain pending;
this slice does not add syntax-only implementations or broader safety claims.

Value-loop combined-tree validation: GCC/generated **47/47**, Clang/fallback **46/46**,
SDK **56/56**, SDK ASan/UBSan **56/56** (generated programs instrumented; no
findings), focused GCC Release/fallback/native-off **6/6**. Exact configurations,
commands and historical failed attempts are preserved in `../VERIFICATION.md`.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Nonrecursive nominal declarations (M1 slice) | `src/{parser,type,nominal,infer,sema,loader,ppx_ast}.cpp`, `include/on1x/{ast,type,nominal}.hpp` | Existing bounded by-value C11 structs/tags; exact catalog/layout validation at backend ingress; no new runtime/foreign ABI | Named records/parameterized enums, inferred invariant arguments/phantoms, generic/forall functions, declared-catalog matching, imported type identity | `test_nominal_adts.{cpp,py}`, `examples/nominal_adts.on1x`; strict C11/C++ cross-TU, seeded oracles, hygiene/staging/PPX/imports, capture/OOM, malformed-HIR/wire and resource/budget negatives; exact results in `../VERIFICATION.md` | EXECUTABLE | Unique recursion is a separate direct-C11 profile; aggregate MIR/SDK models, separate compilation and other targets remain |
| Value-producing loops | `src/{parser,infer,sema}.cpp`, `src/canonical/{control,values,verify_values}.cpp`, source transforms | Existing C11 lexical RC/JSON cleanup and scalar/numeric tuple MIR slots/edges; no ABI change | `loop` expressions, labeled/typed `break with` joins; void on bare break; possible/definite initialization and backedge verification | `test_value_loops.{cpp,py}`, `examples/value_loops.on1x`; C11/MIR oracles, mutation/wire/import/generic/macro/PPX/staging/closure/JSON/fault tests; exact matrix in `../VERIFICATION.md` | EXECUTABLE | General bottom type, unique-resource transfer, aggregate/managed MIR and broader SDK models |
| Intrinsic owning-list / iterator drop protocol | User-defined recursive nominal lists now use `own<Named>`; existing persistent `sexpr` facades remain | No dedicated list/iterator ABI or protocol | No intrinsic list type, list-comprehension/cons syntax or generalized iterator/drop protocol | Existing unsupported-profile fixtures plus recursive-owner list tests | ABSENT | Iterator semantics, syntax, protocol coherence and MIR/effect integration |
| Go-style function defer (M2) | `src/{parser,infer,sema,codegen}.cpp`, `src/canonical/{values,verify_values}.cpp`, `runtime/on1x_runtime.*` | Per-invocation typed LIFO records/captures in environment allocator domain; 4,096 records / 8 MiB immediate captures; located count/byte/OOM failures; C11 ABI guard | `defer call;`, registration-time callable/argument snapshots, ordinary immutable/RC captures; explicit/implicit/error returns drain before remaining lexical cleanup | `test_defer_errors.{cpp,py}`, `test_defer_runtime.c`, `examples/defer_errors.on1x`; source/HIR/wire negatives, loop/recursive/capture/outcome oracles, RC/JSON/fault/budget/C/C++ boundaries; exact profiles above and in `../VERIFICATION.md` | EXECUTABLE | No using, exception/trap/cancellation unwind, JSON captures, effectful MIR/independent SDK models, other-target certification |
| Typed recoverable Result propagation (M2) | `src/{parser,infer,sema,nominal}.cpp`, `src/canonical/{values,verify_values}.cpp`, source/PPX adapters | Existing bounded by-value nominal C11 tags; explicit checked early-return/cleanup edges; no foreign ABI change | Postfix `?` on nominal enum with exactly Ok(T)/Err(E); same declaration/error type on return, independent success type; explicit match recovery | `test_defer_errors.{cpp,py}`; exact errors/module identity, later-operand suppression, control regions, generics/hygiene/PPX, C11 O0/O2 seeded oracles and private-HIR mutations | EXECUTABLE | Managed/owning Result payloads, Option propagation, general exceptions/effect inference, aggregate MIR/SDK models, general foreign error adapters |
| Recursive / owning nominal ADTs | `src/{type,nominal,infer,sema,codegen}.cpp`, canonical value lowering/verifier | Unique ABI v1, bounded allocation/cleanup records and iterative drop | Only `own<Named>` cuts direct/indirect by-value cycles; explicit move-only operations | `test_recursive_owning_adts.{cpp,py}` plus retained by-value-cycle negatives | EXECUTABLE | General borrow/resource model, MIR/SDK/PPX/FFI/concurrency integration and separate compilation |

## M2 continuation: immutable C-string admission

Baseline: MuLIB `7391d9e9d58e987ea234c64dbfd352c31f220537`, configured
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763`. This slice extends the
existing reviewed scalar admission, not the general ownership/FFI profile.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Reviewed immutable C strings | `tools/on1x-bind.cpp`, `src/safety.cpp`, `include/on1x/safety.hpp` | Exact `const char *`, bounded call-scoped inputs and non-null immutable static results; exit 72 on null/limit failure | Existing `string`; directional v4 admission evidence | `test_contract_strings.py`, `test_safety.cpp`; isolated GCC/generated 50/50, Clang/fallback 49/49, Clang/generated ASan/UBSan 50/50; commands in `../VERIFICATION.md` | EXECUTABLE | No independently reviewed standard bridge contracts; owned/retained buffers, allocation/error/effect lowering unsupported |
| General standard bridge safe exports | `stdlib/_bridge/`, `contracts-exoml/` | Existing bounded raw adapters | Existing raw externs/status tuples | Historical bridge tests below | PARTIAL | Reviewed contracts, owned-resource/effect lowering; not promoted by this slice |

Structural-match continuation: MuLIB
`9090e643e11051e000b3bcc68ebc777dfe07776a`, pinned LibStone
`55755485acaded7c4ed804c0946bc8c7363f6763` unchanged. Shared structural patterns,
guarded expression/statement matches and typed decision regions execute in C11,
with scalar/numeric tuple MIR differential tests. Previous loop/Category changes
remain intact. Full-specification concurrency/cleanup/list/value-loop capabilities
are not admitted by this slice. Exact evidence: `../VERIFICATION.md`.
Final matching suites: GCC/generated **45/45**, Clang/fallback **44/44**, SDK
**53/53**, SDK ASan/UBSan **53/53**; focused native-off Debug/Release **4/4** each.

Earlier statement-loop continuation: HEAD `9090e643e11051e000b3bcc68ebc777dfe07776a`,
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763`. Executable G1/G2 slice:
intrinsic statement loops, i64 ranges and lexical exits; existing Category work
is preserved. General match/iteration, value loops, cleanup and concurrency are
not admitted by this slice.
Final core-syntax suites: GCC/generated **43/43**, Clang/fallback **42/42**, SDK
**51/51**, SDK ASan/UBSan **51/51**; focused Debug and Release profiles **2/2**
each. Exact commands, instrumentation, migration limits and unsupported cases
are recorded in `../VERIFICATION.md` and `core-syntax.md`.

Category continuation baseline: MuLIB `9090e643e11051e000b3bcc68ebc777dfe07776a`,
LibStone unchanged at `55755485acaded7c4ed804c0946bc8c7363f6763`. CATEGORY M0/M1
now has a runnable local scalar profile: pinned native activation, balanced spans,
On1x grammar/coherence/substitution and ordinarily checked explicit selectors.
Constrained generics, cross-module evidence, associated types, functors/sealing
and auto protocols remain unimplemented. See `category.md` and the exact tests
in `../VERIFICATION.md`; this is not Category completion.
Category final suites: GCC/generated **41/41**, Clang/fallback **40/40**, SDK
**49/49**, SDK ASan/UBSan **49/49**. Standalone ordinary PPX suite: **30/30**.

Inspection baseline for this continuation: MuLIB `cbdb2e290a41e39a8e6447f39426b1407c3e73b0`,
LibStone `55755485acaded7c4ed804c0946bc8c7363f6763` at
`on1x/third_party/libstone` (read-only). Linux x86_64; GCC 16.2.1,
Clang 22.1.8. This matrix distinguishes executable subsets from completion of
the general capabilities. Historical evidence remains in `../VERIFICATION.md`.
The two fresh pre-change baseline suites passed (GCC/generated 36/36,
Clang/fallback 35/35). Final continuation commands/results are recorded in
`../VERIFICATION.md`; the full-language mandate is not complete.

The next continuation implemented decimal f64 literals and MIR v2 for floats and
trivially copied tuples. Existing scalar artifacts remain v1. Foreign ownership
and concurrency are not admitted by these slices.

Data-model continuation implemented: structural records, closed tagged sums with
exhaustive elimination, bounded fixed arrays and owning snapshot slices. These
are a by-value C11 profile, not general borrowed/dynamic buffers or foreign ABI.

Safe-FFI continuation implemented reviewed scalar-only profiles: exact signed
integer, `_Bool` and binary64 `double` C ABI (void result), reviewed ExoML claims, all-or-nothing profile admission,
Chisel-checked generated usage, strict target compilation/execution and a
machine-readable evidence manifest. It is not general resource-safe FFI.
Post-change suites: GCC/generated **38/38**, Clang/fallback **37/37**, optional
SDK **46/46**, optional-SDK ASan/UBSan **46/46**.

M2 typed-scalar continuation: exact `_Bool`/binary64 `double` adapters, typed
Chisel admission and v2 evidence now execute C/C++/On1x boundary programs and a
seeded bit oracle. Integer-only v1 evidence is preserved without widening.
Post-continuation profiles and exact commands are recorded in `../VERIFICATION.md`.

Numeric continuation implemented `u8/u16/u32/u64` and IEEE binary32 `f32` through
the canonical HIR/C11 path, with explicit conversions/bit constructors (existing
unsuffixed literals remain i64/f64). Numeric semantics are specified in
`numerics.md`; at that stage MIR and independent structural verification still
rejected the new types (superseded by the v3 continuation below). Post-change profiles:
GCC/generated **39/39**, Clang/fallback **38/38**, optional SDK **47/47**,
SDK ASan/UBSan **47/47** (explicit generated-program sanitizer flags; see the
correction of earlier sanitizer scope in `../VERIFICATION.md`).

Numeric-limit continuation implemented scalar MIR v3 execution (unsigned/f32
operations and conversions), independent numeric HIR type obligations and
suffixed literals, preserving legacy MIR/evidence formats. Post-change suites:
GCC/generated **40/40**, Clang/fallback **39/39**, SDK **48/48**, SDK ASan/UBSan
**48/48**. Other-target floating support remains blocked on actual toolchains
and execution evidence, not deletion of platform/environment guards.

| capability | source files | runtime/ABI support | syntax/type support | tests | status | remaining blocker |
|---|---|---|---|---|---|---|
| Generalized structural matching (G2 bounded profile) | `src/{parser,patterns,infer,sema}.cpp`, `src/canonical/{values,verify_values}.cpp`, loader/macro/PPX/staging adapters | Existing by-value/RC C11 and lexical cleanup; scalar/numeric tuple MIR; separate unique-owner C11 path | Guards, or/as, wildcard/rest, tuples, closed sums, records, exact arrays/snapshot slices; finite coverage/redundancy. Move-only nominal sums use exhaustive unguarded consuming arms only | `test_patterns.{cpp,py}`, `test_recursive_owning_adts.{cpp,py}`, source examples, seeded MIR/C11 oracle, wire/mutation/fault/JSON tests | EXECUTABLE | Intrinsic list patterns, guarded/nested-refutable resource matches, refutable let/parameters, aggregate/managed MIR and broader SDK models |
| Typed decision/initialization regions | `src/canonical/{values,verify_values}.cpp`, `src/{codegen,safety,type_safety}.cpp`, `src/backend/lower.cpp` | Existing C11 scopes/cleanup and MIR slots/branches/jumps; no runtime/ABI addition | Surface matches erased; independent initialized-local/type/tag-domination/region checks; private nodes not PPX constructors | HIR mutation rejection, transactional lowering, idempotence, guard traps, nested return/break/continue and for-latch regression | EXECUTABLE | Full effect/ownership/error/suspension Core; no general bottom type |
| Intrinsic statement loop control (G1/G2) | `src/{parser,sema,infer,codegen}.cpp`, `src/canonical/control.cpp`, `src/backend/lower.cpp`, SDK adapters | Existing C11 lexical ownership cleanup; explicit MIR branches/jumps | Statement loop/for-C, lexical labels/exits; checked targets/latches across macros/PPX/staging | `test_control.{cpp,py}`, closure allocator/JSON cleanup, MIR/oracles/wire/HIR negatives | EXECUTABLE | General iterator protocol and full effect/cleanup Core; expression-loop values are a subsequent slice above |
| Intrinsic i64 ranges | `grammar/On1x.g`, `src/lexer_fallback.cpp`, `src/{parser,canonical/control}.cpp` | Checked C11 i64; inclusive completion before increment | Once-evaluated ascending `..`/`..=` bounds; immutable iteration binding | Scanner parity, extrema/reversed/zero bounds, MIR/C11 seeded oracle | EXECUTABLE | Other scalar ranges, custom steps and general iterator/drop protocol |
| Category native protocols (M0/M1) | `src/extensions.cpp`, `src/{parser,loader,cli}.cpp`, `stdppx/native/protocols.on1x`, `stdlib/ppx/category/` | Native bootstrap processor, trusted registry/SHA256 pinning, ordinary HIR/MIR/C11 selectors | Module-local concrete scalar protocols/witnesses; generic balanced carriers; normal post-expansion typing | `test_category.py`, actual Eq/ordering clients, malformed packet/type/coherence/import/relocation tests | EXECUTABLE | Qualified generics, imported evidence/interfaces, associated types, functors/sealing and ownership auto protocols; concrete typed feedback added above |
| Generated/fallback scanning | `grammar/On1x.g`, `src/lexer*.cpp` | Bounded token callback | Functional syntax, decimal floats and typed numeric suffixes | `test_scanner.cpp`, `scanner_parity.py` | EXECUTABLE | Broader malformed-input/target certification |
| Checked i64 | `src/{parser,infer,sema,codegen}.cpp` | Checked C11 runtime operations | i64 literals/operators | `test_compiler.py`, `test_mir.py` | EXECUTABLE | Broader target conformance |
| f64 values | `src/{type,parser,infer,sema,codegen,primitives,safety,type_safety}.cpp`, `runtime/on1x_runtime.*` | Linux x86_64 binary64; guarded raw/generated double ABI; bit transport/hex output | Arithmetic/comparisons, explicit conversions, tuples/closures/staging; Chisel and independent numeric typing | `test_f64.py`, `test_meta.cpp`, `test_types.cpp`, `test_type_safety.*`, `test_contracts_sdk.py` | EXECUTABLE | Other targets and full IEEE conformance certification |
| Numeric source literals | `src/{numeric,parser,lexer_fallback,macro_rules,ppx_ast,ppx_schema}.cpp`, `grammar/` | Exact integer and direct binary32/64 bit emission; 1024-byte typed literals | Decimal/hex unsigned suffixes; decimal f32/f64 suffixes; backward-compatible unsuffixed i64/f64; shared macro/PPX categories | `test_numeric_limits.py`, `test_f64.py`, `scanner_parity.py`, native/Python PPX tests | EXECUTABLE | Hex float syntax and broader compiler/host certification |
| Unsigned fixed-width words | `src/{type,parser,infer,sema,codegen,primitives,type_safety}.cpp`, `src/backend/`, `runtime/on1x_runtime.*` | Exact uint8/16/32/64 C11 ABI; checked arithmetic, wrapping/bits/conversions; scalar MIR v3 verifier/evaluator/emitter | Distinct u8/u16/u32/u64, suffixed literals; independent numeric typing; functions/tuples/value data/closures/staging (last kinds remain HIR-only) | `test_unsigned_f32.py`, `test_numeric_limits.py`, `test_contracts_sdk.py`; Python oracle, strict C/C++ and ASan/UBSan | EXECUTABLE | Broader targets; Basis word facades retain legacy encodings; general aggregate/closure MIR |
| f32 values | `src/{type,parser,infer,sema,codegen,primitives,safety,type_safety}.cpp`, `src/backend/`, `runtime/on1x_runtime.*`, PPX schemas | Linux x86_64 IEEE binary32; exact bits/hex/C float ABI; scalar MIR v3 independent evaluation and emission | Distinct f32 with directly rounded decimal literals and explicit conversions; no implicit promotion; independent typing | `test_unsigned_f32.py`, `test_numeric_limits.py`, `test_type_safety.*`, `test_contracts_sdk.py`; bit/rounding/failure oracles and ASan/UBSan | EXECUTABLE | Other targets and full IEEE conformance certification |
| Narrow signed / pointer-sized integers | `include/on1x/type.hpp` | None | None | None | ABSENT | Exact semantics and ABI for i8/i16/i32/isize/usize |
| Functions / closures | `src/{infer,sema,codegen}.cpp` | Single-thread RC environments | Structural functions, immutable captures | `test_functions.py`, `test_closures.py` | EXECUTABLE | Mutable/resource capture lifetimes |
| Tuples / irrefutable patterns | `src/{parser,infer,sema,codegen}.cpp` | Internal C11 by-value records | Shared name/tuple patterns; new matches add refutable patterns without changing `_` in old lets/parameters | `test_tuples.py`, `test_parameter_patterns.py`, `test_patterns.py` | EXECUTABLE | Foreign aggregate ABI; refutable let/parameter patterns |
| Polymorphism | `src/infer.cpp` | Bounded whole-unit specializations/bundles | Restricted schemes / forall | `test_poly*.py` | PARTIAL | General parameterized data types |
| Records / ADTs / exhaustive matching | `src/{parser,infer,sema,codegen,patterns,nominal}.cpp`, `src/canonical/`, `src/type.cpp` | Private versioned C11 structs plus unique ABI for finite owning recursion | Structural and nominal records/sums, invariant parameters, ordinary guarded matching, and restricted consuming matches for move-only sums | `test_data.py`, `test_patterns.py`, `test_nominal_adts.*`, `test_recursive_owning_adts.*`, examples | EXECUTABLE | Derives, data equality/printing, field mutation, generalized resource patterns and aggregate/ownership MIR/SDK lowering |
| Arrays / slices | `src/{parser,infer,sema,codegen}.cpp`, `runtime/on1x_runtime.*` | By-value fixed arrays; owning snapshot slices with composed offsets; bounds runtime with located exit-70 errors | `array<T,N>`/`slice<T,N>` (extent <= 256), `array[...]`, `a[i]`, `a[]`, `a[lo:hi]`, slice-of-slice; no mutation/borrowing | `test_data.py`, `examples/data_model.on1x` | EXECUTABLE | Mutable arrays, zero-copy borrows, dynamic buffers, foreign ABI |
| Modules | `src/loader.cpp` | Whole-unit C11 | Namespaces, imports, aliases | `test_modules.py`, `test_loader.cpp` | PARTIAL | Interfaces, separate compilation, signatures/functors |
| Macros / PPX | `src/{macros,macro_rules,ppx,ppx_ast}.cpp` | Build-time processes; bounded wire format | Hygienic macros, external PPX | `test_macros.py`, `test_ppx.py`, `test_local_syntax.py` | EXECUTABLE | Process sandboxing; richer data derives |
| Staging | `src/staging.cpp` | AOT closure thunks | Closed code, quote/splice/lift/run | `test_staging.py` | EXECUTABLE | Broader staged data types |
| Managed data / text | `src/primitives.cpp`, `runtime/on1x_runtime.c` | RC sexpr/buffer; bounded text snapshots | Primitive managed types | `test_selfhost.py` | PARTIAL | Owning Unicode text / binary slices |
| Ownership / recoverable errors | `src/{type,nominal,sema,safety,codegen}.cpp`, `src/canonical/`, `runtime/` | Lexical JSON/RC cleanup, finite unique-owner ABI, and typed Result early returns; fatal failures remain terminal | Explicit whole-value unique moves plus restricted resource flows; bounded nominal Result propagation and function-scoped defers remain separate profiles | `test_json.py`, `test_safety.*`, `test_defer_errors.*`, `test_recursive_owning_adts.*` | PARTIAL | General borrows/receivers/foreign resources, move-only Result/defer composition and full effect/cleanup MIR |
| Generic Option / Result | `stdlib/{pervasive,core/Pervasive}.on1x`, existing nominal frontend/C11 lowering | Native bounded nominal enums; older tuple/scalar-pair/S-expression facades preserved | Canonical default `Option<T>`/`Result<T,E>`; copy-safe constructors/helpers, predicates, map/chain/error-map, fallbacks, conversions, iteration and Result `?`; automatic names obey import/local precedence | `test_prelude.py`, nominal/defer/module/library compatibility suites, `examples/pervasive_options.on1x` | EXECUTABLE | Move-only helper/propagation/defer profiles, Option propagation and aggregate ownership MIR/SDK models |
| Exceptions / effect system | `src/backend/mir.cpp`, `runtime/` | Scalar effect chains; no unwind edges | No recoverable exceptions or inferred effect summaries | Scalar MIR tests only | PARTIAL | Typed failure/control-flow/cleanup model |
| Raw C FFI | `src/codegen.cpp`, `tools/on1x-bind.cpp` | Exact declared scalar escape hatch; generated signed/unsigned integer, boolean and binary32/64 adapters assert signature/type/size/alignment/format/range | extern; explicit parameter flattening | `test_contracts*.py`, `test_parameter_patterns.py`, `test_unsigned_f32.py` | PARTIAL | Enums, aggregates, callbacks and resources |
| Safe foreign admission | `tools/on1x-bind.cpp`, `src/{cli,safety}.cpp`, `include/on1x/safety.hpp` | `reviewed-scalar/v1`: signed integer/void; v2 adds bool/f64; v3 adds unsigned/f32. Generated C11 facade, strict target boundary run, `on1x.safe-ffi/v1` manifest and bounded versioned admitted-extern evidence | Ordinary `on1x_safe_*` functions; raw declarations remain explicit and Chisel requires exact declaration/callsite parameter/result evidence | `test_contracts_sdk.py`, `test_safety.*` | EXECUTABLE | Accountable real reviews for production bundles; pointers/resources/errors/aggregates/callbacks, Wetstone effects and broader targets |
| Boolean/binary64 foreign admission (M2 continuation) | `tools/on1x-bind.cpp`, `src/safety.cpp` | Exact C11 `_Bool`/`double` and typedefs; separate-TU C/C++ boundary, binary64 target/format/relaxed-mode guards | Typed `bool`/`f64` extern evidence, scalar Chisel lowering | `test_contracts_sdk.py`, `test_safety.*`, `tests/fixtures/contract_scalars.on1x`; Python oracle, ASan/UBSan | EXECUTABLE | Other floating formats/targets, floating environment effects and broader contract verification |
| Native kernel planning | `stdlib/mulib/`, `tools/on1x-exotract.py` | One implementation TU plus companions | Metadata, raw adapters | `test_mulib.py`, `test_exotract.py` | EXECUTABLE | Not general safe language bindings |
| MIR / reference evaluator | `src/backend/{mir,lower,serialize,evaluate,emit_c}.cpp`, `floating.hpp`, `numeric*.hpp` | Legacy v1/v2 plus complete scalar numeric v3; allowlisted descriptors, exact typed payloads and independent C++ evaluation | Source/saved numeric MIR, trivial tuple destructuring/calls/slots/phis, checked optimizations | `test_mir.*`, `test_f64.py`, `test_unsigned_f32.py`, `test_numeric_limits.py` | PARTIAL | Managed aggregates, records/ADTs/arrays, closures, ownership, cleanup and async effects |
| Trivial tuple MIR | `src/backend/{mir,lower,serialize,evaluate,emit_c}.cpp` | By-value copies; no owned/borrowed fields; bounded recursive evaluator storage | Nested bool and all admitted numeric leaves (v2/v3); shared source patterns | `test_mir.*`, `test_numeric_limits.py` | EXECUTABLE | Resource-bearing aggregates, foreign layouts, separate compilation |
| MIR passes / relational analyses | `src/backend/{passes,relational}.cpp` | Conservative v1/v2/v3 passes; no float/v3 numeric folding; Novostone v1 only | Explicit opt-in, versioned pass descriptors and numeric-v3 target | `test_passes.*`, `test_relational.*`, `test_mir.py`, `test_numeric_limits.py` | PARTIAL | v2/v3 relational mapping, full-language legality and ownership descriptors |
| C11 AOT | `src/codegen.cpp`, `runtime/` | Standalone C11 for admitted subset | Default backend | Compiler/native suite | EXECUTABLE | Broader language/runtime representation |
| Native / VM backend | `BACKEND.md` | None | None | None | DESIGNED | Real equivalent code generator |
| Async/await and native structured concurrency | `AGENTS.md`, `docs/concurrency.md` | Cooperative G4 runtime only; no async frame/native worker ABI | Taskgroup only in the initial stackful single-thread profile | `test_concurrency.{cpp,py}` tests its narrower profile | PARTIAL | Effectful MIR, async frames, cancellation/ownership and native worker isolation |
| General channels / select / actors / workers | MuLIB headers; LibStone Plaid; `src/concurrency.cpp` | Bounded i64 1:1 uni channels and atomic `poll`, plus role-checked process byte streams; no actor/worker adapter | Checked send/receive/close and bounded i64 offers only | `test_concurrency.{cpp,py}`; general gate absent | PARTIAL | General message ownership, byte/general `poll`, duplex/quota, deadlines/cancellation, actors/workers |
| Diagnostics / repair | `src/diagnostics.cpp` | Optional Syren build-time adapter | Located errors; proposed edits only | `test_diagnostics.*` | PARTIAL | Full stable diagnostic taxonomy |
| Formatter / LSP / linter / packages | `tools/` | Build helper only | None for these tools | Existing installation tests | ABSENT | Implement tools and protocol guarantees |
| Standard library | `stdlib/`, `std/` | Persistent helpers, trusted scalar bridges | Partial functional modules | `test_libraries.py`, `test_stdlib_extensions.py` | PARTIAL | Coherent data/resource model and conformance |
| Compatible first-party control-flow migration | [Source audit](source-migration.md): examples, Basis/PPX libraries, native unroll | Existing C11 and scalar/numeric tuple MIR; CSP examples use the existing ABI v4 deadline path | Ranges, three-clause `for`, value/statement `loop`, structural `match`, bounded taskgroup examples | [Verification](../VERIFICATION.md#first-party-source-migration-control-flow): both scanners, strict C11, ASan/UBSan, and output parity | EXECUTABLE | Unsupported kernel adapters unchanged; expiry is terminal, not a recoverable join result |
| Self-hosting | `stdlib/{sexpr,ppx,visitor}/` | C11 native authoring tools | Compiler remains C++20 | `test_selfhost.py` | PARTIAL | Real compiler-stage migration/bootstrap |

No row is newly certified VERIFIED by source inspection alone. Optional SDKs,
cross-target ABI and TSAN are not implied by default-build tests.
