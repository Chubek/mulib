# Native On1x PPX authoring libraries

The parser/printer, walker, schema JSON reader/validator, extension loader and
six reference processors are now written in On1x. They compile through the
ordinary compiler to standalone C11 executables. **The C++ compiler itself is
not self-hosted.** Imports are existing file imports, not a new ML module system.
Python implementations remain bootstrap and differential-test references.

## Build and run

CMake builds `BUILD/stdppx/on1x-ppx-{identity,checks,control,unroll,derive,fold}`.
Use `-DON1X_BUILD_NATIVE_PPX=OFF` to omit these tools. No PPX is executed while
compiling their sources. Only the generated C11 and small C11 runtime are linked.

```sh
export ON1X_PPX_SCHEMA="$PWD/on1x/grammar/PPX.schema.json"
on1x/build/on1xc --ppx checks "$PWD/on1x/build/stdppx/on1x-ppx-checks" \
  --emit-c program.on1x -o program.c
```

The schema remains one authoritative JSON file, not a duplicated hard-coded tag
table. Native reference executables require its absolute path in
`ON1X_PPX_SCHEMA`. A custom program can call `ppx_schema_load(path)` explicitly.
`ppx_serve` requires the compiler's `ON1X_PPX_PROTOCOL=1` and assigned label in
`ON1X_PPX_LABEL`. Source registration still requires `--allow-source-ppx`.
Authorization is not sandboxing; these tools can read files/environment and use
ordinary/raw On1x functions. No Python/LibStone/compiler is needed at runtime.

## Libraries

| Import | Main API |
|---|---|
| `stdlib/sexpr/sexpr.on1x` | `sexpr_parse`, `sexpr_print`, `sexpr_list`, `sexpr_size`, `sexpr_nth`, `sexpr_reverse`, `sexpr_nat` |
| `stdlib/visitor/visitor.on1x` | `visitor_tag`, persistent postorder `visitor_rewrite`, preorder `visitor_walk` |
| `stdlib/ppx/ppx.on1x` | `ppx_schema_load`, `ppx_schema_extend`, `ppx_validate`, `ppx_serve`, `ppx_fresh`, `ppx_node`, `ppx_ident`, `ppx_integer`, `ppx_call`, `ppx_list1..4` |
| `stdlib/ppx/numbers.on1x` | Trap-free literal recognition and signed-i64 formatting |
| `stdlib/ppx/signature.on1x` | Source annotation signature rendering, including `code<T>` |

`visitor_rewrite(tree, fn(node: sexpr, path: sexpr) -> sexpr {...})` rebuilds
children first, then calls the transform. `visitor_walk` takes the same callback
arguments with a void result and visits parents first. Paths are proper lists of
zero-based natural-number **symbols**, root `()`. A transform's output is not
revisited automatically. Printing/schema validation separately bound its output.

`ppx_serve(schema, fn(tree: sexpr, label: string) -> sexpr {...})` reads one
request, validates it, transforms it, validates the result and only then writes
canonical stdout. Diagnostics go to stderr and terminate with status 73.
The compiler independently validates output: authoring libraries are not its
trust boundary. Schema extensions return a new schema; existing constructors
cannot be redefined and new tags must start with `owner:`.

`ppx_fresh(tree, previous_serial)` returns `(name, serial)`, reserving every
input atom spelling; pass the returned serial to subsequent calls. PPX is not
automatically hygienic. The checks processor uses a fresh global wrapper to
protect `assert` from caller-local shadowing. Native checks currently emits that
wrapper even when unused. Native derive emits source arity/signature metadata,
not type classes or ADT deriving. Native fold preserves invalid literals and
arithmetic traps. The other processors have the same operations as the bootstrap
versions; see `stdppx/README.md`.

## C11 substrate and ownership

