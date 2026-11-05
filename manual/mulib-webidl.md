# WebIDL compiler

`include/mulib-webidl.h` parses a UTF-8 WebIDL source span and compiles it to:

* **S-expression AST** (default): semantic definitions, members, types and arguments.
* **S-expression CST**: a lossless concrete token tree, retaining punctuation,
  whitespace, comments, original spellings and byte offsets.
* **CodeYAML**: the same semantic AST as YAML mappings and sequences.
* **C99**: a standalone declaration header with explicit data representations.

This is a syntax compiler and declaration generator, **not** an implementation of
ECMAScript conversions, browser bindings, overload resolution or all WebIDL
validation rules. Unsupported C lowering fails with a diagnostic; semantic
formats can represent declarations whose types are supplied in another file.

## Integration and API

Define `MULIB_IMPL_WEBIDL`, `MULIB_IMPL_DBSCHEMA`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF` and `MULIB_IMPL_UTF8`
in exactly one translation unit, then include `mulib-webidl.h`. Dependencies
are included normally; the header never defines their implementation switches.
Compile implementations as C99 (other MuLIB modules may require C11). Public
declarations provide C linkage for C++17 clients.

```c
const char source[] = "dictionary Point { required double x; double y = 0; };";
mulib_buffer_t out = {0};
mulib_webidl_error_t error;
mulib_webidl_options_t options = { MULIB_WEBIDL_SEXPR, MULIB_WEBIDL_AST };
int ok = mulib_webidl_compile(source, sizeof source - 1, &options, &out, &error);
/* On success, out.data[0..out.length) contains text, without a trailing NUL. */
mulib_buffer_free(&out);
```

Select `MULIB_WEBIDL_CODEYAML` or `MULIB_WEBIDL_C99` as `format`. Select
`MULIB_WEBIDL_CST` as `tree` only with `MULIB_WEBIDL_SEXPR`; other combinations
fail. NULL options selects S-expression AST. Empty input is valid. NULL source
is valid only for zero length. There is no filesystem access in the API: read a
file into a buffer, then pass its bytes. The
[runnable example](../examples/modules/webidl.c) also acts as a small file compiler:

```sh
cc -std=c99 -Iinclude examples/modules/webidl.c -o webidl
./webidl --ast input.idl
./webidl --cst input.idl
./webidl --yaml input.idl
./webidl --c99 input.idl > declarations.h
```

Returns 1 on success, 0 on failure. Append is atomic, including allocation and
backend failures: existing output bytes and length are unchanged. The caller
owns and frees the output buffer. No input or options are retained; input may
alias output. `error` is optional, is cleared on success, and otherwise contains
a borrowed static message, zero-based byte offset and one-based byte line and
column (CRLF counts as one newline). Allocation failures are diagnosed too.

## Supported syntax and validation boundary

Interfaces (including callback interfaces), dictionaries, namespaces, callbacks,
enums, typedefs, partial definitions, interface mixins and `includes` statements.
Members include fields, attributes, constants, constructors, operations and
special operations, stringifiers, iterable/async iterable, maplike and setlike.
Types include multiword numeric primitives, string and buffer types, named types,
nullable types, unions, `sequence`, `FrozenArray`, `ObservableArray`, `Promise`
and string-keyed `record`. Arguments support extended attributes, optional values,
defaults and variadics. Literals support integers (decimal/octal/hex), floating
point/exponents, booleans, strings, null, infinities, NaN, empty arrays and empty
dictionaries. WebIDL strings have no backslash escape processing: a backslash is
a literal character. Non-ASCII UTF-8 is supported in strings and comments;
identifiers use ASCII letters, digits, underscores and internal hyphens.

Extended attributes are preserved as balanced token/group trees, including
unknown vendor attributes. Their individual grammars and effects are not
interpreted. A leading underscore is removed from semantic names, as WebIDL's
identifier escape; concrete tokens retain it. Multiword type whitespace and
comments are normalized in the AST.

Malformed tokens, mismatched delimiters, missing terminators, misplaced defaults,
invalid optional/variadic argument ordering, invalid generic arity, duplicate
local names/enum values/full definitions and some invalid nullable types are
rejected. Operations may share names (overloads); partial definitions can share
their full definition's name. Cross-file linking, all reserved-name restrictions,
overload ambiguity, member-modifier legality, type compatibility/ranges of
defaults/constants and extended-attribute semantics are **not** fully validated.
Legacy `implements`, exceptions, arbitrary expressions and nonempty literal
arrays/dictionaries are not supported.

Limits per call: 16 MiB input, 262144 tokens (including trivia), 65536 AST nodes,
64 levels of syntactic nesting or C type expansion, 262144 C type expansions,
4194304 combined validation/name-resolution/cache comparisons, 64 MiB per output
staging buffer and 64 MiB total cached C type spellings. Hitting a
limit fails rather than truncating output. There is no mutable global state;
allocation uses standard `calloc`/`realloc`/`free`.

## Output schemas

AST nodes have a `kind`, an optional string `value`, and ordered children. Node
kinds correspond to grammar concepts, e.g. `dictionary`, `field`, `type`, `name`,
`argument`, `default`, `required`, `nullable`, and `extended-attributes`.
Defaults preserve their source literal spelling; enum values omit surrounding
quotes. Source locations and trivia are absent from semantic outputs.

For `dictionary P { required long x; };` the S-expression AST is:

```lisp
(webidl (dictionary (name "P") (field (required) (type "long") (name "x"))))
```

The CST is `(webidl-cst (TOKEN-KIND OFFSET "EXACT-TEXT") ...)`, with token kinds
`identifier`, `number`, `string`, `punctuation`, `whitespace`, and `comment`.
Concatenating the decoded token strings, in order, reconstructs the entire input
exactly. CST mode still parses and validates the grammar, not merely lexes it.
Quoted S-expression strings use JSON escaping. UTF-8 text remains UTF-8.

CodeYAML is this module's explicit YAML AST interchange schema (no external
CodeYAML dialect or runtime is assumed). Each node is a mapping with `kind`,
optional `value`, and `children` (always present, `[]` when empty). For example:

```yaml
kind: webidl
children:
  -
    kind: typedef
    children:
      -
        kind: type
        value: "long"
        children: []
      -
        kind: name
        value: "Count"
        children: []
