# YANTLR core

`mulib-yantlr.h` implements the `.y1` **host language**, static linking,
evaluation and provider-driven artifact generation. It does not implement the
YANTLR standard library or interpret the declarative rule strings in the example
specifications. In particular, parsing `examples/Lua.y1` or `examples/YBASIC.y1`
does not by itself produce a Lua or BASIC compiler. Their proposed `Gen[...]`
and `Services[...]` exports must be supplied explicitly; missing exports are
errors, not ignored stages. `library/yantlr` is unchanged.

## Integration

Define these switches in exactly one C11 translation unit:

```c
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PEG
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_YANTLR
#include "mulib-yantlr.h"
```

The PEG host grammar compiles into `mulib-parsec.h` combinators. JSON decodes
literal strings and numbers, arena storage gives stable pointers, and strbuf
builds canonical qualified names and reads files. No third-party dependencies.
See the [standalone provider example](../examples/modules/yantlr.c).

## Supported syntax and semantics

```text
intermediary "Name";
set Config {"answer": 42, "enabled": true};
set Component Host[Language]::Constructor!Config;
set Filter \value -> value.code `geq` Threshold;
```

* One required, nonempty, NUL-free intermediary name precedes zero or more
  `set NAME expression;` declarations. Names are case-sensitive ASCII
  `[A-Za-z_][A-Za-z_0-9]*`. `intermediary`, `set`, `true`, `false`, and `null`
  are reserved. Duplicate declarations, record keys (after decoding) and lambda
  parameters are rejected.
* Whitespace, `//` line comments and non-nested `/* ... */` comments are accepted
  outside strings. The parser requires the complete input.
* JSON strings, finite binary64 numbers, booleans, null, arrays and records are
  expressions. Strings/keys are valid UTF-8, decode JSON escapes and surrogate
  pairs, and may contain NUL. Numbers have JSON spelling and binary64 precision;
  they are not arbitrary-precision integers. Containers may contain functions or
  opaque provider values. Trailing commas and single-quoted strings are rejected.
* Qualified symbols have the form `Root[Qualifier,Other]::Member::Leaf` (the
  qualifier list and member suffixes are optional). Whitespace/comments between
  components are ignored; registry names use canonical spelling without them.
  Qualifiers are symbolic names, not evaluated arguments. A bracketed identifier
  list immediately after a name is a qualifier list, not an array argument;
  parenthesize an array argument to disambiguate.
* Postfix `.field` selects a record member. Postfix `!atom` applies a constructor
  to one argument. Ordinary juxtaposition applies functions left-to-right:
  `f a b` is `(f a) b`. Postfix binds tighter than juxtaposition, which binds
  tighter than backtick infix application. ``a `op` b`` means `op a b` and infix
  chains fold left. Use parentheses around compound constructor arguments.
* `\x y -> expression` creates a curried lexical closure. Application evaluates
  the function and argument eagerly, in source order. Lambdas capture parameters
  lexically. Parameters shadow global/provider names. Operators such as `geq`
  are explicit provider bindings, not implicitly installed standard-library code.
* Top-level sets form an acyclic dependency graph; forward references are
  allowed. All names, including names in uncalled lambda bodies, and every
  declaration cycle are checked **before any provider callback**. Recursive sets
  (including recursive closures) are intentionally rejected. Sets are evaluated
  once in depth-first dependency order, selecting roots and references in source
  order. All declarations are evaluated, not just a specially named `Build`
  declaration.
* Strings never become expressions, imports, dependencies or shell commands.
  The prose inside the supplied design specimens remains provider-owned data.

There are no implicit imports, filesystem search roots, arithmetic operators,
process execution, standard services or automatic native compiler invocation.

## API and ownership

`mulib_yantlr_parse(bytes, length, options, error)` copies and parses source;
`mulib_yantlr_load(path, ...)` explicitly reads a binary file. Inspect the
intermediary name, declaration names/spans and distinct external dependencies
without running providers. External dependencies are listed in first occurrence
order and exclude declarations and bound lambda parameters. Spans are zero-based,
half-open byte ranges into the original source; structural spans may include
following whitespace/comments. Diagnostics own their message and report 1-based
byte line/column positions (LF advances the line).

`mulib_yantlr_evaluate(module, bindings, count, options, error)` first validates
the registry, resolves every required symbol and rejects cycles, then evaluates
all declarations. It returns an owned run or NULL; failure destroys intermediate
values, opaque owners and staged artifacts. The module must outlive all its runs.
Registry storage/constant JSON can be destroyed after evaluation; native `user`
data remains borrowed for the run lifetime. Modules are immutable after parsing;
separate runs do not share mutable state. Individual runs are not thread-safe.

A binding supplies either a JSON constant or an arity-positive native function,
never both. Canonical names must be unique and cannot collide with declarations.
Native functions are curried and receive the complete borrowed argument vector
only once enough arguments have been supplied. Return 1 and a non-NULL,
run-owned result; otherwise call `mulib_yantlr_fail` and return 0. Use `get` to
inspect evaluated declarations, and `apply` to invoke retained closures from C
or a provider. The first error is sticky and hides all artifacts and globals;
destroy a failed run rather than trying to recover it. Fuel is cumulative within
each run, including later applications and provider calls into the evaluator.

Value constructors copy strings/keys and container references into run-owned
storage. Child values must belong to that run. `from_json` deep-copies JSON.
No individual value is freed. `opaque` transfers ownership only on success;
its optional destructor runs exactly once when the run is destroyed, in reverse
value-creation order. Native providers can use opaque values for grammar/AST/IR
handles, and compose existing MuLIB parser, rewrite, analysis and codegen APIs.
Provider callbacks are trusted C code: external side effects, unbounded work,
direct allocation and reentrant destruction are not sandboxed or rolled back.
Do not destroy a module/run from a callback or opaque destructor.

## Generation and failure handling

Providers call `mulib_yantlr_emit(run, relative_path, bytes, length)` to stage
generated C, headers, dependency manifests or other files. All names and bytes
are copied. Names must be canonical relative slash paths, at most 4095 bytes,
without empty, `.`/`..` components, control bytes, backslashes or colons.
Duplicate paths fail instead of overwriting an earlier output. Zero-byte artifacts
are supported. No files are written by the core. After successful evaluation,
iterate `artifact_count`/`artifact` in emission order and publish them using the
application's filesystem or buildgraph policy. No partial run is returned on
failure; arbitrary provider side effects are not transactional.

Errors distinguish syntax, allocation, resource limits, duplicate names,
unresolved bare names, unresolved qualified services, cycles, types, provider
failures, IO and invalid artifact output. All optional error pointers accept NULL.
NULL destroy/query arguments are harmless. NULL source is permitted only at
length zero (which is a syntax error because the intermediary is required).

Zero options select parser depth 2048, 10,000,000 parsec steps, 1,000,000 captures;
host depth 256, 1,000,000 traversal/evaluation steps, 1,000,000 values and 64 MiB
of artifact payloads. Parsing, static linking and evaluation have separate fuel
budgets. Depth counts host expression/grammar frames, not just container nesting.
Allocation sizes are checked for overflow. Limits do not bound source-file size,
string bytes, registry size, arena overhead, or the work inside native providers;
applications should impose input limits before loading untrusted specifications.

Focused tests: `tests/test_yantlr.c` exercises both supplied `.y1` specimens,
literal decoding, lexical closures/currying, linking and cycles, provider
failures, ownership, generation staging and resource limits.
`tests/test_yantlr_alloc.c` sweeps allocation failures through parsing and
evaluation, checking cleanup of partial values, opaque owners and artifacts.