`sexpr` and `buffer` are opaque runtime-managed types. `sx_nil()` is an empty
list; `sx_symbol(text)` and `sx_string(text)` are distinct atoms;
`sx_cons(head, tail)` requires a proper-list tail. `sx_kind` returns 0 for nil,
1 for symbol, 2 for string, 3 for a nonempty list. `sx_head`/`sx_tail` require
a nonempty list and return owned references. `sx_text` requires an atom.
`data_live_nodes()` is an ownership-test counter, not a tracing collector.

S-expressions are immutable, persistent and reference-counted. Buffers are shared
mutable references: `buffer_new`, `buffer_append`, `buffer_byte`, `buffer_clear`,
`buffer_len`, `buffer_text`. Managed arguments borrow; results own. The compiler
retains/releases copies, captures, tuple fields, returns, assignments and
temporaries automatically. Destruction uses the environment runtime's iterative
queue; long flat lists do not recurse during destruction. Managed extern ABIs
are rejected. These built-in rules are not a generalized safe MuLIB/FFI binding.

`text_len`, `text_byte`, `text_slice` and `text_concat` operate on bytes, not
Unicode code points. Slices use half-open `[begin,end)` bounds. `io_stdin`,
`io_stdout`, `io_stderr`, `tool_env`, `tool_read_file` and `tool_fail` provide
bounded process I/O. The runtime contains **no S-expression/JSON parser, schema
logic, walker or AST transformer**; that logic lives in the On1x sources.

The existing `string` ABI remains `const char *`. Dynamic text operations and
atom construction use a **bounded process-lifetime snapshot pool**, freed at
normal process exit. `buffer_text` and `sx_text` therefore remain valid after
buffer mutation or node destruction. This intentionally avoids claiming an
owning, length-aware String implementation: long-running applications must
respect the cumulative snapshot budget. Raw extern strings still carry all
foreign lifetime obligations. NUL is rejected, including `\x00` in the native
codec; the compiler's core schema also forbids NUL. Other bytes round-trip via
ASCII/hex escapes without Unicode normalization.

## Limits and failures

- Native codec/visitor/schema validation: 4 MiB wire text, 262,144 logical nodes,
  256 levels; flat list spines are iterative, not nested syntax.
- Schema JSON: 2 MiB, 64 levels, 65,536 nodes, 4,194,304 JSON parsing work units;
  duplicate keys, malformed escapes and unpaired Unicode surrogates fail closed.
  Constructor/field limits match the compiler: 1,024 constructors, 32 fields,
  128-byte names; numeric location symbols use checked unsigned-64 bounds.
- Runtime: 4 MiB per text/buffer, 64 MiB cumulative text snapshots (including
  allocation headers), 64 MiB live buffer capacity, 1,048,576 live managed nodes.
  These independent budgets may reject a tree before its wire-size limit.
- Fresh-name search: 4,194,304 work units. Compiler transport timeouts still apply.
- Data/I/O errors exit 73, environment allocation errors 72, checked arithmetic
  errors 70. Fatal exits do **not** unwind language owners. Normal exits release
  owned nodes/environments and the text pool; no concurrent reference counting
  or general exception handling is provided.

`ON1X_DATA_MAX_TEXT`, `ON1X_DATA_MAX_POOL`, `ON1X_DATA_MAX_NODES`,
`ON1X_DATA_MALLOC/FREE` are C11 runtime build/test hooks. Generated C checks
`ON1X_DATA_ABI_VERSION=1`, alongside closure ABI version 1. Rebuild emitted C
and runtime together; this is not a stable foreign extension ABI.

`on1x.selfhost` compiles all six native processors independently, runs a chain,
bootstraps the authoring library through its own identity PPX, compares codecs
and folding with Python, loads/rejects extension schemas, checks paths/fresh
names, destroys a 100,000-node list, and exercises allocation/bounds failures.
It tests normal-exit cleanup under sanitizers separately from fatal non-unwinding
paths. General ADTs/modules, a self-hosted compiler, IPC and a 30-processor catalog
remain separate future work.
