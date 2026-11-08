# mulib-asdl

[Manuals](README.md) · [Library index](index.md)

An owned schema API and compiler for the **Zephyr Abstract Syntax Description
Language** described by Wang, Appel, Korn and Serra, *The Zephyr Abstract Syntax
Description Language* (DSL'97), especially Figures 1–2 and Sections 2.1–2.6.
The implementation is original. Parsing uses a complete `mulib-parsec.h`
combinator graph, not a separate tokenizer or an external parser generator.

## Integration

Define these switches in exactly one C11 translation unit:

```c
#define MULIB_IMPL_ASDL
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#include "mulib-asdl.h"
```

Other translation units include the header without switches. Dependency
implementations are never enabled implicitly. The ASDL implementation does not
require PEG, JSON, S-expression or YANTLR implementations; generated text can be
consumed separately by those modules. Generated C requires only C99 and its
standard library, not MuLIB.

```c
const char source[] =
  "module Example { expr = Num(int value) | Add(expr, expr) | Empty }";
mulib_asdl_options_t options = {0};
mulib_asdl_error_t error;
mulib_strbuf_t output = {0};
options.format = MULIB_ASDL_C99;
options.c_prefix = "example";
if (!mulib_asdl_compile(source, sizeof source - 1, &options, &output, &error)) {
  fprintf(stderr, "%zu:%zu: %s\n", error.line, error.column, error.message);
}
/* Use output.data / output.length, then: */
mulib_strbuf_free(&output);
```

See [`examples/modules/asdl.c`](../examples/modules/asdl.c) for a checked example.

## Language and validation

The paper's grammar is accepted directly:

```text
description = definition*
definition  = type_id '=' (fields | sum)
sum         = constructor ('|' constructor)* ('attributes' fields)?
constructor = constructor_id fields?
fields      = '(' field (',' field)* ')'
field       = type_id ('?' | '*')? identifier?
```

Type identifiers start with `[a-z]`, constructor identifiers with `[A-Z]`.
Field names may start with either case. Subsequent bytes are ASCII letters,
digits or underscores. Leading underscores, non-ASCII identifiers, complex
inline types, empty field parentheses, trailing commas and semicolons are
rejected. A fieldless constructor is `Empty`, not `Empty()`.

Whitespace and `--`, `//`, and non-nesting `/* ... */` comments may separate
tokens. Comments and whitespace are discarded. As a conventional extension,
one `module Name { ... }` wrapper is accepted; it is not part of the 1997
paper's grammar. Bare and empty descriptions are valid. Bare descriptions use
`options.module_name`, default `asdl`; a wrapper overrides that option. Module
names follow the same identifier rules as field names.

Builtins are exactly `int`, `identifier` and `string`. Validation rejects
undefined field types, duplicate type names, globally duplicate constructor
names, duplicate explicit field names within a product/constructor/attribute
list, and constructor fields conflicting with shared attributes. Builtins and
the keywords `attributes` and `module` cannot be defined as types. Constructors
in different variants may reuse field names. Unnamed fields are independent.

Forward references and recursive sums are supported. Product-only cycles are
rejected, including cycles through optional or sequence fields; crossing a sum
breaks such a cycle. Attributes belong to sums, not products. A sum is emitted
as a finite enumeration only when all constructors are fieldless **and** there
are no attributes.

## Schema API: macros and inline constructors

```c
static const mulib_asdl_field_t fields[] = {
  MULIB_ASDL_FIELD("int", "value"),
  MULIB_ASDL_OPT("string", NULL),
  MULIB_ASDL_SEQ("expr", "children")
};
static const mulib_asdl_constructor_t constructors[] = {
  MULIB_ASDL_CONSTRUCTOR("Node", fields), MULIB_ASDL_ATOM("Empty")
};
static const mulib_asdl_type_t types[] = {
  MULIB_ASDL_SUM_TYPE("expr", constructors)
};
mulib_asdl_module_t *module = mulib_asdl_from_types(
    "Example", types, MULIB_ASDL_COUNT(types), &error);
```

`MULIB_ASDL_PRODUCT_TYPE` creates a product descriptor;
`MULIB_ASDL_ATTR_SUM` adds an attribute array to a sum. Array macros require
real arrays, not pointers. Runtime code can instead use the static inline
`mulib_asdl_field`, `mulib_asdl_constructor`, `mulib_asdl_product` and
`mulib_asdl_sum` helpers with explicit counts. These helpers create borrowed
descriptors; they do not allocate or validate. `mulib_asdl_from_types` deep-copies
and validates the entire schema, so arrays and strings can then be released.
Spans in API declarations default to zero and do not refer to source text.

`mulib_asdl_parse` returns the same owned module representation.
`mulib_asdl_name`, `mulib_asdl_type_count`, `mulib_asdl_type` and
`mulib_asdl_find` expose immutable borrowed views in declaration order, valid
until `mulib_asdl_destroy`. Builtins have no type descriptor. `mulib_asdl_is_enum`
identifies finite enumerations. Do not cast away const and modify module views.

`mulib_asdl_generate` emits any backend from a module;
`mulib_asdl_compile` combines parsing, validation, emission and destruction.
NULL options select S-expressions. Text is appended atomically to a caller-owned
`mulib_strbuf_t`: failure preserves existing content, success maintains a NUL
not counted in `length`. Source and option strings may alias the destination.
No filesystem access, shell commands, compiler invocation, global mutable state
or implicit YANTLR services are involved.

## Output schemas

### S-expressions (`MULIB_ASDL_SEXPR`)

```lisp
(asdl "Example"
  (sum "expr"
    (constructors
      (constructor "Num" 1 (fields (field "int" "value" "required")))
      (constructor "Empty" 2 (fields)))
    (attributes (fields))))
```

Products are `(product "name" (fields ...))`. Each field contains its type,
name and cardinality (`"required"`, `"optional"`, `"sequence"`). An absent
name is `()`, distinguishable from every quoted identifier, including `"nil"`.
Constructor tags start at one in declaration order and restart per sum.

### YANTLR (`MULIB_ASDL_YANTLR`)

The result is an executable `.y1` module with **no free dependencies**:

```text
intermediary "Example";
set Schema {"name": "Example", "types": [
  {"kind": "sum", "name": "expr", "constructors": [
    {"name": "Num", "tag": 1, "fields": [
      {"type": "int", "name": "value", "cardinality": "required"}]},
    {"name": "Empty", "tag": 2, "fields": []}], "attributes": []}]};
```

Products have `kind`, `name`, and `fields`. Sums have `kind`, `name`,
`constructors`, and `attributes`. Unnamed fields use `null`. Use
`mulib_yantlr_parse`, then `mulib_yantlr_evaluate` with no bindings, and retrieve
`Schema`. Downstream explicit providers can consume this schema; the output
does not pretend that nonexistent compiler-provider services are installed.

### C99 (`MULIB_ASDL_C99`)

The result is a guarded standalone header. With prefix `example`:

| ASDL | Generated representation |
| --- | --- |
| `int` | `example_t_int`, a signed `int64_t` |
| `string`, `identifier` | `example_t_string`, `example_t_identifier`: `{ const char *data; size_t length; }` |
| Product / non-enumeration sum `expr` | `example_t_expr`, a pointer to `struct example_node_expr` |
| Finite sum `binop` | `example_t_binop`, an enum with one-based tags |
| `expr*` | `example_seq_expr`: `{ size_t count; example_t_expr *items; }` |
| `expr?` | `example_opt_expr`: `{ int present; example_t_expr value; }` |

Sequence and optional wrappers exist for every builtin and defined type.
Optional presence is explicit, including for primitive zero values and enums.
Strings are length-delimited and may contain NULs; identifiers are not interned.
The C `int` representation deliberately uses finite precision, as permitted by
Section 2.2 of the paper. There is no arbitrary-precision arithmetic runtime.

Products expose `f_name` members. Sums expose `kind`, shared `a_name` attribute
members, and `v.c_Constructor.f_name` variant members. Unnamed fields use their
one-based position, e.g. `f_1`; unnamed attributes use `a_1`. Nullary variants of
non-enumeration sums contain a dummy byte rather than a nonstandard empty struct.
Tags are named `example_tag_expr_Num`. All enum definitions precede references,
so forward references do not require non-C99 enum forward declarations.

Inline constructors are `example_make_product(...)` or
`example_make_expr_Num(...)`. Variant arguments precede attribute arguments,
each in source order. Non-enumeration constructors use `malloc` and return NULL
on allocation failure; finite-enum constructors return their tag directly.
Constructors shallow-copy all arguments. **The caller owns the returned node and
all child nodes, sequence arrays and string storage**; release nodes with `free`
and manage children separately. There is no recursive destruction, since
caller-created structures may share nodes or contain cycles.

Every ASDL underscore is encoded as `_u` in generated identifiers, preserving
an injective mapping. For example `node_name` becomes `example_t_node_uname`.
Category prefixes avoid C keywords and collisions with generated positional
fields, so a field named `switch` is legal (`f_switch`). The C prefix follows
the same encoding and must start with an ASCII letter. Use distinct prefixes
for independent generated schemas included in the same translation unit.

These backends compile **schema definitions**, not tree values. The paper's
binary pickle/unpickle protocol, runtime value validation, imports, custom
builtin types, and additional target languages are not implemented.

## Errors, limits and verification

Functions return NULL/0 on failure. Optional errors are cleared on entry and
contain a stable owned message, code, source span and one-based byte line/column.
API declarations have no source, so their diagnostics use offset zero, line one,
column one. Parser diagnostics identify the farthest mismatch; no recovery is
attempted. Source is copied before parsing and may be released immediately.

Limits are 16 MiB source, 255 bytes per identifier, 65536 allocated schema
objects (including intermediate parse nodes), 1000000 semantic comparisons/work
units, and 64 MiB per generated document. Quadratic name checking is bounded
by the semantic budget; large schemas can reach it before the object cap.
Parsec depth/steps/captures may be configured through `options.parser`; zero
uses depth 256, steps 10000000 and captures 65536. Captures cannot exceed 65536.
Parsec additionally enforces its default 16000000 work-unit bound. Raising
parser limits does not disable schema/output caps. No native recursion is used
for parsing, product-cycle detection or code emission.

`tests/test_asdl.c` checks syntax, validation, descriptor/text agreement,
ownership, limits, transactional append, and parsing/evaluating YANTLR output.
`tests/test_asdl_alloc.c` injects failure at every allocation site.
`tools/check_asdl.py` compares complete S-expression/YANTLR schema content and
compiles and executes generated code under strict C99. CMake registers all
three checks; `tools/check.py` also runs the generated-code check. GCC, Clang,
and ASan/UBSan can be used without any external ASDL toolchain.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-asdl.h`](../include/mulib-asdl.h) |
| Implementation switch | `MULIB_IMPL_ASDL` |
| Repository-local includes | [mulib-arena](mulib-arena.md), [mulib-parsec](mulib-parsec.md), [mulib-strbuf](mulib-strbuf.md) |
| Complete example | [`examples/modules/asdl.c`](../examples/modules/asdl.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/asdl.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
