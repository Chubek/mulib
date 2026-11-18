# On1x standard PPXs

An additional **native On1x Category processor** is now built for opt-in
`@enable Protocols` / `--enable Protocols`. It is separate from the ordinary v1
processor catalog below: `--stdppx` defaults to the shipped Python files but can
discover explicitly selected native bundles through configured paths. Its grammar and
lowering live in On1x libraries, not Python/C++. See [Category](../docs/category.md)
for the checked local scalar profile, native typed feedback and independent
host admission, trusted activation, and remaining generic/functor stages.

**24 Python processors and 17 native On1x processors** implement bounded,
schema-v1 source transformations. CMake builds/installs all native executables;
the compiler always runs its normal inference, type, staging and ownership checks
after reconstruction. This is not a typed deriving or safe-FFI framework.

[`LuaEmbed`](luaembed/README.md) is native-only: it runs bounded, restricted
Mune Lua at compile time, captures stdout into normal runtime stream calls or
strings, and validates generated S-expression expressions, statement streams
and programs. Its embedded Lua module `llembed` supplies AST constructors,
escaping, fresh names and bounded metaprogram chaining. Activation stays explicit.

[`GrammarEmbed`](grammarembed/README.md) is native-only and uses LibStone
`firestone/liblr`: Perl 6-like `grammar`/`token`/`rule` constructs emit NFA/DFA
lexers and LR(1)/LALR(1)/LL(1) parsers with token spans and concrete parse trees.

The M1 numeric increments add `F64Type` and unsigned decimal `FloatLiteral`
spelling to the canonical and standalone v1 schemas, and f64 to native/Python
signature rendering. All existing constructors and
protocol framing are unchanged. The unsigned/binary32 extension also adds
`U8Type`, `U16Type`, `U32Type`, `U64Type` and `F32Type` to both schemas and
native/Python signature rendering, without changing literal token syntax. Existing
processors must be rebuilt with the new schema;
older schema-pinned processors may reject the new constructors. `FloatLiteral`
is a `NumericLiteral`, not an `IntegerLiteral`; signs use `Unary`, and integer
folders must leave floats untouched. See `../docs/numerics.md` for grammar and
limits. This additive migration does not add float folding or safe FFI metadata.
The literal continuation adds suffixed decimal f32/f64 spellings and a separate
`UnsignedLiteral`/`UnsignedSpelling` constructor under `NumericLiteral`, not the
legacy i64-only `IntegerLiteral` category. Canonical, standalone and native
validators preserve spelling/location and bound it to 1,024 bytes. Existing
integer folders leave these typed literals unchanged; old schema-pinned tools
must reject unknown constructors rather than reinterpret their values.

```sh
cmake --build build/on1x --parallel 4
build/on1x/on1xc --emit-c client.on1x --stdppx assertions -o client.c
# Use the equivalent native processor explicitly:
build/on1x/on1xc --emit-c client.on1x \
  --ppx assertions build/on1x/stdppx/on1x-ppx-assertions -o client.c
```

`--stdppx NAME` resolves the Python implementation beside the standard library.
`--ppx LABEL PROGRAM` accepts native executables too; LABEL must match the source
carrier. For standalone native protocol runs, set `ON1X_PPX_SCHEMA` to the absolute
`grammar/PPX.schema.json` path, `ON1X_PPX_PROTOCOL=1`, and `ON1X_PPX_LABEL=LABEL`.
Source `@ppx` registrations require `--allow-source-ppx`.

## Implemented native/Python operations

All seventeen ordinary native processors import the original `stdlib/ppx` authoring library.
`native/common.on1x` is a shared helper, not an executable.

