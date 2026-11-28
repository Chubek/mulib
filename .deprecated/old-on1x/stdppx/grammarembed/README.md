# GrammarEmbed

`GrammarEmbed` is a native On1x PPX that compiles a Perl 6/Raku-inspired
`grammar` construct into **ordinary On1x lexer and parser functions**. It uses
LibStone's `firestone/liblr` for Thompson construction, DFA determinization and
minimization, EBNF lowering, LR(1)/LALR(1) tables and LL(1) analysis.

## Example

The grammar occupies a literal string in an explicit PPX attribute:

```on1x
@GrammarEmbed("grammar",
  "grammar Calculator :lexer<dfa> :parser<lalr1> { token NUMBER { <[0-9]>+ } token SPACE :skip { \\s+ } rule TOP { <sum> } rule sum { <sum> '+' <term> | <term> } rule term { <NUMBER> | '(' <sum> ')' } }")
fn calculator_grammar() {}

fn main() {
  println(Calculator_matches("12 + (30 + 4)"));
  let (status, tree, offset) = Calculator_parse("12 + 30");
  println(status);
  println(sx_text(sx_head(tree)));
}
```

The marker must be a parameterless, empty `void` function with just this
attribute. It is replaced by generated declarations. Use the existing On1x
string escaping (`\\` to pass a backslash into the grammar, `\n` for a grammar
line break). On1x string literals do not span physical source lines.

```sh
cmake -S on1x -B build/on1x \
  -DON1X_LIBSTONE_ROOT="$PWD/on1x/third_party/libstone" \
  -DON1X_BUILD_NATIVE_PPX=ON -DON1X_BUILD_GRAMMAREMBED=ON
cmake --build build/on1x --parallel 4
python3 on1x/tools/on1x-build.py on1x/stdppx/examples/grammarembed.on1x \
  --on1xc build/on1x/on1xc --stdppx-path build/on1x/stdppx \
  --use-ppx GrammarEmbed -o build/grammar-demo
build/grammar-demo
```

The configured LibStone checkout must contain `firestone/liblr`. A bootstrap
without it can explicitly use `-DON1X_BUILD_GRAMMAREMBED=OFF`.
Installed discovery uses the generated `libexec/on1x/On1xfile`; the native
processor embeds its runtime template and catalog, so relocation requires no
LibStone headers, source checkout, Python generator, or external grammar file.

## Grammar language

```
grammar Name :lexer<dfa> :parser<lalr1> :start<TOP> {
  token NAME { regular expression }
  token SPACE :skip { regular expression }
  rule TOP { grammar expression }
}
```

- Defaults: minimized `dfa`, `lalr1`, and `TOP` when present (otherwise the first
  rule). Header options are optional; duplicates/unknown options reject.
- Definitions can reference later definitions. Names use ASCII letters,
  underscores and digits, start with a letter/underscore, have at most 48 bytes,
  and cannot begin `ge_`. Token and rule names share a namespace.
- `#` comments run to end of line outside quoted literals and byte classes.
  Semicolons after definitions are optional.
- Rules use `<NAME>` references, single/double-quoted literal terminals,
  adjacency, `|`, grouping with `()` or `[]`, and postfix `?`, `*`, `+`.
  An empty body or alternative denotes epsilon. Grouping is nestable to 32.
  Rule whitespace is formatting; skipping input requires an explicit skip token.
- Tokens use byte regular expressions: adjacency, `|`, `()`, `?`, `*`, `+`,
  `.`, `[abc]`, `[a-z]`, `[^a-z]`, and Raku-style `<[a-z]>` classes. Quoted
  literals are escaped automatically. A postfix quantifier applies to the
  whole quoted literal: `'ab'+` matches `ab`, `abab`, and so on, not `abb`.
  Write `'a' 'b'+` to repeat only `b`. `\d`, `\w`, `\s` expand to ASCII
  digit/word/whitespace classes. Liblr byte escapes `\n`, `\r`, `\t`, `\xHH`
  and escaped regex punctuation are supported. Formatting whitespace outside
  a class/quoted literal is ignored.
- `<TOKEN>` inside a token expands that token's regex. Recursive token
  definitions reject. Helper-only tokens do not become independent lexer rules:
  only tokens referenced by a parser rule and `:skip` tokens are scanned.
- Literal terminals get priority before named tokens. Remaining equal-length
  matches use token declaration order, including skip rules. **Longest match
  wins first**. Empty-matching lexer rules reject, including skip rules.
- Full input must match the start rule. There are no implicit anchors to write.

This is a documented grammar subset, not Raku language compatibility. Captures
are concrete parse trees; semantic action blocks, inheritance, dynamic rules,
backreferences, lookaround, counted repetition, Unicode properties and
context-sensitive lexing are not provided. Unsupported syntax rejects.

## Algorithms

