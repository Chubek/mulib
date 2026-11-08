# MuLIB CSS declarations, rules and selectors

[Manuals](README.md) · [Library index](index.md)

**Header:** `include/mulib-css.h` · **Language:** ISO C11, C++17 declarations

## Supported scope

This module owns parsed declaration lists and ordinary stylesheet rules, and
matches a focused selector language against the shared XML/HTML DOM. It provides
an author cascade and explicit length conversion, without claiming full CSS
standards coverage or a layout engine.
It never follows `url(...)` or performs filesystem/network access.

Declarations support semicolon separation, comments, quoted/escaped strings,
balanced parentheses/brackets (nesting limit 64), and trailing case-insensitive
`!important` (whitespace between `!` and `important` is allowed). Property names
use `[-_A-Za-z][-_A-Za-z0-9]*`. Ordinary names are lowercased; names starting `--`
retain case. Empty values are rejected. Values are stored as trimmed text, not
interpreted or validated against property grammars. Comments outside strings
become spaces, preserving source offsets. Duplicate declarations are retained.

Stylesheets support `selector { declarations }` rules, preserving rule order.
At-rules (`@media`, `@import`, etc.), nested rules and bare statements are errors.
Input must contain XML-valid UTF-8 plus CSS form feed; embedded NUL and other
invalid controls fail.

Selectors support:

- Type names and `*`; `#id` and `.class` filters.
- `[attr]`, `[attr=value]`, `[attr='value']` and double-quoted equality values.
  Unquoted values use the identifier grammar above.
- Compound filters, whitespace descendant combinators, `>` child combinators,
  and comma-separated groups. Whitespace around attribute operators is allowed.

Names/IDs/classes in selectors use the ASCII identifier grammar above. Attribute
and type names, IDs/classes, and equality values compare **case-sensitively**;
there is no automatic HTML name folding. Classes are split on space/tab/CR/LF.
No namespace resolution, CSS identifier escapes, pseudo-classes/elements, sibling
combinators, attribute operators other than equality, or attribute flags.
Maximum 64 compounds across all groups in an expression. Unsupported syntax is
an error even if another comma branch already matched.

## Integration

Explicitly enable these implementations in one TU, before any include:

```c
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#include "mulib-css.h"
```

XML supplies DOM access and UTF-8 character validation. No second markup tree is
allocated; selectors evaluate ancestor chains with bounded-step dynamic
programming rather than recursive descendant backtracking.

## API and ownership

| API | Contract |
| --- | --- |
| `mulib_css_parse_declarations(source, length, error)` | Allocate an owned `mulib_css_style_t`; empty input is a valid empty list. |
| `mulib_css_style_destroy(style)` | Free a list and copied strings; NULL is harmless. |
| `mulib_css_count(style)` | Number of declarations including duplicates. |
| `mulib_css_name(style, i)` / `mulib_css_value(style, i)` | Borrow indexed name/value, or NULL out of range. Values exclude `!important`. |
| `mulib_css_important(style, i)` | Return important flag; zero out of range. |
| `mulib_css_get(style, name)` | Last important value if present, otherwise last ordinary value; NULL when absent. Ordinary property lookup ignores ASCII case, custom-property lookup is exact. This is declaration-local precedence, not a stylesheet cascade. |
| `mulib_css_parse(source, length, error)` | Allocate an owned `mulib_css_sheet_t`; empty sheet is valid. |
| `mulib_css_destroy(sheet)` | Free sheet, rules, selectors and declarations; NULL is harmless. |
| `mulib_css_rule_count(sheet)` | Number of ordered rules. |
| `mulib_css_selector(sheet, i)` | Borrow indexed selector or NULL. |
| `mulib_css_declarations(sheet, i)` | Borrow immutable indexed declaration list or NULL. Do not destroy separately. |
| `mulib_css_matches(node, selector)` | 1 match, 0 no match, -1 syntax/limit/allocation error. NULL node validates syntax and returns 0. Non-elements never match. |