| Processor | Source API |
|---|---|
| `identity` | Wire/schema roundtrip |
| `checks` | `@checks("assert_eq", a, b)` / `assert_ne` |
| `control` | `when`, `unless`, `while`, `until(condition, {body})` statements |
| `unroll` | `repeat(integer, {body})`, count 0..256 and expansion budget |
| `derive` | Function attribute `derive("arity", "signature")`; metadata only |
| `fold` | Trap-preserving literal `i64` arithmetic folding |
| `pipe` | `apply(value, f, g, ...)`, left-to-right calls |
| `compose` | `apply(value, outer, inner, ...)`, right-to-left calls |
| `assertions` | `assert_true`, `assert_false`, `assert_eq`, `assert_ne`, `between(value,low,high)` |
| `cond` | `branches(condition,{body},...,{fallback})`; compatibility `choose(condition,{yes},{no})` |
| `format` | `string("{0}+{1}={{literal braces}}", a, b)`; compatibility `concat(a,b)` |
| `logging` | `debug`, `info`, `warn`, `error(string)` to stderr; compatibility `print(value)` to stdout |
| `state_machine` | `transition(state,event,from,on,to,...)`; compatibility statement `set(identifier,value)` |
| `tap` | `call(value,callback_identifier)`; compatibility `identity(value)` |
| `trace` | Function attribute `entry` with zero or one literal message; stderr `TRACE message` |
| `LuaEmbed` | Native-only: bounded Mune Lua `stdout`, `text`, `expr`, `statements`, and `program`; embedded `llembed` helpers and finite chaining |
| `GrammarEmbed` | Native-only: liblr-generated NFA/DFA lexers and LR1/LALR1/LL1 parsers for embedded `grammar` definitions |

`tap` evaluates the value then the callback exactly once, invokes the callback,
and returns the value. A mutable callback is passed as an argument rather than
captured. Assertion operands evaluate once in source order, and introduced names
are collision-checked; a local `assert` cannot hijack the generated assertion.
`between` is inclusive and i64-only. Format arguments are strings, occur once,
and must use consecutive `{0}`, `{1}`, ... slots in source order. State transitions
use at most 64 unique i64 `(from,event)` pairs; unmatched inputs preserve state.
Conditions short-circuit, branch blocks retain lexical scope, and tracing never
pretends to instrument exits. Malformed/unconsumed carriers fail with no stdout.

## Other Python processors

- `embed`: explicit compile-time confined text reads, authorized with
  `ON1X_PPX_EMBED_ROOT`; no absolute/escaping paths, NUL or files above 1 MiB.
- `regex`: **literal** full/prefix/suffix matching, not a regular-expression engine.
- `contract`: checked `requires(condition)` function preconditions.
- `match_int`: single-evaluation literal dispatch with a default block.
- `cfg`: enabled/disabled function declarations using `ON1X_PPX_FEATURES`.
- `test`: explicit zero-argument void test-case runner generation.
- `return_guard`: nonnegative explicit i64 return postconditions.
- `timing`: scalar function timing; the build helper links
  `runtime/on1x_ppx_timing.c` automatically. Early returns are instrumented once.
- `ffi_bind`: validates explicit **raw** scalar metadata from
  `ON1X_PPX_FFI_MANIFEST` against `ON1X_PPX_FFI_TARGET`. Duplicate/unknown fields,
  mismatched signatures/targets and absent review flags reject. The metadata's
  review boolean is a caller claim, **not** independent C implementation review,
  CDecl discovery, target layout probing, or safe-import admission.

The [catalog](docs/CATALOG.md) lists all operations and seventeen **design-only**
processors. Records/ADTs, typed derivation, async and `ffi_safe` remain unavailable.
No C/C++ algorithm or semantic-contract parser is invented by these processors.

## Protocol, installation and tests

Canonical v1 schema: [`../grammar/PPX.schema.json`](../grammar/PPX.schema.json).
Python support bounds input/output to 4 MiB, depth to 256, and syntax nodes to
262,144. Native processors additionally use bounded managed S-expression and
snapshot storage. The compiler imposes subprocess timeout/output limits.
Processor authorization is not an OS sandbox.

```sh
python3 -m unittest discover -s on1x/stdppx/tests -v
ctest --test-dir build/on1x -R 'stdppx|selfhost|ppx' --output-on-failure
python3 on1x/stdppx/make_catalog.py
```

Catalog regeneration preserves design documents unless `--refresh-designs` is
explicitly supplied. Native programs install under `libexec/on1x`; Python sources,
schemas and authoring resources install under `share/on1x`. Tests execute emitted
C11 with GCC, Clang and ASan/UBSan, compare native/Python behavior, and reject bad
protocol input, unconsumed carriers, collisions and post-PPX type errors.
See [`VERIFICATION.md`](VERIFICATION.md) for measured results and historical scope.
## Discovery metadata

All shipped script processors have a terse version-1 manifest on the second line
after their shebang. Native builds/installations include a declarative
`On1xfile` processor bundle. Select processors by name with `--use-ppx NAME` or
`--stdppx NAME` and `ON1X_PPX_PATH`/`ON1X_STDPPX_PATH` (matching CLI flags exist).
Discovery does not authorize execution or alter the pinned Category registry.
See [the complete search/manifest contract](../docs/search-paths.md).
