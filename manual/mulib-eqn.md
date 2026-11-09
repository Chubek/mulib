# mulib-eqn

[Manuals](README.md) · [Library index](index.md)

A bounded expression language for mathematical and logical notation, with an
inspectable owned AST, Presentation MathML, math-mode LaTeX and focused scalar
evaluation. It is not a TeX parser, computer algebra system or theorem prover.
It relies heavily on [mulib-symbol](mulib-symbol.md) for symbol identity and
operator behavior.

## Integration and lifetime

Define EQN, SYMBOL, MATHML, TEX, XML, STRBUF and UTF8 switches once, and link math
on platforms requiring `-lm`. The [runnable example](../examples/modules/eqn.c)
uses the explicit shared [notation recipe](../examples/notation-common.h); the
recipe also enables other independent notation modules for those examples.

`parse(registry,source,length,error)` borrows the registry, copies node text and
returns an owned AST or NULL. The registry must outlive the AST because symbol
descriptors are borrowed. Empty/whitespace input produces an empty group;
NULL source is allowed only at zero length. Source is XML-valid UTF-8, <=64 KiB;
trees have <=10000 nodes, <=64 levels and <=16 call arguments. Both parser
recursion and constructed tree height are bounded, including long left-associated
operator chains. Diagnostic messages are static, offsets are source bytes.
No partial tree escapes. `destroy(NULL)` is harmless. Accessors borrow AST text,
children and descriptors until destruction; child access outside bounds is NULL.

## Grammar

Examples:

```text
sqrt(x^2) + frac(1,2) >= 3 and not false
forall(x, x in real -> x^2 >= 0)
alpha + \beta * γ
```

Decimal numbers may have an exponent, rendered as mantissa times ten to a power.
ASCII identifiers are letters followed by
letters/digits; an otherwise unregistered Unicode scalar is an identifier.
Registered names become symbols, Unicode spellings are recognized, and explicit
backslash names must be registered (and may contain underscores). `_` is an
operator, not part of an ordinary identifier.
Parentheses/braces group expressions. Calls use comma-separated arguments.
There is no implicit multiplication: `2*x` is valid; `2x` is rejected.

Operators use registry precedence/associativity. ASCII aliases include `*`, `-`,
`!`, `<=`, `>=`, `!=`, `&&`, `||`, `->`, `=>`, and `<->`. Native Unicode set/logic
operators also work. Prefix minus/not bind below exponentiation. Ordinary calls
may name user functions; special constructors frac(a,b), sqrt(a), root(a,n),
pow(a,b), subsup(base,sub,sup), forall(variable,body), and exists(variable,body)
have checked presentation arities. Scripts follow the declared operator grammar,
not full TeX simultaneous-sub/superscript rules; use `subsup` when needed.

## Outputs and evaluation

`mathml` returns a new owned inline/block MathML root; `latex` returns malloc-owned
math-mode source without dollar delimiters. Optional lengths exclude NUL and are
zero on failure. XML text is escaped, ordinary identifiers are TeX-escaped, and
registered TeX fragments are trusted. Grouping is rendered explicitly so output
does not obscure operator association. MathML passes the existing MathML module's
structural profile. Consumers may need AMS packages for catalogue commands such
as `\mathbb`; no TeX process or package is implicitly loaded.

`eval` returns 1/0 and changes the output only on success. A host callback resolves
variables/custom atomic symbols to finite doubles. Builtin pi/true/false, finite
arithmetic, power/division, comparisons, Boolean logic, sqrt/abs/sin/cos/exp/ln,
frac/root/pow are implemented. and/or/implication short-circuit. A zero divisor,
invalid domain, nonfinite result, unsupported operator/function, quantifier or
unresolved variable fails with a diagnostic. Equality is double equality, not
symbolic equivalence. Root uses real `pow(a,1/n)` domain conventions. There is no
proof search, quantifier universe or automatic simplification. Numeric parsing
conservatively rejects values/intermediates outside the supported finite range.

AST operations require caller synchronization. Parsing/rendering/evaluation are
bounded by source/tree limits and catalogue lookups. Evidence:
[functional/hostile tests](../tests/test_notation.c),
[allocation injection](../tests/test_notation_alloc.c), and
[XML/TeX interoperability](../tools/check_notation.py).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eqn.h`](../include/mulib-eqn.h) |
| Implementation switch | `MULIB_IMPL_EQN` |
| Repository-local includes | [mulib-symbol](mulib-symbol.md), [mulib-tex](mulib-tex.md) |
| Complete example | [`examples/modules/eqn.c`](../examples/modules/eqn.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eqn.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
