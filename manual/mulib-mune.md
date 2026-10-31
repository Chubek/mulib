# mulib-mune

Mune is an independent Lua implementation targeting Lua 5.4. It provides an
owned syntax-tree frontend, lexical analysis, a garbage-collected runtime,
embedded standard libraries, tree/VM/JIT/C-AOT execution, and module bundles.
Its implementation is original C11; no upstream Lua sources or Lua runtime
library are embedded. The original numeric API remains available separately.

Define these switches explicitly in one translation unit before including
`mulib-mune.h`:

```c
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-mune.h"
```

On Linux link with `-lm -pthread -ldl`. Coroutines use C11 threads, with a
condition-variable handoff ensuring only one thread executes a state at a time.
Hosts without C11 threads can use the other runtime facilities; coroutine
creation reports an unsupported-platform error. Native JIT currently targets
Linux AMD64 SysV. The normal MuLIB implementation-placement rules apply.

## Execute Lua

```c
const char source[] =
    "local function sum(...)\n"
    "  local total = 0\n"
    "  for _, value in ipairs({...}) do total = total + value end\n"
    "  return total\n"
    "end\n"
    "return sum(10, 20, 12)\n";
mulib_mune_state_t *state = mulib_mune_state_create(NULL);
mulib_mune_result_t *result = NULL;
mulib_mune_error_t error;
if (state) {
    if (mulib_mune_run_source(state, source, sizeof(source)-1,
                              &result, &error) == MULIB_MUNE_OK) {
        mulib_mune_value_t value = mulib_mune_result_get(result, 0);
        /* value.type == MULIB_MUNE_INTEGER; value.as.integer == 42 */
        (void)value;
    }
    mulib_mune_result_destroy(result);
    mulib_mune_state_destroy(state);
}
```

Use adjacent C string literals for multiline inline source, `mulib_mune_run_file`
for `.lua` files, or `mulib_mune_parse` followed by repeated `mulib_mune_run`
calls to reuse a checked chunk. Runtime errors retain earlier Lua side effects.
Execution results preserve nil values and multiple returns. `*result` must be
NULL on entry; failure leaves it NULL. A returned result list is owned by the
caller and is a GC root until destroyed. Destroy all results before their state.

The runtime implements lexical closures and shared upvalue cells, varargs,
multiple assignment, proper tail calls, numeric and generic iteration, scoped
goto/break, `<const>` and `<close>`, metamethods, integer wrapping and floating
arithmetic, tables, protected calls, coroutines, userdata and finalization.
Strings are length-delimited byte sequences. Integers are signed 64-bit;
floating values are C `double`. Metamethod calls share the ordinary call path.
`xpcall` invokes its handler before lexical cleanup. Closures retain their
checked chunk and only the outer bindings needed by their bodies/nested functions.

### Execution modes and compilation

`mulib_mune_set_mode(state, mode)` selects subsequent execution:

| Mode | Implementation |
| --- | --- |
| `MULIB_MUNE_TREE` | AST evaluation; default |
| `MULIB_MUNE_VM` | Typed register/control-flow MIR executed through VMWeave |
| `MULIB_MUNE_JIT` | MIR compiled through JIT into native control flow and runtime-helper calls |
| `MULIB_MUNE_AOT` | Generated C entries produced by MIR's C emitter |

The compiler lowers expressions, calls, indexing, constructors, short-circuit
operations, conditionals, while/repeat loops, statement sequencing and goto
dispatch. Dynamic values use rooted runtime slots and typed host operations.
Statically integral arithmetic lowers to MIR wrapping/bitwise instructions.
VMWeave performs constant propagation/folding and bounded e-graph/rewrite
identity optimization. Scope management, function calls, multiple results,
generic/numeric-for iteration and metamethods share runtime helpers across modes.
JIT uses writable-then-executable memory; unsupported native targets return an
error when selecting JIT.

`mulib_mune_emit_c(chunk, "mulib_generated_app", writer, user, &error)` emits
a translation unit containing source, typed MIR images, native C entry points
and `mulib_generated_app(state, &result, &error)`. The writer returns nonzero
on success. Compile this output with a MuLIB implementation translation unit
and the platform link flags above. The generated wrapper checks/reconstructs
the source AST and uses the emitted entries through `mulib_mune_run_aot`.
Generated entry arrays must remain alive while their closures exist. Lua
loaded dynamically during AOT execution uses the VM. Compiler caches belong to
the chunk and are released after its last retaining closure is collected.

### Embedded standard library

State creation installs the base functions and `math`, `string`, `table`,
`utf8`, `coroutine`, `package`, `io`, `os`, and `debug` tables. This includes Lua
patterns/captures/frontiers/balanced matches, gsub/gmatch, binary packing,
function dump/load, reader-based loading, file/pipe I/O, process/date/locale
facilities, registry/uservalues, upvalue manipulation, locals, hooks and traces.
System facilities use the host C/OS APIs. Native module entry points use the
Mune callback ABI described below. Debug frame descriptions, temporary-local
visibility, instruction-hook counts, trace wording and collector scheduling
reflect this implementation rather than a particular upstream VM's internals.

