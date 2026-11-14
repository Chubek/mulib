# On1x P0 / P1 verification report — 2026-11-14

## Environment and dependency identity

- Linux x86_64, glibc 2.44; compiler target `x86_64-pc-linux-gnu`.
- GCC/G++ 16.2.1 (20260810); Clang/Clang++ 22.1.8.
- CMake 4.4.3, Python 3.14.7, Perl 5.42.2.
- Supplied LibStone checkout: `on1x/third_party/libstone`, commit
  `aafc6994e9322c9a46ac8f99854c00a8a37a3d5a`; `asdl.pl` and `aurocks.pl` 2.0.
  Only read/generator invocations were performed; the checkout was not changed.
- Canonical MuLIB kernels came from this repository's parent `include/`, not
  LibStone's vendored MuLIB snapshot.

## Executed checks

| Configuration | Result |
|---|---|
| GCC generated ASDL/Aurocks frontend (fresh closure build) | 11/11 CTest tests PASS |
| GCC independent fallback frontend | 10/10 PASS |
| Clang generated ASDL/Aurocks frontend | 11/11 PASS |
| Clang independent fallback frontend | 10/10 PASS |
| Clang ASan+UBSan generated frontend and generated C11 programs | 11/11 PASS |
| Clang ASan+UBSan independent fallback and generated C11 programs | 10/10 PASS |
| Standalone GCC fallback without LibStone or parent MuLIB headers | 9 PASS; JSON SKIP |

Compiler builds and emitted C11 programs use
`-Wall -Wextra -Wpedantic -Werror`. Sanitized configurations additionally use
`-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all`,
including the real MuLIB kernel implementation TUs. No test was skipped in the
six parent-MuLIB configurations. A separately staged standalone source copy
verified that absent canonical headers produce an explicit CTest JSON SKIP.

Generator builds visibly generated and compiled `on1x_ast.c` and
`on1x_parser.c`. The differential corpus compares **244** exact token/error
cases, including byte offsets, invalid strings, comments, NUL and source limits.
Scanner API tests cover non-terminated bounded buffers, copied arena-owned
tokens, repeated scans after failures and serialized concurrent calls.

The compiler suite includes 25 negative source cases, eight checked arithmetic
traps and native precedence/evaluation-order tests. Separate input-limit tests
reject 20,000-level blocks, parentheses, unary expressions, binary chains and
calls without crashing, now including long postfix-call chains and nested
function types. The original deeply nested block input crashed before the
parsing/tree-depth guards.

### Initial P1: first-class monomorphic functions

`on1x.types` checks structural monotype equality/ordering and owned signature
storage. `on1x.functions` compiles and runs eleven programs, with 30 semantic
rejections separated from three syntax rejections and an argument-location
regression. The same function suite also passes with `-O2` under GCC and Clang.

Coverage includes mutable function aliases, nested higher-order parameters and
returns, zero-argument/void/string/bool signatures, recursion, lexical shadowing,
imports, callee-before-argument evaluation, short circuiting, raw-call wrappers,
typedef/extern name collisions and lexical JSON cleanup through indirect calls.
Function-valued extern ABIs, raw extern values, intrinsic values and nested
resource-bearing signatures are rejected, not silently adapted. These regression
programs now use the closure-capable representation described below, while
top-level function values still need no environment allocation.

### P1 continuation: bounded monomorphic inference

`on1x.inference` compiles/runs twelve programs and separates 33 semantic
rejections from two syntax rejections. It covers omitted parameter annotations,
explicit/nested `_` holes, inferred returns, higher-order constraints, mutual
recursion, forward references, imports, lexical shadowing and JSON cleanup.
The suite also passes with `-O2` under GCC and Clang. `--types` output is checked
for exact concrete signatures, explicit raw-extern labels and deterministic
file/stdout behavior.

`on1x.inference_engine` checks idempotent admission of grounded HIR, absence of
escaped holes, occurs-check rejection, inclusive budget boundaries and
transactional failure under lowered node/work/depth/expanded-size budgets.
Separate source cases reject 300-level inferred type chains and exponential
type growth before recursive semantic/backend traversal. A finite shared type
graph still compiles natively, and long function names do not amplify per-
parameter diagnostic metadata.

All types are grounded before the ordinary semantic/resource/ABI checks run.
Tests reject inferred resource/view escapes, aliases, invalid intrinsic domains,
void bindings/parameters, missing returns and raw-function escapes. Extern
signatures must remain concrete. No type schemes, generalization, guessed
defaults, effect inference or foreign safety proof is claimed.

### P1 continuation: immutable lexical closures

