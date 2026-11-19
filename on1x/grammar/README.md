# Grammar boundaries

`On1x.asdl` owns the token arena, and `On1x.g` is the generated scanner grammar.
LibStone's actual `scripts/{asdl,aurocks}.pl` generate these in the build tree.
The C++ parser in `src/parser.cpp` owns source expression/statement semantics;
see [the readable grammar](../docs/language.md).

Local syntax groups add these parser productions (keywords remain identifier
tokens in **both** scanners):

```ebnf
syntax_group = ('let_syntax' | 'letrec_syntax') '{' { macro_definition } '}' ;
local_expression = syntax_group 'expression' '{' expression '}' ;
local_statement = syntax_group 'block' block [';'] ;
```

`macro_definition` is `macro name [literals(...)] [ellipsis name] { rules }`,
with the existing recursive patterns, categories and template syntax. Either
modifier order is allowed. The expression form also accepts ordinary postfix
calls/operators; as an expression statement it needs the ordinary semicolon.
The block form is only accepted as a statement, not a value.

Quotation regions use the same scanner tokens and ordinary statement grammar:

```ebnf
quotation = 'quote' '{' { statement } [ expression ] '}' ;
```

The parser preserves a sole expression exactly. A nontrivial region is
represented with existing `Quote`/`Call`/`Lambda` nodes before macro expansion,
so this addition changes neither scanner grammar nor Unit-v1 PPX wire schema.

The C++ `SyntaxLet` node holds the lexical definitions and one body; expansion
erases it before PPX, stage lowering or value inference. It is intentionally
**not** a `PPX.asdl`/wire constructor: processors cannot introduce imports,
executable registrations, or new macro definitions. `PPX.schema.json` remains
the authoritative versioned, macro-expanded S-expression schema. There is no
wire version or runtime ABI change for local syntax bindings.
