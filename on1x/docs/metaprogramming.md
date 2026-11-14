# Typed hygienic macros, PPX and staging

The core-first increment now includes recursive R7RS-style patterns/templates,
self-hosted On1x authoring libraries and six native processors, and typed AOT
multi-stage programming (MSP). These are separate facilities: macros rewrite
source syntax, PPX transforms validated external ASTs, and MSP constructs typed
runtime code values. This is not a Scheme interpreter or a self-hosted compiler.

## Phases

```
scan → parse/imports → typed hygienic AST expansion
     → PPX 1 → validate → PPX 2 → validate → reconstruct core AST
      → stage checks/AOT lowering → inference/universal elaboration
      → ownership/type checking → C11
```

No token substitution or C preprocessor expansion is involved. PPX receives the
untyped, macro-expanded AST, not inferred types, closure proofs, CDecl objects,
or backend code. It cannot inject private HIR constructors. Transformations do
not waive the ordinary resource, foreign ABI, type, or definite-return checks.
PPX output cannot register more executables, imports, or macro definitions.

## Typed declarative rules

```on1x
macro unless {
  rule ($condition: Expression, $body: Block) => block {
    if !$condition { $body; }
  };
}
macro classify {
  rule ($n: HexLiteral) => expression { "hex" };
  rule ($n: NumericLiteral) => expression { "number" };
}
fn main() {
  unless!(false, { println(classify!(0x2a)); });
}
```

Macros live at module top level, including imported modules, and use a distinct
`name!(...)` namespace. All definitions are available throughout the merged
unit. Rules are tried in declaration order. Each capture **must** declare a
syntax category, separate from ordinary value types such as `i64`:

```
Syntax
├─ Block
└─ Expression
   ├─ Identifier
   ├─ Literal
   │  ├─ StringLiteral
   │  ├─ BooleanLiteral
   │  └─ NumericLiteral
   │     └─ IntegerLiteral
   │        ├─ DecimalLiteral
   │        └─ HexLiteral
   ├─ UnaryExpression / BinaryExpression
   ├─ CallExpression / TupleExpression / LambdaExpression
   └─ ExtensionExpression
```

The parent relation in `grammar/PPX.schema.json` is authoritative. A granular
category is admissible where an ancestor is required, not vice versa. There is
no floating-point literal category because On1x has no floating-point values.
Hex spellings (`0x`/`0X`) are recognized before value checking and lowered to
the existing checked `i64` representation. A leading minus is normally a unary
expression, not a literal (the existing minimum-i64 special case is retained).

`expression { expr }` produces one expression. `block { statements }` produces
a statement sequence, valid only in statement position; it **splices** rather
than adding a lexical scope. Use an explicit nested `{ ... }` for a new scope.
A Block capture used as `$body;` splices its statements. Identifier captures
can be used in binding/name positions: `let $name = value;`. These positions
require exactly `Identifier`, not a broader type that might admit other nodes.

### Recursive patterns and ellipses

Patterns use the R7RS `syntax-rules` structure, adapted to typed On1x ASTs:

```text
pattern = $name:Category | _[:Category] | literal
        | [sequence] | vector[sequence] | (sequence)
        | ast_call(sequence) | ast_unary(sequence) | ast_binary(sequence)
sequence = comma-separated patterns, each optionally followed by ellipsis
         + optional . tail-pattern
```

`[]` matches proper or dotted syntax lists, `vector[]` matches vectors, and
`(...)` matches ordinary tuple expressions. AST calls match their callee followed
by arguments. Unary/binary forms match a **string** operator followed by operands.
Lists/vectors are macro syntax, not new runtime collection types. Every such
carrier must disappear during expansion. Vectors cannot have dotted tails.
The invocation name already supplies R7RS's ignored macro-keyword position.

Any sequence position may repeat, including compound and nested patterns.
Zero matches, empty rows, fixed suffixes and multiple repeated groups work;
ambiguous groups use greedy left-to-right matching with bounded backtracking.
Captures retain their full repetition shape, not a flattened vector.

