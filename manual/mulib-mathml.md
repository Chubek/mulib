# MuLIB MathML: Presentation MathML on the XML DOM

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_MATHML`, `MULIB_IMPL_XML`, `MULIB_IMPL_STRBUF` and
`MULIB_IMPL_UTF8` in one translation unit and include `mulib-mathml.h`.
No external libraries. [Complete example](../examples/modules/mathml.c).

## Supported structural profile

The root is an unprefixed `math` with explicit default namespace
`http://www.w3.org/1998/Math/MathML`. Namespace prefixes, nested math roots,
namespace redeclarations and unknown elements/attributes are rejected.
Supported elements are:

| Elements | Content |
| --- | --- |
| `mi`, `mn`, `mo`, `mtext`, `ms` | UTF-8 text/CDATA and comments, no elements |
| `mspace` | Empty apart from whitespace/comments |
| `math`, `mrow`, `msqrt`, `mstyle`, `mphantom`, `mtd` | Zero or more presentation children |
| `mfrac`, `mroot`, `msub`, `msup`, `munder`, `mover` | Exactly two element children |
| `msubsup`, `munderover` | Exactly three element children |
| `mtable` | One or more `mtr` children |
| `mtr` | One or more `mtd` children, inside `mtable` only |

`mtd` occurs only inside `mtr`. Non-token elements accept only whitespace text.
Token contents are not evaluated or classified numerically (`mn` does not require
a C-style number). Empty tokens are accepted. Comments do not contribute to arity.

Supported attributes: root `display=inline|block`; common `id`, `class`,
`mathcolor`, `mathbackground`, `mathvariant`, `displaystyle`; operator `form`,
`stretchy`, `fence`, `separator`, `symmetric`, `largeop`, `movablelimits`,
`lspace`, `rspace`; `mspace` width/height/depth; and `mfrac` linethickness.
Booleans and enumerated display/form/mathvariant values are checked.
Color, class, identifier and dimension values are preserved as XML strings;
their CSS/length semantics are not evaluated. This is structural validation,
not full MathML schema validation or rendering. Content MathML, semantics,
annotations, entity catalogs, layout, arithmetic evaluation and TeX conversion
are outside the profile and unknown markup fails explicitly.

The [XML parser](mulib-xml.md) supplies UTF-8 validation, XML character checks,
escaping, numeric/predefined entities, and a 128-element nesting bound. Its
DTD/external-entity and processing-instruction restrictions apply.

## Building, parsing and ownership

`mulib_mathml_new(block)` allocates a math element with namespace and display.
`mulib_mathml_element(name)` allocates a supported non-token container/space;
`mulib_mathml_token(name, text)` copies a NUL-terminated UTF-8 token string.
NULL means bad input or allocation failure. A new fixed-arity container is
temporarily invalid until populated. Use `mulib_xml_append` to attach children:
ownership transfers only on success. Use XML attribute and traversal APIs
directly; validate after editing. Destroy trees with `mulib_xml_destroy`.

`mulib_mathml_validate` accepts an element root or an XML document and returns
1/0. Optional errors contain a borrowed failing node and static message.
`mulib_mathml_parse(source, byte_length, error)` returns an owned XML document,
or NULL. Parse errors carry XML byte offsets; structural errors use offset zero.
Failed parse always clears the node pointer because its temporary tree is freed.

`mulib_mathml_serialize` validates before returning malloc-owned NUL-terminated
UTF-8. Free it with `free()`. Optional length excludes NUL and is zero on failure.
Errors are per-call, not global. Successful validation clears diagnostics.
Inputs remain caller-owned and are not modified. Tree traversal is linear in
nodes/attributes, bounded to XML's depth limit. Structural diagnostics distinguish
bad namespace, unsupported syntax and incorrect child counts; construction uses
NULL for both invalid arguments and resource exhaustion.

`tests/test_ebook_formats.c` covers nested fractions/scripts, escaping, roundtrip,
bad arities, tokens, tables and attributes. `tests/test_ebook_alloc.c` checks
allocation-failure cleanup.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-mathml.h`](../include/mulib-mathml.h) |
| Implementation switch | `MULIB_IMPL_MATHML` |
| Repository-local includes | [mulib-xml](mulib-xml.md) |
| Complete example | [`examples/modules/mathml.c`](../examples/modules/mathml.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mathml.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