The collector is tracing mark/sweep with weak keys/values, ephemeron convergence,
resurrection and `__gc`. It runs at statement boundaries or on explicit request.
Collector modes/tuning are API-level scheduling settings for this collector.
Abandoned coroutines release their suspended native stacks without executing
`<close>` callbacks; explicit `coroutine.close` performs lexical cleanup.

### C integration and ownership

`mulib_mune_bind` installs a native function. Its arguments and result builder
are borrowed for the call; append return values with `mulib_mune_result_push`.
Return a Mune status. `mulib_mune_call` invokes Lua/native functions from C,
including from a native callback. Globals, raw table access, metatables,
strings and zero-initialized userdata have corresponding public APIs.
Userdata memory belongs to the state; `debug.getuservalue/setuservalue` access
its indexed Lua values. The callback ABI is independent of `lua_State`.

Object values borrow state storage. Keep retained objects in a global/table or
an owned `mulib_mune_result_create` root across calls and collections. Mixing
objects from different states is rejected. Scalar values are copied. An object
pointer retained after its root/state is destroyed is invalid. A state requires
external synchronization; native callbacks execute on the current coroutine's
thread. State destruction must occur outside execution.

Zero runtime options select 64 MiB of tracked runtime heap, 10000000 work steps,
200 nested calls and 100000 values per list. Tail calls reuse a call frame.
The heap budget covers runtime objects, tables, values and working byte buffers;
parser/AST storage, chunk compiler caches, MIR/native code and native thread
stacks have separate ownership and limits. Work counts are implementation/mode
dependent. Execution diagnostics borrow state storage until the next execution.

## Modules and bundles

`require` consults `package.loaded`, `package.preload` and configurable searchers.
The default Lua path is `./?.lua;./?/init.lua;./?.luac`; dots in module names
become path separators. Native modules use `package.cpath` (`./?.so` by default)
and `muneopen_<module>` symbols with dots replaced by underscores. A native
entry has the `mulib_mune_native_fn` signature and returns its module value in
the supplied result builder. `package.loadlib` accepts an explicit symbol.
Loaded shared libraries remain open until state destruction.

Describe modules with `mulib_mune_bundle_module_t { name, source, length }`,
then call `mulib_mune_bundle_write(modules, count, writer, user, &error)`.
All sources are syntax/semantically checked before the first output byte;
names must be nonempty and unique. The writer can target a `.luac` file.
`mulib_mune_load_bundle` and `mulib_mune_load_bundle_file` install lazy
`package.preload` functions. Alternatively, `require` discovers a bundle via
`package.path` and selects the named module from it. A bundle may contain many
interdependent modules. Successful module results are cached in `package.loaded`.

The portable bundle is a versioned **Mune source container**: 8-byte
`MUNEBC\0\1` magic, little-endian u32 module count, then u32 name length,
u64 source length, name bytes and checked source bytes for each module. Sources
are checked again on load and lowered for the chosen execution mode. Truncation,
duplicates, embedded-NUL names and trailing data are rejected. Registration
starts after validation; allocation failure while updating `package.preload`
can leave earlier registrations in place. This format and `string.dump`'s
versioned function-image format are independent of PUC Lua bytecode.

## Verification

`tests/test_mune_runtime.c` runs language/library assertions in tree, VM and
native-JIT modes and checks C callbacks, userdata, roots and bundles.
`tests/test_mune_runtime_alloc.c` injects failures throughout state creation,
tree execution and VM/JIT compilation/execution. `tools/check_mune_runtime.py`
also compiles/runs generated C and, when `lua5.4` is installed, compares the
shared assertion corpus and deterministic generated numeric/library cases.
These are regression/reference checks, not a claim that every Lua program or
all upstream implementation-specific behavior has been exhaustively tested.

## Lua frontend

```c
const char source[] =
    "local function factorial(n)\n"
    "  if n <= 1 then return 1 end\n"
    "  return n * factorial(n - 1)\n"
    "end\n"
    "return factorial(10)\n";
mulib_mune_error_t error;
mulib_mune_chunk_t *chunk =
    mulib_mune_parse(source, sizeof(source) - 1, NULL, &error);
if (chunk) {
    const mulib_ast_t *ast = mulib_mune_chunk_ast(chunk);
    /* Inspect ast, or render it with mulib_ast_render. */
    (void)ast;
    mulib_mune_chunk_destroy(chunk);
}
```