```on1x
macro rows {
  rule ([$x: Expression ...] ...) => block { println($x)... ...; };
}
macro bind {
  rule ([$name: Identifier, $value: Expression] ...) => block {
    syntax_block { let $name = $value; }...;
  };
}
fn main() { rows!([1, 2], [], [3]); bind!([answer, 42]); println(answer); }
```

Templates can repeat whole argument/tuple elements or statement expressions.
`syntax_block { statements }...;` repeats and splices statement groups; use an
inner `{ ... }` when each group needs a separate lexical scope. Dotted tails
forward residual syntax lists, e.g. `next!(. $rest)` or `f(1 . $rest)`.
`$x... ...` flattens two repetition levels; nested compound templates can retain
their grouping instead. Variables require enough surrounding ellipses for their
pattern rank; shallower captures can broadcast into deeper groups. All drivers
at a repetition level must have equal lengths, including empty dimensions.
Scalar/operator-position repetition, missing/excess ellipses, duplicate captures
and incompatible template categories are rejected, including unused definitions.
An `Expression` capture need not have any particular runtime value type.

`_` is a macro wildcard (optionally typed), but remains an ordinary runtime
binding name. `macro m literals(else, done, _) ellipsis dots { ... }` declares
literal identifiers/keywords and a custom ellipsis identifier. Explicit `$`
captures mean other bare identifiers remain literal patterns for compatibility.
Listing `_` makes it a binding-sensitive literal instead of a wildcard. The
ellipsis identifier cannot also be a literal. `syntax_literal(quote)` represents
a reserved keyword; `escape([...])` disables ellipsis recognition within a
template, corresponding to R7RS's `(... template)` escape.

A datum pattern is an identifier, keyword, string, boolean or integer.
Identifier literals compare lexical binding identity: a caller's
local binding with the same spelling does not match a definition-site global
identifier. String literals compare decoded bytes; integer patterns compare
mathematical values, including decimal/hex spellings and unary-negative data.
Arguments are matched **before** recursively expanding them.
Rules can drop or duplicate syntax; macros do not promise once-only evaluation.

### Hygiene

Introduced syntax receives a fresh context mark per expansion. Captured syntax
retains the caller's context. Bindings resolve by `(name, context)` within real
lexical scopes, then become deterministic collision-checked internal names.
Thus introduced temporaries cannot capture caller expressions, and free template
identifiers resolve at the definition site (module globals/intrinsics), not to
same-spelled caller locals. Nested expansions preserve these distinctions.
Identifier captures deliberately used as binders carry the caller's context;
normal scope/duplicate-binding rules apply to references in that context.

This implements the recursive R7RS-style **pattern/template** family over On1x
syntax, not the entire R7RS macro binding system. Definitions remain top-level;
`let-syntax`/`letrec-syntax`, procedural macros, Scheme reader data types absent
from On1x, and intentional-capture escape hatches are not implemented. Examples:
`examples/macros.on1x`, `examples/structural_macros.on1x`; tests: `tests/test_macros.py`.

## Registering external PPX

```on1x
@ppx checks "../stdppx/checks.py"
fn main() { @checks("assert_eq", 40 + 2, 42); }
```

```sh
on1xc --allow-source-ppx --emit-c program.on1x -o program.c
on1xc --ppx checks /absolute/path/to/checks.py --emit-c program.on1x
python3 on1x/tools/on1x-build.py on1x/examples/ppx.on1x \
  --allow-source-ppx --on1xc PATH/TO/on1xc -o /tmp/opencode/ppx-example
```

Source registrations (also in imports) require **`--allow-source-ppx`**. CLI
`--ppx LABEL PROGRAM` registrations explicitly authorize their own programs.
Source programs run first, in import/source order, followed by CLI registrations
in CLI order. Labels must be unique identifiers. Source paths are relative to
the declaring file; CLI paths are relative to the working directory. Paths are
not shell commands. Registration semicolons are optional.