```

## C99 mapping

Generated text includes only standard headers and declarations, with no
third-party/runtime dependency. Applications implement the declared functions
and manage all memory themselves. It is not a wire format or a stable ABI across
source revisions. It does not apply defaults, conversions, ownership policies,
attribute effects or inheritance dispatch automatically.

* All identifiers are prefixed `webidl_`; `_` becomes `_u`, `-` becomes `_h`.
  This injective encoding avoids C keywords and collisions after escaping.
* Interfaces and callback interfaces become opaque forward-declared structs.
  Named dictionaries also have forward declarations and are passed as pointers;
  their bodies contain typed fields and `bool has__webidl_FIELD` for non-required
  fields. Empty dictionaries have a dummy byte. Dictionary bases are explicit
  `webidl__base` pointers; interface bases affect neither layout nor dispatch.
  Base names must resolve to the same definition kind and be acyclic.
* Attributes generate `OWNER__webidl_NAME__get` and, unless readonly, `__set`.
  Operations generate `OWNER__webidl_NAME__call_N`, constructors `OWNER__new_N`.
  `N` is the zero-based member ordinal, including attributes/constants.
  Anonymous special operations omit the name segment. Non-static interface
  functions take a self pointer; namespace/static functions do not.
* Optional parameters get an `int webidl__has_N` presence parameter. Variadics
  become a typed pointer plus a `size_t` count, not C varargs. Argument names use
  their ordinal, avoiding collisions with generated parameters.
* Booleans use `bool`; byte/octet/short/long/long long use exact-width
  `intN_t`/`uintN_t`; float/double use C float/double. `void` and `undefined`
  returns use `void`. Numeric/boolean constants become typed macros.
* Strings use `webidl__string {const void *data; size_t length;}`. DOMString
  elements are UTF-16 units, USVString elements Unicode scalar `uint32_t`, and
  ByteString elements bytes; lengths count elements. `bigint` uses a decimal
  ASCII string span. No terminators are required.
* Enums use `const char *` and numbered `OWNER__value_N` string macros, with
  C escaping (including trigraph protection). Typedefs preserve C type identity;
  callbacks become function-pointer typedefs. Forward references are supported.
* Nullable types become `{int is_null; T value;}`. Sequences and array types
  become `{T *data; size_t length;}`. Records become parallel string-key and
  typed-value arrays plus length. Unions have a zero-based alternative `tag`
  and a C union with `value_N` fields. Helper typedefs are generated as needed.
* `any` is an application-defined `{void *data; size_t tag;}`;
  object/symbol are opaque pointers; Promise is an opaque promise pointer.
  Buffer and typed-array types use `{void *data; size_t length;}` byte spans.

The C backend requires all referenced named types (including Promise results) to
resolve. Cyclic typedefs/callback signatures, partials, mixins, includes,
iterable/maplike/setlike/bare stringifiers, nonnumeric constants and constants
with nonnumeric/nullable types fail explicitly. Merge declarations before C
lowering when using partials/mixins. Extended attributes remain metadata in the
semantic formats and have no binding behavior in C declarations.

## Compatibility: schema generation

`mulib_webidl_generate(schema, out)` is unchanged. It generates one flat WebIDL
dictionary from `mulib_dbschema_t`, not from source. Names use underscore
escaping; null-only fields fail, arrays map to `sequence<any>`, objects to
`object`. Append is atomic without a trailing NUL. See also
[schema mapping](storage-server.md#idl-and-native-output).

## Verification

`tests/test_webidl.c` covers all formats, grammar failures, source/output aliasing,
empty input, invalid UTF-8/options, recursion limits and allocation failure at
every allocation in each backend. `tools/check_webidl.py` compiles generated
output as strict C99 and verifies concrete-token roundtrips and YAML structure.