| Selection | Emitted runtime machinery |
|---|---|
| `:lexer<dfa>` | Dense byte transition table from liblr Thompson NFA → subset construction → DFA minimization. |
| `:lexer<nfa>` | Thompson state-set simulation, precomputed epsilon closures and byte-range edges; no determinization. |
| `:parser<lr1>` | Canonical LR(1) shift/reduce driver and liblr action/goto tables. Supports left recursion. |
| `:parser<lalr1>` | LR driver with liblr's merged LR(0)-kernel tables. Supports left recursion, but some LR(1) grammars conflict. |
| `:parser<ll1>` | Predictive LL(1) table driver with an explicit work stack. Nullable/FIRST/FOLLOW analysis and left-recursion checks come from liblr. |

Conflicts are rejected; no implicit precedence resolution or silent algorithm
fallback occurs. Rewrite ambiguous expression grammars into precedence levels.
NFA/DFA have the same lexical priority rules. The supported algorithms are
explicitly selected; GLR/GLL/PEG are not selectable in this version.

## Generated API

For `grammar Name`:

```on1x
fn Name_lex(input: string) -> (i64, sexpr, i64);
fn Name_parse(input: string) -> (i64, sexpr, i64);
fn Name_matches(input: string) -> bool;
fn Name_name(token_or_rule_id: i64) -> string;
```

`lex`/`parse` return `(status, result, byte_offset)`. Status meanings are defined
by `catalog.json`: **0** success, **1** lexical error, **2** syntax error,
**3** resource limit. Every non-success result is `()`, with no partial tree or
token stream. Success consumes the complete input and returns its byte length.
`matches` returns true exactly for a successful parse; use `parse` to distinguish
malformed input from resource exhaustion.

`lex` returns a list of tokens:

```
((token_id begin_byte end_byte "lexeme") ...)
```

Token IDs and byte offsets are numeric S-expression symbols. Skip tokens and EOF
are omitted. `Name_name(id)` returns the token/rule display name, or `""` for an
invalid ID. IDs are deterministic for a particular grammar but not a stable ABI
across grammar edits.

`parse` returns a concrete tree. A rule `TOP { <NUMBER> '+' <NUMBER> }`
parsing `12+30` produces:

```
("TOP" 0 5 (("NUMBER" 0 2 "12") ("+" 2 3 "+") ("NUMBER" 3 5 "30")))
```

Each node has `(name begin_byte end_byte payload)`: terminal payloads are exact
lexeme strings; nonterminal payloads are child lists in source order.
Offsets refer to the original input, including skipped whitespace.
Epsilon nodes have a zero-width interval at the next token or EOF. Generated
`@ebnf…` helper nodes are retained, matching liblr's lowering; there is no
unadvertised AST simplification or semantic evaluation.

Use ordinary `sx_*` intrinsics or `stdlib/sexpr` to inspect the result. Generated
functions require no imported runtime helper module. Private function names are
fresh against the input unit; public-name collisions reject. Multiple distinct
grammars coexist. Calls have local parser state; the ordinary On1x runtime's
managed data facilities retain their existing thread/resource restrictions.
For use through a source module's exports, declare ordinary wrapper functions in
that module; ordinary PPX expansion occurs after loader export-name binding.

## Implementation, limits and verification

`native/GrammarEmbed.on1x` consumes schema-v1 ASTs with the PPX standard library,
checks markers, instantiates an embedded On1x machinery template, replaces
generated tables, freshens names, and validates the returned Unit. The normal
compiler subsequently checks types, ownership and generated C11.

`bridge.cpp` is a synchronous build-host-only C ABI adapter to liblr. C++ objects
never cross it; borrowed output is copied before the next invocation. Exceptions
produce empty plans and catalog messages. Grammar DSL parsing and liblr table
construction happen only in this companion. The actual output lex/parse engines
are in `machinery.on1x`, compiled to canonical AST at build time. Long tables
use bounded string chunks to remain portable strict C11. Generated applications
link only the ordinary On1x runtime, not LibStone or the PPX's C++ runtime.

The JSON catalog owns new messages, result statuses and admission/runtime limits:
16 KiB grammar source; 64 definitions; 2 KiB total emitted regex; 128 symbols;
256 productions; 64 symbols per RHS; 256 NFA/DFA states; 512 LR states; 1 MiB
plan; eight grammars per PPX request. Runtime limits are 64 KiB input, 4,096
tokens, 128 tree levels and 1,048,576 charged operations. NFA set operations are
charged conservatively and may exhaust work before the byte/token limits.
On1x's existing managed-data/pool limits also apply.
Those process-wide allocation limits retain the standard runtime's fatal-error
behavior; status 3 describes the grammar engine's own explicit limits.

Table size caps are checked after liblr construction; liblr's subset/LR
construction itself has no incremental work budget. The compiler's existing
PPX subprocess deadline (default five seconds) still bounds compilation time.
This is trusted build-host processing, not an OS sandbox or a claim of bounded
whole-process memory. Text uses On1x's current NUL-free byte-string ABI; Unicode
is treated as encoded bytes rather than graphemes.

Tests compile and run all six algorithm combinations, compare an exhaustive
short-input corpus to liblr's independent C++ lexer/parser runtimes, and exercise
tree/spans, epsilon, priorities, left recursion, LR1-versus-LALR1 conflicts,
malformed grammar/protocol output, collisions, runtime limits and relocated
installation. Exact measured configurations are in `on1x/VERIFICATION.md`.