The compiler sends one canonical S-expression on **stdin**, and expects exactly
one S-expression on **stdout**. Nonzero exits, signals, empty/trailing/malformed
output, unknown constructors, wrong categories, wrong fields and unknown
versions terminate compilation without emitted C. Errors identify the PPX label,
wire line/column and a schema path such as `$.functions[0].body.statements[1]`.
Failing-process diagnostics include a bounded stderr excerpt.

`ON1X_PPX_LABEL` and `ON1X_PPX_PROTOCOL=1` tell a program which label it owns.
The label is not an extra positional argument. Programs run shell-free with
inherited environment plus these two overrides. Host-native executables and
readable shebang scripts are supported on tested Linux/POSIX hosts. The shebang
must name an absolute interpreter, with at most one opaque interpreter argument
(e.g. `/usr/bin/env python3`). No execute bit is needed for shebang scripts.
Text files without a shebang never receive an implicit shell fallback. PE/Mach-O
files do not become runnable on Linux; unsupported host executables fail closed.
The non-POSIX transport currently reports unsupported rather than emulating it.

**Execution permission is not a sandbox.** Authorized PPX programs can access the
compiler user's files/network and run arbitrary code. Only use trusted programs
and inspect registrations in imports before enabling source execution. I/O uses
nonblocking streams to avoid stdin/stdout deadlock and SIGPIPE; timeouts/errors
terminate the process group. There is no caching or implicit daemon launch.

### Extension carriers and schema extensions

`@label("extension-name", args...)` is an expression carrier. Arguments can be
expressions or `{ statements }` Blocks. Before a function/extern, the same syntax
is a function attribute: `@derive("derive", "arity") fn f(...) ...`. Each carrier
retains its label, name, AST payloads and location. Every label must be registered;
all carriers/attributes must be consumed before ordinary checking.

`grammar/PPX.schema.json` is the versioned **tagged S-expression schema language**,
not JSON Schema over a JSON document. CMake embeds this exact file into the
compiler. A constructor is `(Tag field...)`; repeated fields are untagged lists.
Quoted strings are byte strings, symbols represent tags/enums/natural numbers.
Wire strings are ASCII with `\\`, `\"`, `\n`, `\r`, `\t`, `\xHH` escapes;
UTF-8 source text is preserved byte-for-byte. NUL does not enter the core AST.
Locations are `(Loc "file" offset line column)`. The root is `(Unit 1 (...))`.
There are no reader macros, comments, dotted pairs, or executable S-expressions.

For example, load an additional constructor with
`--ppx-schema producer /path/to/extension.json`:

```json
{"version":1,"nodes":{"producer:Box":{
  "category":"Expression",
  "fields":[["value","Expression"],["location","Location"]]
}}}
```

Constructors must be namespaced by their registered owner. Existing constructors
and categories cannot be redefined. Field types name a built-in syntax category
or scalar; `*` denotes a list. These typed nodes may survive **between** stages,
but some later processor must lower them to core constructors. Remaining custom
nodes fail before HIR reconstruction. This adds AST constructs and DSL payloads,
not arbitrary new lexer/parser productions for otherwise invalid On1x text.

LibStone's real `scripts/asdl.pl` generates and compiles the `PPX.asdl` portable
wire tree in generated builds. Received wire trees are materialized/structurally
validated in its temporary C11 arena; no arena-owned pointer survives. The
independent fallback has the same C++ codec/schema checks without LibStone.

`--emit-ast` exports macro-expanded, pre-PPX syntax for inspection/authoring. It
**never executes transformers**, even if the source registers them, and cannot
be combined with CLI `--ppx`/`--ppx-schema` registrations.

## Authoring libraries and ready-made processors

