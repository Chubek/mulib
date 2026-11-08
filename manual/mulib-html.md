# MuLIB explicit-tag HTML parser and builder

[Manuals](README.md) · [Library index](index.md)

**Header:** `include/mulib-html.h` · **Language:** ISO C11, C++17 declarations

## Scope and dependencies

Parse and build escaped HTML with the owned XML DOM. The parser implements a
bounded explicit-tag subset, without the HTML5 browser tree-construction/error
recovery algorithm. Do not feed arbitrary HTML to the XML parser and expect HTML
semantics. The HTML serializer understands void elements,
explicit end tags, document doctypes and escaped ordinary text/attributes.

Tag/attribute names must start with a lowercase ASCII letter and then contain
only lowercase letters, digits, `-`, `_`, or `:`. This supports ordinary HTML and
custom elements, not an SVG/MathML foreign-content serializer. Use `mulib-svg.h`
for SVG. CDATA nodes serialize as ordinary escaped text, not CDATA markup.

Void elements are `area base br col embed hr img input link meta param source
track wbr`. They serialize without closing tags and must have no children.
`script style xmp iframe noembed noframes plaintext noscript` are rejected: their
browser parsing contexts do not obey ordinary text escaping. HTML-specific
hazardous comment spellings also fail serialization. This is not a content-model
validator; callers must build sensible HTML. Title/textarea children must be
text/CDATA. The builder rejects adding elements to void/raw/text-only parents.

Implementation switches, explicitly defined in exactly one TU:

```c
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_HTML
#include "mulib-html.h"
```

Escaping, node/attribute allocation and lifetime management come from XML.
The HTML parser decodes its supported entity subset into that shared DOM.

## Parsing and text extraction

`mulib_html_parse(source,length,fragment,error)` returns an owned tree or NULL.
Fragment mode (1) returns a synthetic `div`; document mode (0) requires an `html`
root, optionally preceded by a case-insensitive `<!doctype html>`. Surrounding
document whitespace is discarded. Destroy with `mulib_xml_destroy`.

The supported grammar includes explicit matching end tags, ASCII case folding
for tag/attribute names, void elements, comments, single/double-quoted or
unquoted attribute values, and boolean attributes (empty values). Duplicate
attributes are rejected after case folding. Self-closing syntax is accepted only
on void tags; non-void elements require end tags. Unknown/malformed references
fail; supported names are case-sensitive `amp lt gt quot apos nbsp`, plus decimal
and hexadecimal numeric references with semicolons. Numeric values must be
XML-compatible Unicode scalars. CRLF/CR normalize to LF in literal input;
references preserve their decoded characters. Initial LF is stripped from
pre/textarea/listing, including entity-produced LF, matching serialization.

Raw-text elements, SVG/MathML, processing instructions, external doctypes,
implicit closures and malformed nesting fail with a byte offset and static
message. RCDATA-like title/textarea accept escaped text and their closing tag;
nested markup and comments are rejected. This is an explicit-tag document
grammar, not validation of all HTML content-model constraints. Input must be
XML-compatible UTF-8, maximum 16 MiB, 128 element levels, and one million parser
tokens. Allocation failures clean up the partial tree.

`mulib_html_text(node,&length)` returns owned concatenated descendant text/CDATA,
ignores comments, and translates `br` into LF. It does not collapse whitespace,
insert block separators or evaluate CSS. This provides input for `mulib-layout`;
applications can instead traverse styled blocks and lay out each separately.

## API and ownership

| API | Contract |
| --- | --- |
| `mulib_html_document(title)` | Allocate an XML document containing `html/head/title` and `body`. NULL title means empty. Caller owns the document. |
| `mulib_html_body(document)` | Borrow the immediate body child of an html-rooted document, or NULL. |
| `mulib_html_add(parent, tag, text)` | Append a named element and, if non-NULL, a copied text child. Returns a borrowed element; NULL means no change to parent. Use NULL text for void elements. |
| `mulib_html_escape(out, text, length, attribute)` | Forward to shared XML escaping; 1/0. Never use in raw-text/script/style contexts. Source must not alias the destination buffer. |
| `mulib_html_serialize(node, length)` | Allocate NUL-terminated HTML. Documents require an `html` root and receive `<!doctype html>\n`; other nodes are fragments. |

Use XML APIs for attributes, child traversal, detachment and destruction. All
content must be XML-valid UTF-8; this deliberately excludes embedded NUL and
invalid controls. Serializers enforce XML's 128-element nesting limit. NULL
returns signal invalid arguments/content, depth overflow or allocation failure;
lengths exclude NUL and are zero on failure. Free output with `free`.

For `pre`, `textarea` and legacy `listing`, a leading LF is serialized with one
extra LF because HTML parsers discard the first LF immediately after the start
tag. Empty initial text/CDATA nodes are skipped when deciding whether padding
is needed; a preceding element/comment prevents padding. This preserves content
such as `"\nx"` after HTML parsing without changing the DOM.

**Escaping is not sanitization.** Event-handler attributes, dangerous `href`/`src`
schemes and application-specific attribute policies remain the caller's concern.
The builder prevents text from becoming ordinary markup; it is not a safe policy
layer for arbitrary user-supplied tags/attributes, CSS or URLs.

## Runnable example

```c
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_HTML
#include "mulib-html.h"

int main(void) {
    mulib_xml_node_t *doc=mulib_html_document("A & B");
    char *html;
    assert(doc);
    assert(mulib_html_add(mulib_html_body(doc),"p","<escaped> & text"));
    assert(mulib_html_add(mulib_html_body(doc),"br",NULL));
    html=mulib_html_serialize(doc,NULL);
    assert(html&&strstr(html,"<p>&lt;escaped&gt; &amp; text</p><br>"));
    free(html); mulib_xml_destroy(doc);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/html.c -o ./mulib-html
./mulib-html
```

`tests/test_markup.c` and `tests/test_html_css_pipeline.c` verify escaping,
parsing/serialization, malformed inputs, cascade integration and injected
allocation failures.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-html.h`](../include/mulib-html.h) |
| Implementation switch | `MULIB_IMPL_HTML` |
| Repository-local includes | [mulib-xml](mulib-xml.md) |
| Complete example | [`examples/modules/html.c`](../examples/modules/html.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/html.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
