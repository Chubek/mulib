# LuaEmbed and `llembed`

LuaEmbed is an **On1x-authored native PPX**, using the existing PPX, visitor,
S-expression and schema libraries. A trusted C11 build-host bridge executes Lua
with canonical [`mulib-mune.h`](../../../kernels/mulib-mune.h). It is not a
second On1x parser, a Python processor, stock Lua, or a safe-FFI admission.

## Build and activate

```sh
cmake -S on1x -B build/on1x -DON1X_BUILD_NATIVE_PPX=ON
cmake --build build/on1x --parallel 4
python3 on1x/tools/on1x-build.py on1x/stdppx/examples/luaembed.on1x \
  --on1xc build/on1x/on1xc --stdppx-path build/on1x/stdppx \
  --use-ppx LuaEmbed -o build/luaembed-demo
build/luaembed-demo
```

The exact, case-sensitive name is `LuaEmbed`. Explicit-path activation also works:
`on1xc --emit-c INPUT --ppx LuaEmbed BUILD/stdppx/on1x-ppx-LuaEmbed`.
The installed `On1xfile` advertises the executable and its canonical schema.
Standalone protocol clients must set `ON1X_PPX_SCHEMA`, `ON1X_PPX_PROTOCOL=1` and
`ON1X_PPX_LABEL=LuaEmbed`. Source registrations still need `--allow-source-ppx`.
The Lua library and JSON message catalog are embedded during the build; a
relocated processor needs only its ordinary PPX schema file.

## Semantics

The syntax is the existing `@label("operation", arguments...)` carrier. Every
operation takes exactly **one literal Lua source string**, with ordinary On1x
string escaping; there is no new raw-Lua lexer or brace grammar.

```on1x
fn main() {
  @LuaEmbed("stdout", "for i=1,3 do print(i) end");
  println(@LuaEmbed("expr", "llembed.emit(llembed.integer(6*7))"));
}
```

Lua runs **at compile time** in a fresh state per carrier. Its `print`, `io.write`,
`io.stdout:write`, `llembed.write` and `llembed.emit` write into a bounded capture,
never directly onto the PPX process's protocol stdout.

| Operation | Capture interpretation |
|---|---|
| `stdout` | Generate a normal On1x `io_stdout` call containing the exact captured bytes. No extra newline is added. |
| `text` | Generate a string literal containing the exact capture. |
| `expr` | Consume exactly one complete S-expression and validate it as `Expression`. |
| `statements` | Consume a whitespace-separated stream of complete `Statement` S-expressions, validating every item; generate a lexical scope. Statement position only. |
| `program` | Function attribute only: consume exactly one complete `Unit`, replacing the empty, parameterless marker function with its declarations. Other attributes/body statements on that marker reject. |

A `stdout` call executes at its original runtime position: branches, loops,
function calls and stream ordering behave normally. Lua itself does not rerun
when that function is called. A fresh global wrapper prevents caller-local
`io_stdout` shadowing. Applications contain only ordinary generated On1x/C11;
they do **not** link Mune, Lua, LibStone or the compiler.

```on1x
@LuaEmbed("program",
  "local L=llembed; L.emit(L.unit({L.function_('main',L.type_('VoidType'),{L.do_(L.call('println',{L.integer(42)}))})}))")
fn generated_program() {}
```

Generated declarations are checked against preexisting value/type namespaces,
then the compiler's normal inference, staging, resource and ABI checks run.
Schema validity is **not semantic validity** or an ownership proof.

## Chaining

An `expr`, `statements` or `program` result may contain more LuaEmbed carriers.
They are consumed on subsequent bounded passes. For example:

```on1x
fn main() {
  println(@LuaEmbed("expr",
    "llembed.emit(llembed.extension('expr','llembed.emit(llembed.integer(42))'))"));
}
```

Generated carriers for **other already-registered** labels pass to those
processors in ordinary explicit CLI/source registration order. Place the
consumer after LuaEmbed. Output cannot authorize new processes, add imports,
install macros, or make the compiler rerun an earlier processor. No shell or
recursive compiler invocation occurs.

## Lua module: `llembed`

The table is available as `llembed` and through the restricted
`require("llembed")`. Helpers return canonical S-expression **strings**:

| API | Purpose |
|---|---|
| `location`, `label`, `input`, `version` | Invocation `Loc` text, registered owner, read-only pre-pass `Unit` text and API revision `1`. |
| `quote(string)`, `symbol(string)` | Byte-preserving ASCII wire escaping / bounded symbol validation. NUL is rejected, not truncated. |
| `list(sequence)`, `node(tag, fields)` | Dense sequence and tagged-node construction. Final schema validation remains mandatory. |
| `integer(i64)`, `string(text)`, `boolean(bool)`, `ident(name)` | Located leaf constructors. Integers must be actual Lua integers, not rounded floats. |
| `call(name,args)`, `do_(expr)`, `return_(expr)`, `block(statements)` | Located calls and statements. |
| `type_(tag)`, `function_(name,result_type,statements)`, `unit(declarations)` | Type tags, zero-parameter function and program construction. |
| `extension(operation,source[,owner])` | A located carrier for chaining; owner defaults to this invocation's label. |
| `fresh()` | Bounded unique identifier, checked against all quoted names in the pre-pass tree, unique across this processor request's evaluations. |
| `write(strings...)`, `emit(sexpr_text)` | Capture exact strings / one string plus LF. |

`node` permits the full canonical schema, not just these convenience forms.
Use `fresh()` for introduced binders; arbitrary raw AST strings do not
automatically gain hygienic binding identities. Intentional references to caller
names remain intentional. `statements` adds a scope so generated locals do not
accidentally leak into the caller.

## Limits and trust boundary

- Per Lua source: 64 KiB; parser: 16,384 tokens, 32,768 nodes, 8,192 semantic
  records and 500,000 work units.
- Per state: 16 MiB tracked runtime heap, 500,000 execution steps, 64 calls,
  4,096 values per result list. Mune also bounds parser depth. AST/compiler
  storage has separate kernel budgets; heap size is not a whole-process RSS cap.
- Per capture: 512 KiB, no NUL. Output-limit/invalid native write attempts poison
  the transaction even if Lua catches the callback error with `pcall`.
- Per PPX request: 64 evaluations, 1 MiB aggregate Lua source, 4 MiB aggregate
  capture and 16 expansion passes; canonical 256-level/262,144-node AST bounds.
  The compiler's existing subprocess deadline (default 5 seconds) also applies.
- Failed execution/validation publishes no protocol stdout or partial C file.
  Captured output is discarded. Runtime Lua errors remain catchable by Lua;
  this does not remove any work/heap/output limits.
- No `os`, filesystem/network/process I/O, dynamic loaders, `package`, external
  `require`, `debug`, `load`, `loadfile`, `dofile`, coroutines, random seed/state,
  warnings or GC controls are exposed. `io` is a new capture-only table.
  Pure base/string/table/math/UTF-8 facilities remain available.

This is an explicit, restricted **trusted PPX**, not an OS security sandbox or a
guarantee against Mune/native bugs. Only authorize trusted processor binaries
and SDKs. Unsupported platforms/targets are not certified; tested profile is
Linux x86-64 with GCC/Clang and both On1x scanners.
Stable scalar/string computations reproduce their captures; Lua operations
that expose object identity (for example `tostring` on a table or function) are
not promised to produce reproducible bytes. Use the explicit AST constructors
and fresh-name helper for reproducible generation.

`runtime.[ch]` is the only raw scalar bridge: C pointers and Mune objects do not
escape; input strings are borrowed for one synchronous call; output/error views
last until the next evaluation and are copied into managed S-expressions before
then. States/results/chunks are always destroyed. Mune's C callback ABI stays
inside this trusted C11 code, not an invented On1x foreign-callback ABI.
`generate.py` uses the existing MuLIB planner for exactly one implementation TU,
and embeds `llembed.lua` plus the validated `messages.json` catalog. No
implementation switch is defined in `runtime.c`.

Tests: `ctest --test-dir BUILD -R 'on1x.luaembed' --output-on-failure`.
The integration suite executes actual compiled On1x programs, AST/meta chains,
later-PPX composition, malformed protocols, semantic errors, recursion/work/
memory/output limits, NUL handling, name collisions and reproducible output.
Linux allocation tests inject failure at 128 deterministic points spanning the
full Mune parse/state/library/execution lifetime, accepting only complete
success or empty captured output with an error. Sanitizers check these paths.
