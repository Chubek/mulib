# Complete PPX catalog

| Processor | Status | What it does |
|---|---|---|
| `checks` | existing | assert_eq/assert_ne intrinsic wrappers |
| `control` | existing | when/unless/while/until statement expansion |
| `derive` | existing | function arity/signature metadata only |
| `fold` | existing | trap-preserving integer literal folding |
| `identity` | existing | AST identity and schema round-trip |
| `unroll` | existing | bounded repeat() expansion |
| `assertions` | implemented-python | assert_true/false, assert_eq/ne, single-evaluation between |
| `cfg` | implemented-python | enabled/disabled(feature) conditional function declarations |
| `compose` | implemented-python | apply(value,outer,inner,...) right-to-left unary calls |
| `cond` | implemented-python | branches(condition,Block,...,fallbackBlock) |
| `contract` | implemented-python | requires(condition) function preconditions |
| `embed` | implemented-python | text(relative_filename) compile-time resource embed |
| `ffi_bind` | implemented-python | raw_scalar metadata admission for exact simple raw C ABI |
| `format` | implemented-python | string(template,{0},{1},...) string-only formatting |
| `logging` | implemented-python | debug/info/warn/error string statements |
| `match_int` | implemented-python | cases(state,integer,Block,...,defaultBlock) |
| `pipe` | implemented-python | apply(value,f,g,...) left-to-right unary calls |
| `regex` | implemented-python | full/prefix/suffix literal string matcher; no regex metacharacters |
| `return_guard` | implemented-python | nonnegative postcondition on i64 returns |
| `state_machine` | implemented-python | transition(state,event,from,on,to,...) for i64 |
| `tap` | implemented-python | call(value,callback) with single evaluation |
| `test` | implemented-python | case() zero-arg void test runner generation |
| `timing` | implemented-python | elapsed() instrumentation of scalar functions |
| `trace` | implemented-python | entry([message]) function entry logs |
| `actor` | design-only | Actor/message-processing syntax |
| `async` | design-only | Async/await syntax lowering |
| `derive_arbitrary` | design-only | Property-testing generator |
| `derive_binary` | design-only | Binary codec and versioned schemas |
| `derive_builder` | design-only | Named-field builders |
| `derive_clone` | design-only | Safe resource cloning |
| `derive_debug` | design-only | Debug-formatted structured values |
| `derive_eq` | design-only | Structural equality derivation |
| `derive_fold` | design-only | Typed catamorphism and recursion schemes |
| `derive_hash` | design-only | Stable hashing derivation |
| `derive_json` | design-only | JSON encoders and decoders |
| `derive_lens` | design-only | Record-field lenses |
| `derive_ord` | design-only | Total ordering derivation |
| `derive_show` | design-only | Readable structured formatting |
| `derive_visitor` | design-only | Per-ADT visitor dispatch |
| `ffi_safe` | design-only | Verified ownership-safe MuLIB wrappers |
| `letops` | design-only | Monadic/applicative binding expansion |

`implemented-python` processors have protocol and compiler-generated C11 execution tests. See `VERIFICATION.md` for measured configurations. `design-only` means documentation exists but no processor executable is shipped.