`on1x.closures` compiles/runs twenty native programs, separating 29 semantic
rejections from four syntax rejections. Coverage includes anonymous functions,
escaping scalar/string/function captures, nested/transitive captures, function-
valued returns, aliases/self-assignment, shadowing, snapshots, local mutation,
top-level recursion, imports, raw calls, JSON-local cleanup and generated-name
collisions. Allocator hooks verify zero live environments at normal exit and
precise loop/condition/discard cleanup. Nested short-circuit operands preserve
the lifetime of previously evaluated callees and arguments.

The suite releases a 50,000-environment generated capture chain without
invoking it recursively. An isolated C11 runtime test also checks a 100,000-
environment destruction chain, null operations, reference initialization,
overflow/underflow and invalid allocation sizes. Failure injection on the
first/second capture allocation verifies fatal status 72; these errors do not
promise unwinding. The function, inference and closure suites all pass with
`-O2` under GCC and Clang; closures additionally pass with `-O2` plus Clang
ASan/UBSan on emitted C and the runtime.

`on1x.closure_hir` verifies concrete inferred lambda types, deduplicated first-
use/transitive captures, shadowed bindings, idempotent checking/emission and
transactional inference failure. Input-limit tests exercise lambda-body depth,
the inclusive 2,048-lambda / 16,384-capture boundaries, and the 2 MiB copied-name
budget against transitive long-name amplification. Scanner token grammar needs
no new tokens; both real generators remain in the verified frontend path.

Function values now use exact typed invocation pointers plus optional
reference-counted environments. `ON1X_CLOSURE_ABI_VERSION` guards the internal
ABI, and a deliberately incompatible runtime header fails to compile. Emitted
C and runtime must be rebuilt together; no stable extension or foreign callback
ABI is claimed. Mutable/JSON captures, recursive `let`, polymorphism, concurrent
reference counting and general foreign lifetime/effect analysis remain absent.
See [closure ownership/runtime manual](docs/closures.md).

The **separately registered** `on1x.json` test exercises the real MuLIB example,
nested document lifetimes, loop/return cleanup, i64 conversions, malformed
input, missing members/null values, and parse/stringify allocation failures. The old
CTest registration omitted this test entirely; its stale example expectation
has also been corrected. Fatal `or_die` error paths still terminate rather than
providing general resource unwinding. The suite now also compiles/runs an
inferred string-to-size function and a string-capturing closure with locally
owned JSON and borrowed child locals
against the real canonical kernel, under the same sanitizer flags.

Independently of CTest, `tools/on1x-build.py` built and ran `examples/hello.on1x`,
`examples/json.on1x` and `examples/higher_order.on1x` using both GCC and Clang.
Results were `21`, `3` followed by `{"items":[1,2,3],"ok":true}`, and the lines
`41, 42, 14, 10, 18, 12, 21`, respectively. `ldd` showed no LibStone, scanner or
C++ runtime linkage. This verifies the source-tree native build workflow, not
a system-wide installation/package.

The same independent native-helper check also built and ran
`examples/inference.on1x` with both compilers: output `42`, `true`, `42`, with
no LibStone/scanner/C++ runtime linkage. This increment adds no application
dependency beyond the small C11 runtime.

`examples/closures.on1x` was likewise built/run with both native compilers:
output `42`, `42`, `hello from a closure`. All five native-helper examples were
rebuilt after the closure ABI change; no LibStone/scanner/C++ runtime linkage
was present. Environment allocation uses only the C runtime, not a vendored
allocator or foreign closure library.

## Reproduction

From the MuLIB root, using the supplied non-default LibStone location:

```sh
cmake -S on1x -B /tmp/opencode/on1x-closures-gcc-generated \
  -DON1X_REQUIRE_LIBSTONE=ON -DBUILD_TESTING=ON \
  -DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone" \
  -DCMAKE_C_FLAGS='-Wall -Wextra -Wpedantic -Werror' \
  -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror'
cmake --build /tmp/opencode/on1x-closures-gcc-generated --parallel 4
ctest --test-dir /tmp/opencode/on1x-closures-gcc-generated --output-on-failure
python3 on1x/tools/on1x-build.py on1x/examples/hello.on1x \
  -o /tmp/opencode/on1x-hello --on1xc /tmp/opencode/on1x-closures-gcc-generated/on1xc
/tmp/opencode/on1x-hello
python3 on1x/tools/on1x-build.py on1x/examples/json.on1x \
  -o /tmp/opencode/on1x-json --on1xc /tmp/opencode/on1x-closures-gcc-generated/on1xc
/tmp/opencode/on1x-json
python3 on1x/tools/on1x-build.py on1x/examples/higher_order.on1x \
  -o /tmp/opencode/on1x-higher-order --on1xc /tmp/opencode/on1x-closures-gcc-generated/on1xc
/tmp/opencode/on1x-higher-order
python3 on1x/tools/on1x-build.py on1x/examples/inference.on1x \
  -o /tmp/opencode/on1x-inference --on1xc /tmp/opencode/on1x-closures-gcc-generated/on1xc
/tmp/opencode/on1x-inference
python3 on1x/tools/on1x-build.py on1x/examples/closures.on1x \
  -o /tmp/opencode/on1x-closures --on1xc /tmp/opencode/on1x-closures-gcc-generated/on1xc
/tmp/opencode/on1x-closures
/tmp/opencode/on1x-closures-gcc-generated/on1xc --types on1x/examples/closures.on1x
```