Adjacent C string literals provide inline multiline source. `mulib_mune_parse_file`
instead reads a file with checked I/O and the same source limit. It recognizes
the Lua `loadfile` UTF-8 BOM and first-line `#` convention, replacing this prefix
with spaces so diagnostic byte offsets remain accurate. Inline parsing treats
those prefixes as ordinary source and rejects them.

The lexer recognizes all Lua 5.4 keywords/operators, decimal/hexadecimal numbers,
short strings and escapes (including `\z`, `\x`, `\u` and decimal byte escapes),
and arbitrary-level long strings/comments. Identifiers use the ASCII/C-locale
letter/digit rules. Strings retain raw spelling in the AST; runtime evaluation
decodes them. Numeral overflow is not a syntax error. Malformed numeral
boundaries, invalid escapes, and unterminated strings/comments are diagnosed.

The lexer produces a token byte stream consumed by an embedded PEG grammar
through `mulib-parsec.h`. Transactional captures build an owned production AST.
The grammar covers expressions with Lua precedence/associativity, suffix calls
and indexing, constructors, functions/varargs, multiple assignment/return,
local attributes, loops, conditionals, labels and goto. AST node `offset`,
`length` and owned `text` refer to original source spans. The root retains the
whole source, including comments/whitespace. Children preserve grammar
structure; punctuation/operators without a named production remain in their
parent's text. Returned ASTs outlive the source and grammar. Borrowed trees
must not be mutated or individually freed; destroy the owning chunk.

Semantic analysis checks:

- legal assignment targets and function-call statements;
- lexical local shadowing and assignment to `<const>`/`<close>` locals,
  including captures by nested functions;
- local attributes and at most one to-be-closed variable per declaration;
- varargs only in vararg functions (the top-level chunk is vararg);
- break only inside a loop in the same function;
- duplicate visible labels, unresolved/non-visible goto targets, and jumps
  into local scopes, including the end-of-block label exception and repeat's
  extended local scope through its until condition.

Global names are legal without declarations. Dynamic value types, metamethods,
closability, and other runtime errors cannot be checked by this frontend. Its
temporary scope/jump records are released after validation; function capture
descriptors remain with the checked AST. Executable plans are cached lazily.

`mulib_mune_parse_options_t` bounds source bytes, tokens, AST captures/nodes,
semantic records per category, and work. Zero fields select defaults: 1 MiB,
100000 tokens, 100000 nodes, 100000 records, and 10000000 work steps. Parsing and
semantic checking each have that work budget. Parser combinator depth is 1024;
owned AST depth is 256. Both are implementation resource limits. Lexer storage
is bounded by the smaller of source length and token limit. AST source copies
can require O(source length × tree depth) memory. Diagnostic messages are static;
offsets are zero-based source byte offsets, including on allocation/limit errors.
NULL source is accepted only at zero length. Error output may be NULL.

`tests/test_mune_frontend.c` covers syntax, semantic rejection, source ownership,
file input and resource limits. Its `--check file.lua` mode performs frontend
validation without executing the file and supports differential testing against
Lua's `loadfile`.

## Legacy numeric execution

Create an owned persistent environment with `mulib_mune_create`, evaluate using
`mulib_mune_eval(m, source, length, &number, &error)`, and release it using
`mulib_mune_destroy` (NULL safe). Input is borrowed only for the call. Objects
require external synchronization; independent environments have no shared state.
`mulib_mune_get` copies a named value and leaves output unchanged if absent.

Grammar: global numeric assignments (`x = expression`), optional semicolons,
whitespace, single-line `--` comments, and an optional final `return expression`.
Expressions support decimal/exponent literals, variables, parentheses, unary
minus, `+`, `-`, `*`, `/`, and `%` with usual precedence and left associativity.
Modulo follows the divisor's sign, as in Lua. Values are finite C doubles;
require the C numeric locale for literal conversion. Division/modulo by zero,
nonfinite results and out-of-range literals are errors. An empty script or a
script without return produces zero. Assignments are transactional across the
entire evaluation: an error leaves both environment and output unchanged.

The legacy evaluator rejects strings, booleans/nil, local declarations,
functions/calls, multiple assignment/return, comparisons, concatenation,
exponentiation, floor division, control flow, tables, metatables and libraries.
All Lua keywords besides final `return` are rejected. The numeric restrictions,
transaction semantics and limits are deliberate deviations from full Lua.

Limits: 64 KiB source, 10000 lexer steps, 64 expression frames, 128 variables,
63-byte ASCII identifiers, 127-byte decimal literals. Length-delimited embedded
NULs are errors, not end-of-input. Source may be NULL at length zero. Diagnostics
hold a static message, status and byte offset. Cleanup accepts NULL; allocation
failure at creation returns NULL. Evaluation uses bounded temporary storage.

See `examples/modules/mune.c` and `tests/test_new_modules.c` for the numeric API.

`mulib_mune_version()` returns API revision 1.