Native `.on1x` libraries now implement the S-expression parser/printer, persistent
visitor, schema JSON loader/validator, schema extension loader, constructors,
fresh-name search and `ppx_serve`. All six processors also have native On1x
implementations. CMake builds them into `BUILD/stdppx/on1x-ppx-NAME` by default.
They need no Python, compiler or LibStone at runtime; they do load the authoritative
schema data file. Set `ON1X_PPX_SCHEMA` to its absolute path. See the
[self-hosting/data runtime manual](selfhosting.md) for APIs, ownership and bounds.

The Python versions below remain available as bootstrap/reference tools:

These bootstrap modules need only the Python standard
library; the compiler itself has no Python runtime dependency.

| Path | API |
|---|---|
| `stdlib/sexpr` | `Atom`, bounded `loads` / canonical `dumps`; byte-preserving strings |
| `stdlib/visitor` | bounded postorder `rewrite`, iterative `walk`, constructor `tag` |
| `stdlib/ppx` | `Schema`, schema extensions, `serve`, AST constructors, `FreshNames` |

`serve(transform)` reads/validates one request, calls `transform(tree, label)`,
validates/writes its result, and reports errors only to stderr. Tools using
custom constructors pass their schema files via `extension_schemas`. The
compiler independently validates output; author tools are not the trust gate.
PPX is not automatically hygienic: use `FreshNames`, scope-preserving blocks,
and explicit handling of definition-site dependencies when introducing code.

Six actual scripts live in `stdppx`: **identity**, **checks** (`assert_eq/ne`),
**control** (`when/unless`), **unroll** (bounded literal `repeat`), **derive**
(function `arity/signature` metadata), and **fold** (checked literal arithmetic).
Checks use fresh global wrappers so caller shadowing cannot disable assertions.
Folding preserves invalid-literal errors, overflow/division traps and evaluation
order; it does not fold effectful operands. Derive is not a claim of Haskell
type classes or record/ADT deriving, which the language does not yet support.

## MSP

`code<T>`, `quote { expression }`, `splice(code)`, `lift(value)` and contextual
`run(code)` implement typed closed-code staging with ahead-of-time compiled C11
closures. Operands of splice/lift execute at construction time; quoted effects
wait until run. Cross-stage local references and future-binding extrusion fail
before inference. No JIT, unchecked eval or compiler subprocess is introduced.
See [the MSP semantics/lowering manual](staging.md) and `examples/staging.on1x`.

## Limits, trust and remaining work

- Macro passes: 512 definitions, 256 rules/definition, 1,024 expansions, 64 active
  expansions, 256 syntax levels, 262,144 cloned expression/statement nodes per
   cloning phase, and 8 MiB of copied expansion text/metadata accounting.
- Pattern matching: 256 active matching frames, 4 MiB cumulative capture metadata,
  and 4,194,304 matching/validation work units. Repetition ranks are at most 256.
- PPX: 32 programs/unit, 4 MiB per wire input/output, 262,144 wire nodes, 256 wire
  levels, 64 KiB stderr/program, 128-byte labels and 4,096-byte program/diagnostic
  paths. Default timeout 5,000 ms; set
  `--ppx-timeout-ms` to 1..60,000. Wire depth is independent of source depth.
- Schema extensions: 2 MiB JSON/file, 64 JSON levels, 65,536 JSON nodes,
  1,024 total constructors, 32 fields/constructor, 128-byte tag/field names.
  Reconstructed `forall` annotations have at most 1,024 binders. Ordinary
  inference/expanded-type/capture/codegen limits apply afterward as before.

Expansion and the whole PPX rewrite are transactional on AST clones. External
process side effects cannot be rolled back. There is no new application runtime
compiler dependency or closure ABI version change. Native authoring adds the
versioned data primitives documented in `selfhosting.md`; emitted C/runtime must
be rebuilt together.

Deferred work: local syntax bindings, general modules/ADTs and safe MuLIB modules,
the missing IPC foundation and
transport, the larger (~30) processor catalog, reviewed inline C/assembly
facilities, and a real ArchML integration. No placeholders are counted as these
features. See [verification](../VERIFICATION.md) for executed configurations.