Parse functions take explicit byte lengths, copy all retained strings, and
return NULL on malformed input, unsupported syntax, overflow or allocation
failure. Optional errors contain source byte offsets and static messages; success
clears them. Source may be released immediately after parsing. Accessors are
borrowed until destruction. Matcher source is NUL-terminated. No `errno` contract.

## Runnable example

```c
#include <assert.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#include "mulib-css.h"

int main(void) {
    const char xml[]="<main><p class='notice'>Hello</p></main>";
    const char css[]="main > p.notice { color: red; color: blue !important; }";
    mulib_xml_node_t *doc=mulib_xml_parse(xml,sizeof(xml)-1,NULL);
    mulib_css_sheet_t *sheet=mulib_css_parse(css,sizeof(css)-1,NULL);
    const mulib_css_style_t *style;
    assert(doc&&sheet);
    assert(mulib_css_matches(mulib_xml_first(mulib_xml_root(doc)),
                            mulib_css_selector(sheet,0))==1);
    style=mulib_css_declarations(sheet,0);
    assert(!strcmp(mulib_css_get(style,"COLOR"),"blue"));
    mulib_css_destroy(sheet); mulib_xml_destroy(doc);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/css.c -o ./mulib-css
./mulib-css
```

`tests/test_markup.c` exercises quoted delimiters/comments, duplicate/important
precedence, exact custom-property case, ancestry matching, malformed inputs,
randomized punctuation input and allocation-failure cleanup.
# Author cascade, lengths and roundtripping

`mulib_css_specificity(node,selector,&rank)` validates every selector branch and
returns 1 for a match, 0 for none, -1 for invalid syntax/allocation. The output is
the greatest `(ids, classes, types)` tuple among **matching** comma branches and
is unchanged on failure/no match. Counts are compared lexicographically rather
than packed into a small integer. The typed stylesheet module uses this API too.

`mulib_css_cascade(sheet,node,inline_style,error)` returns owned winning
declarations, one per property. Sheet and inline list may be NULL; node must be
an element. Importance precedes inline rank, then matching specificity and source
order; later equal-ranked declarations win. Important sheet rules beat ordinary
inline declarations. Ordinary property names compare ASCII-insensitively;
custom properties are case-sensitive. Pass an explicitly parsed style attribute
as inline_style; this function does not implicitly read it. Values remain raw:
inheritance, var() resolution and shorthand expansion are not performed. Use
`mulib-stylesheet` for the supported typed/inherited layout properties.

`mulib_css_serialize_declarations(style)` returns malloc-owned CSS, preserving
declaration order, duplicates and importance. Empty lists produce an allocated
empty string. Parsing the result reconstructs the declaration values.

`mulib_css_parse_length(value,&length)` parses signed decimals with px, pt, em,
rem or %, case-insensitively; unitless zero and unsigned `auto` are accepted.
Whitespace may surround the value. Exponents, unitless nonzero values, calc()
and unknown units fail. `mulib_css_length_pixels(length,em,rem,percent_base,&px)`
converts with 96 CSS pixels per inch (12pt = 16px), caller-provided font/reference
sizes and percentage basis. Auto has no numeric resolution. Bases must be finite
and nonnegative; signed lengths remain signed. Both return 0/-1 and leave output
unchanged on failure. Conversion is explicit, so a document renderer chooses its
own context for relative lengths.

CSS form feed is recognized as whitespace, including selector/class token
boundaries; literal newlines/form feeds in quoted strings are rejected.
`tests/test_html_css_pipeline.c` covers author precedence, nonmatching selector
branches, custom-property casing, unit conversion, roundtrips and allocation
failure cleanup.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-css.h`](../include/mulib-css.h) |
| Implementation switch | `MULIB_IMPL_CSS` |
| Repository-local includes | [mulib-xml](mulib-xml.md) |
| Complete example | [`examples/modules/css.c`](../examples/modules/css.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/css.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
