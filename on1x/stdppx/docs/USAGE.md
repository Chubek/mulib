# On1x PPX examples, exact syntax, and constraints

The attribute/extension payload is **not** a free-form textual macro. The lexer
must first parse it as a valid `@label("operation", ...)` carrier. All
transformations output the compiler's current core AST.

| Processor | Source form | Notes |
|---|---|---|
| `identity` | no carrier required | AST wire roundtrip |
| `checks` | `@checks("assert_eq", a, b)` | `assert_ne` too; emits a global intrinsic wrapper |
| `control` | `@control("when", cond, { stmts })` | also `unless`, `while`, `until`; loop conditions are reevaluated |
| `unroll` | `@unroll("repeat", 4, { stmts })` | 0..256 inclusive, one lexical Scope per iteration |
| `derive` | `@derive("derive", "arity", "signature") fn f(...)...` | metadata only |
| `fold` | no carrier required | only safe i64 literal arithmetic; runtime traps retained |
| `pipe` | `@pipe("apply", value, f, g)` | means `g(f(value))`, only named functions |
| `compose` | `@compose("apply", value, outer, inner)` | means `outer(inner(value))` |
| `cond` | `@cond("branches", cond, { ... }, { fallback })` | statement context, final default block required |
| `tap` | `@tap("call", value, callback)` | first-class callback name (wrap intrinsics); typed-hole parameters; single evaluation |
| `assertions` | `@assertions("assert_true", cond)` | false/eq/ne and `between(value, low, high)` forms supported |
| `trace` | `@trace("entry", "label") fn f(...)...` | function entry only, no exit tracing |
| `logging` | `@logging("warn", message)` | statement context, string expression |
| `format` | `@format("string", "Hi {0}", name)` | string args only; indices ordered, unique, 0-based; escape `{{` and `}}` |
| `embed` | `@embed("text", "asset.txt")` | `ON1X_PPX_EMBED_ROOT`, confined NUL-free <=1 MiB file |
| `regex` | `@regex("prefix", text, "Hello")` | full/prefix/suffix literal operations only |
| `contract` | `@contract("requires", x >= 0) fn f(x:i64)...` | precondition only, checked at entry |
| `match_int` | `@match_int("cases", value, 0, { ... }, 1, { ... }, { default })` | one-time evaluation, i64 literals |
| `state_machine` | `@state_machine("transition", state, event, 0, 1, 2)` | i64 transitions, max 64, duplicate pairs rejected |
| `cfg` | `@cfg("enabled", "A") fn f()...` | `ON1X_PPX_FEATURES=A,B`; disabled form supported |
| `test` | `@test("case") fn test1() { ... }` | generates `on1x_run_tests()` for zero-arg void tests |
| `return_guard` | `@return_guard("nonnegative") fn f()->i64...` | checked original value exactly once before return |
| `timing` | `@timing("elapsed") fn f()->i64...` | scalar explicit return type, link supplied C11 adapter |
| `ffi_bind` | `@ffi_bind("raw_scalar") extern fn c_fn(a:i64)->i64;` | exact-reviewed metadata matched for raw scalar only |

All listed processors have compiler-generated C11 execution fixtures in
`on1x/tests/test_stdppx.py`. `@label` is On1x syntax, with normal compiler checks
after transformation.

## Native PPX mode

CMake builds the eight sources in `native/`, including `pipe.on1x` and
`compose.on1x`. Invoke their generated host executables via `--ppx LABEL EXECUTABLE`.
Native versions use the existing `stdlib/sexpr`, `stdlib/visitor` and
`stdlib/ppx` imports, and require `ON1X_PPX_SCHEMA` to point to the original
`grammar/PPX.schema.json`.

## Timing adapter

The PPX generates `extern fn on1x_ppx_clock_ns() -> i64` and
`extern fn on1x_ppx_timing_log(name:string, duration:i64)` declarations.
The build helper links `runtime/on1x_ppx_timing.c` automatically; manual C11
builds link it once with the generated program. The
adapter uses the POSIX monotonic clock when available, otherwise C11 UTC time;
negative or nonmonotonic durations may appear on fallback platforms. Timing
records are emitted to stderr.

## FFI manifest admission

`ffi_bind` uses exactly the file named by `ON1X_PPX_FFI_MANIFEST`, and checks
`ON1X_PPX_FFI_TARGET` against the manifest target. Format:

```json
{"schema":"on1x-raw-ffi/v1", "target":"x86_64-unknown-linux-gnu",
 "functions":{"my_c_function":{"parameters":["i64"], "return":"i64", "reviewed":true}}}
```

The `reviewed` flag must be explicitly granted by an independent audit; adding
it does not prove the C declaration or implementation matches the source. No
pointer, struct, callback, dynamic ownership or effects are admitted. The user
must separately link the exact C implementation and arrange its ABI verification.

## Trust and reproducibility

Never enable `--allow-source-ppx` for untrusted imports. Executable PPXs can
read files, environment variables and network independently of AST validation.
Pin PPX code and input file hashes in a reproducible build. The compiler still
validates the returned AST, and On1x's ordinary semantic, memory and type checks
must not be skipped.
