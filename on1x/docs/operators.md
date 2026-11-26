# Intrinsic operators

Define an operator, then use its punctuation in expressions.
The compiler checks its body and arguments just like those of an ordinary
function. No source extension, macro, or PPX processor is required.

```on1x
operator (lexeme: "<+>", arity: "binary", precedence: 5, tightness: 1,
    binds: "left", formals: left: i64, right: i64) -> i64 {
  return left * 10 + right;
}

fn main() {
  println(1 <+> 2 <+> 3);   # 123
  {
    infixr 0 <+>;
    println(1 <+> 2 <+> 3); # 33
  }
  let combine = operator("<+>");
  println(combine(4, 2));   # 42
}
```

Build and run the full [example](../examples/operators.on1x) from the
repository root using direct C11 (the example includes a function value):

```sh
on1xc --no-prelude --emit-c on1x/examples/operators.on1x -o operators.c
clang -std=c11 -I on1x/runtime operators.c on1x/runtime/on1x_runtime.c -lm -o operators
./operators
```

## Declaration and use

Put the `formals` attribute last. Supply one simple named formal for `"unary"`,
two for `"binary"`, or three for `"ternary"`. The optional `<T, ...>` after
`operator`, return type, and `where` constraints follow named-function syntax.
An operator body can have the same checked side effects as a named function.

| Arity | Form in an expression | Additional attribute |
|---|---|---|
| Unary | `OP value` or `value OP` | `position: "prefix"` (default) or `"postfix"` |
| Binary | `left OP right` | — |
| Ternary | `first OP second OP2 third` | `lexeme2: "OP2"` |

Lexemes have **two to six ASCII punctuation characters** and must form
one custom token. Write declaration spellings in ordinary quoted strings
without escapes. The ordinary glyph whitelist is ``!%&*+?^|~``;
angle forms additionally allow `<` and `>`, with `$` and `-` allowed
inside `<...>`. `<<.` and `>>.` admit the final dot, and the designated
angle/equality forms include `>>=`, `=<<`, `>=>`, and `<=<`. See the
[shared scanner recognizer](../include/on1x/operator_scan.h) for the exact
token boundaries, including adjacency rules for `!` and `?`.
Examples include `<+>`, `%%`, `^^`, `??`, `|?`, and `|>`. Core operators
and structural delimiters such as `+`, `?`, and `:` cannot be redefined.
Use whitespace where you intend adjacent punctuation as separate tokens.

`operator("<+>")` names the visible declaration as a function value, not a
call or a fixity override. It can be assigned, passed, and called under the
same rules as an ordinary function value. References use one spelling (the
first lexeme for a ternary); qualified operator references are not supported.
If both prefix and trailing declarations are visible for that spelling,
the bare function reference is ambiguous.

## Fixity and evaluation

Binary and ternary declarations can specify a precedence from 0 through 9
(higher binds tighter). Tightness, also 0 through 9, breaks ties at the same
precedence; built-ins have tightness 0. Both attributes default to 0.
`tightness` also accepts a quoted decimal. The built-in precedence tiers remain:

| Precedence | Built-in operators |
|---|---|
| 1 | `\|\|` |
| 2 | `&&` |
| 3 | `==` `!=` |
| 4 | `<` `>` `<=` `>=` |
| 5 | `+` `-` |
| 6 | `*` `/` `%` |

`binds: "left"` groups `a OP b OP c` as `(a OP b) OP c`;
`binds: "right"` groups it as `a OP (b OP c)`. Unary notation binds more
tightly than infix notation but less tightly than calls and projections.
Parentheses make grouping explicit.

`infix N OP;` and `infixr N OP;` change an existing operator's tightness
and make it left- or right-associative, respectively. They do not change the
primary precedence tier set at the declaration. The new fixity applies
to subsequent expressions in that lexical scope, including inner blocks
that do not override it. It does not change the target function, export the
override, or affect preceding expressions.

Module-scope declarations are visible throughout the module, including above
their definition. A declaration in a lexical block is visible from that
declaration to the end of the block and is not exported. It may shadow an
outer declaration; an earlier fixity override for the outer one does not apply
to the new declaration. A local operator body does not capture local variables.

Operands evaluate exactly once, in source order, before the body is called.
A ternary operator is **not** a short-circuit conditional. Use intrinsic `if`,
`&&`, or `||` where appropriate, or make delayed work explicit with a function
operand. Borrowing and moving operands obey the ordinary function-call rules;
operator syntax does not grant extra ownership privileges.

## Modules

Module-level operators are exported. With a named module, select an operator
explicitly by its first lexeme:

```on1x
from .arithmetic import (operator("<+>"));

fn main() {
  println(4 <+> 2);
}
```

Use this form rather than `Arithmetic.<+>` or an `as` alias. See
[Modules](modules.md) for search paths and the automatic pervasive environment.

## Standard-library operators

The default prelude includes the ordinary generic declarations from
`std.operators`. With `--no-prelude`, select only the operators you want,
for example `from std.operators import operator("|>");`.
All ten declarations use tightness 0.

| Form | Operation | Precedence and associativity |
|---|---|---|
| `value \|> f` / `f <\| value` | Apply `f` to `value` | 1; left / right |
| `f >>. g` / `g <<. f` | Compose, applying `f` then `g` | 7; left / right |
| `f <$> result` / `result <&> f` | Map a successful `Result<T,E>` | 2; left / left |
| `result >>= f` / `f =<< result` | Chain a result-returning function | 0; left / right |
| `f >=> g` / `g <=< f` | Compose two result-returning functions | 7; left / right |

For example, `20 |> increment |> increment` is 22 if `increment` adds 1.
Arguments to these operators are evaluated normally. The `Result` helpers
invoke a callback only on success and return the existing error otherwise.
The ordinary function equivalents and other helpers are in
`std.operator.utils`.

Explicit operator declarations are checked even when unused. Automatically
opened library bodies are demand-loaded; check the library directly to
inspect its complete set of signatures without needing an entry point.

## Tooling

`on1xc --operator-info FILE` prints checked operator information as JSON:
declarations, resolved uses, and module-scope fixity directives, with UTF-8
byte locations.
Add `--source-only` to check without executing PPX or declaration extensions.

`on1xc --dump-processor FILE` includes a `source_desugar` report from
[On1xMETA](../hir/On1xMETA/README.md), with rule names and rewrite counts.
The source phase is mandatory, including when `--disable-opt all` disables
optional optimizations. It runs after macro expansion and PPX fixity
readmission, before inference; the rest of the compiler receives ordinary
callable definitions and calls.

`on1xc fmt` supports local, imported, and pervasive operator syntax. It may
discover operator headers from dependencies but does not execute processors.
Use `--isolated` to limit it to the input file. The language server's save
diagnostics load the source graph and can report operator binding errors in
the saved entry file.
For saved, checked files, the [language server](../addons/lsp/README.md)
also offers operator hover and go-to-definition.