Use a separate directory with `-DON1X_USE_FALLBACK_SCANNER=ON`, omitting
`ON1X_REQUIRE_LIBSTONE`, for independent fallback testing. The sanitizer
configuration and generated-program flags are documented in
[tests/README.md](tests/README.md).

## Selected SHA-256 fingerprints

These identify the tested generator/grammar/header inputs, not a complete
foreign-contract or transitive-header inventory:

```text
1d04c59d3c8f90565eefa0d661a9d4650ac56e8968eedba3e12fd1fadab4d0c1  scripts/asdl.pl
882d5c1ed69e46ef84488c86485bbec47f1237878d47c4bb58d1d039065f6396  scripts/aurocks.pl
453a2855098c50545220de54ef053a65f485d6c70e4fc6d5aed4c83961b47098  grammar/On1x.asdl
57e7364ad677342e1ea6d4b4ba3620520f11f1eb5a5010214536810cd2ae9c58  grammar/On1x.g
e9e233759cfa04eed6f5d05e98ca619118c42b6dc71e1f94a1d0aa882a5585e0  ../include/mulib-json.h
```

Generated artifacts were byte-identical between independent GCC and Clang
sanitizer build directories:

```text
311c93a4e3972e01bc551b43b2d99adc22aab27091ded9a8500256e80cebd455  on1x_ast.h
7cde3b01f5cf68d82d4c6452587828394c3af9e739985a90a54b257bbcecd958  on1x_ast.c
d0015efecb388d31546ec471f2aa3725555e07052772088e96b8cda754b4da6e  on1x_parser.c
```

Selected P1 inputs and emitted programs:

```text
8c4740169766c07b611c66339e2c5e6b09f674e88bc85f671580394ea329f6df  include/on1x/type.hpp
bc81baa8746f7908fdf46266aea6515a539e05b190aca6c52f8cf4d5da0f1370  include/on1x/ast.hpp
fd1aaadbc4d38294a1454f196881a2b580b760852f871cbfc78ac08f33cea86c  src/type.cpp
aa003b2fc771caed4c8cd079284306221bf9901a373bd8b431d72d28afe37c08  include/on1x/infer.hpp
a9f971def32199ba26e52726ed70099f79dc6a56fef3335d2ecd87d6ab0d798c  src/infer.cpp
7d4a03fde799299ce2fcfcd0ffc5b570da4fd85a1055da7f8ab611d6cd51d707  src/parser.cpp
cead68e65e8d40bae2d6e9f916e05f9c4d8fc7f0c27b6d4d4411df139c7666a7  src/sema.cpp
682f2ab0c190a67d446f5a71820bd37de549c69578413c3663a1d79a3d1fa706  src/codegen.cpp
c9fdfc3637ffd16fc82ccf144d25a53f742b3f4c3925a12ec275d9fa0725c757  runtime/on1x_runtime.h
7e32987339cbd50d57797e387c7af8ba1bb15fa4bcbfa38ac395d84056393ceb  runtime/on1x_runtime.c
a257238b5ff05f364205540a7754e0c8c810bdd8caab83c99eb678312ccb8478  examples/higher_order.on1x
6dc205b1df541a06025fcb35ad5fffa44d952b3812c60a8b8dcc990bc4c2dbdb  examples/inference.on1x
53233cff843337b87de0c2f4450b55c65288f95155d4f8fd3e2289fe189e8f87  examples/closures.on1x
a4ec4688c18118f2481c3d22c3d3c369b14fc76f1246a65391f396fc5139d9f3  higher_order.c
c996e40b67c1e00d62db4f298934318ffdcc63dd030e10a2affb74b6513d1986  inference.c
03927004e98876c7b4e32f3cbd15af98ea5737520673e1646b4ad49d6302f1c9  closures.c
017d87b57840e3bc055e213891752d2c8015b76192781890553c710968509d14  types.txt
```

All three emitted programs and the `--types` test artifact were byte-identical
across all six integrated configurations, including generated/fallback scanners
and GCC/Clang compiler builds. They are test artifacts, not checked-in generated
sources.

## Not verified / not claimed

The optional ExoML-linked `on1x-bind` target and full contract/ABI gates were
not exercised by this P0/P1 iteration. Cross-target behavior, system
packaging, type schemes/let-generalization, full Hindley–Milner, mutable/resource
captures, recursive let, concurrent closures, ADTs,
safe FFI, Chisel/Wetstone analysis, general error cleanup and native/VM compiler
backends remain future work.
