# mulib-symbol

[Manuals](README.md) · [Library index](index.md)

An original dependency-free catalogue of Greek, mathematical, set, arrow and
logical symbols, with extensible Unicode/LaTeX/MathML representations. This is
not the complete Unicode name database; [uniname](mulib-uniname.md) is a separate,
optional ICU-backed facility. No ICU/font library is used here.

Define SYMBOL, MATHML, XML, STRBUF and UTF8 implementation switches once.
The [example](../examples/modules/symbol.c) adds a custom operator. Equation
parsing in [mulib-eqn](mulib-eqn.md) uses this registry for operator precedence,
associativity, Unicode spellings and output representations.

## Registry and descriptors

`create` returns an independent extension registry or NULL. NULL registry is a
valid readonly view of builtins. `destroy(NULL)` is harmless. Builtins are static
immutable descriptors; custom entries and all four descriptor strings are copied
and individually allocated, so later insertions do not invalidate existing
descriptor pointers. Destroying a registry invalidates its custom descriptors.
No shallow copy of an owned registry is supported.

`define` returns 1 on atomic insertion, 0 on invalid descriptor, duplicate name
or Unicode spelling, the 1024-entry extension bound, or allocation failure.
Builtin entries cannot be overridden. Names are ASCII letters/underscore followed
by letters/digits/underscore, <=63 bytes. Unicode spellings are nonempty XML-valid
UTF-8, <=64 bytes. LaTeX is <=256 bytes of XML-valid text, and is **trusted author
TeX**, not escaped user input. `token` is mi/mo/mn. Flags encode prefix/infix and
right-associativity; precedence is 1..90 for operators, zero for atoms. Custom
right-associative flags require an infix operator. Custom
operators are presentation metadata; they do not automatically define numerical
evaluation semantics.

`find` matches an exact case-sensitive name, optionally prefixed by a backslash.
`unicode` matches an exact spelling. `match` returns the longest spelling prefix
of a borrowed length-delimited input; optional consumed length is zero on absence
or invalid input. It does not perform identifier-boundary checks or normalization.
`count`/`at` enumerate builtins then custom entries, with NULL out of range.
All lookup results are borrowed. Duplicate Unicode spellings in the builtin
catalogue, if introduced, would select first catalogue order; extensions reject
such collisions.

`mathml` copies a descriptor's Unicode spelling into a new owned MathML token;
NULL means invalid descriptor/allocation failure. Destroy it with
`mulib_xml_destroy`. Callers should use registry-validated descriptors, not forge
invalid pointers. Numerical codepoints, fonts, glyph metrics and bidirectional
layout are outside this module's contract.

Lookups scan the small bounded catalogue, O(entries × compared bytes). Registries
require external synchronization for concurrent edits. Test coverage includes
custom definitions, duplicate rejection, equation composition and moving
allocation failures in [test_notation.c](../tests/test_notation.c) and
[test_notation_alloc.c](../tests/test_notation_alloc.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-symbol.h`](../include/mulib-symbol.h) |
| Implementation switch | `MULIB_IMPL_SYMBOL` |
| Repository-local includes | [mulib-mathml](mulib-mathml.md) |
| Complete example | [`examples/modules/symbol.c`](../examples/modules/symbol.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/symbol.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
