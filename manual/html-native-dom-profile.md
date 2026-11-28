# Bounded native HTML documents

`mulib_html_dombridge_parse` parses a whole trusted UTF-8 input into one
`mulib_dom_document_t`. The tree builder appends native nodes and attributes
directly. It never constructs a `mulib_html_tree_node_t` and never calls the
legacy walker, executes markup, or asks for resources. The legacy tokenizer,
tree, walker, query, and range APIs remain separate and unchanged.

## Supported markup

- A single optional legacy-free `<!doctype html>` before the document element
  (case-insensitive, no public/system identifiers). Optional explicit `html`,
  `head` and `body` tags with attributes; omitted containers are created.
  Comments are native Comment nodes. Only ordinary `<!--…-->` comments with
  no internal `--` or abrupt opening are accepted.
- Head: `base`, `link`, `meta`, `style`, `title`. Style text is literal; title
  is parsed as RCDATA (character references decoded, inner tags kept as text).
  An explicit matching closing token is required for either.
- Body: `address`, `article`, `aside`, `blockquote`, `div`, `footer`, `header`,
  `main`, `nav`, `section`, `h1`–`h6`, `pre`, `p`, `ul`, `ol`, `li`, `dl`, `dt`,
  `dd`, `hr`, `a`, `b`, `br`, `code`, `em`, `i`, `img`, `input`, `small`, `span`,
  `strong`, `sub`, `sup`, `textarea`, `u`, `wbr`. `textarea` requires an explicit
  matching end tag and uses RCDATA. `pre` and `textarea` discard an immediate
  leading LF. Void elements may use `/>`; non-void elements may not.
- Selected implied ends: an open top-level-in-stack `p` closes before a
  supported block start; a `li`, `dt`, or `dd` closes before a peer in its
  list. These optional elements may also close at an enclosing end tag (for
  example, `<ul><li>one<li>two</ul>`), or when they are the only remaining
  open body elements at EOF. List and description items need the corresponding
  immediate list parent. Other non-void body tags must be closed in order.
  Nested links and blocks directly inside phrasing elements, headings, or
  `pre` are rejected.
- Tag and attribute names are ASCII and lowercased. Name characters are
  `[A-Za-z0-9_-]`; tag names must start with a letter, attributes
  with a letter or `_`. Attributes must be whitespace-separated and may be
  valueless, quoted, or unquoted. `xmlns` and prefixed attributes are not
  supported. First duplicate wins. Elements use the HTML namespace; parsed
  attributes have no namespace.
- An initial UTF-8 BOM is ignored; CRLF/CR becomes LF. Invalid UTF-8 and
  literal NUL fail. Data, RCDATA and attribute values use the existing
  [staging entity table](../kernels/mulib-html-entities.h), including its
  numeric remapping and attribute ambiguity handling. Comments and style
  text do not decode references. No other character encodings are sniffed.

Anything outside this profile is not offered as recovered WHATWG HTML. In
particular, there is no script tokenization/execution, full parse-error
recovery, formatting reconstruction/adoption agency, table foster parenting,
template/foreign content, quirks mode, incremental parsing, fragments,
custom-element callbacks or browser side effects. This is **not** a sanitizer:
URL text, inline event attribute text, and style text may still be present in
the native tree. Do not pass them to an active environment as trusted output.

## Limits, ownership and linking

Pass explicit `max_bytes` (before BOM/line-ending normalization), `max_nodes`
(including the Document, synthesized elements, attributes, text, doctype and
comments), and `max_depth` (Document = 1, an attribute uses its element's
depth, text/comments count as children). Limits also obey the native DOM byte,
node and depth caps. A single token accepts at most 64 attribute occurrences
and a tag/attribute name at most 63 bytes. A zero byte cap permits only empty
input; zero node or depth caps cannot construct a document.

`MULIB_HTML_DOMBRIDGE_OK` (zero) publishes a new document through `*out`.
Destroy it with `mulib_dom_document_destroy`; source bytes need not survive
the call. Every error leaves `*out` unchanged and destroys any unpublished
tree: invalid argument, malformed input, unsupported profile, limit, or
allocation failure. No existing tree is modified. Allocation is by the C
allocator; separate calls share no mutable parser state.

In one C11 implementation translation unit, define
`MULIB_IMPL_HTML_DOMBRIDGE_NATIVE`, `MULIB_IMPL_DOM`, `MULIB_IMPL_UTF8` and
`MULIB_IMPL_BUFFER` before including `mulib-html-dombridge.h`. This opt-in
switch is distinct from `MULIB_IMPL_HTML_DOMBRIDGE` (the legacy walker). The
native parser does not require the legacy tokenizer or tree builder
implementation switches. The staging entity decoder is inline and does not
add a linker dependency or require Python at runtime.

## Query and live ranges

The [native DOM](mulib-dom.md) exposes a bounded descendant-only first match
against `*` or one ASCII type, `#id` or `.class`, evaluated on current
attributes. It also exposes read-only UTF-16 range text with explicit scan and
output bounds. Both use the same nodes returned by the parser. An isolated,
deterministic fixture verifies native mutation, range tracking, source
ownership, exact limits, and individual allocation failures on one parse path:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels \
  tests/test_html_native_dom.c -o test-html-native-dom
./test-html-native-dom
```

[Standalone example](../examples/modules/html-native-dom-profile.c).
