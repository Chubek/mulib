# `std.html`: owned static HTML snapshots

`std.html` is independent of HTTP, Elvi, daemon, and any browser. It parses
bounded UTF-8 input into a nominal immutable `BasisHtml` (also exported as
`HTML`), serializes it, and
supports DOM navigation, text extraction, restricted XPath, and read-only
visitors. It does not perform I/O or execute scripts.

```on1x
import std.html as H;

fn main()
{
  match H.basis_html_parse("<UL><LI>Café<LI>Open studio</UL>") with
  {
    Ok(doc) =>
    {
      let (status, html) = H.basis_html_serialize(doc);
      assert(status == 0);
      println(html);
    },
    Err(status) => println(status)
  };
}
```

## Parsing profiles

All constructors return `Result<BasisHtml, i64>`. They accept at most 65,536
input bytes (decoded bytes for hex) and require XML-compatible UTF-8, not
arbitrary bytes, NUL, or invalid controls. No encoding sniffing or
legacy charset transcoding takes place. Canonical markup is limited to
131,072 bytes; the final tree has at most 1,024 nodes (including the owning or
synthetic root) and 48 element levels.

| Operation | Profile |
| --- | --- |
| `basis_html_parse(source)` | Bounded tolerant static document parsing. |
| `basis_html_parse_hex(body_hex)` | Same document profile, starting from exact case-insensitive hexadecimal bytes. Useful when transport supplies a byte artifact; odd length, nonhex digits, and decoded NUL are rejected. |
| `basis_html_parse_strict(source)` | MuLIB explicit-tag document parser, requiring an `html` element; optional HTML doctype. |
| `basis_html_parse_fragment(source)` | MuLIB explicit-tag fragment parser, keeping a synthetic `div` traversal root and excluding that wrapper from serialization. |

The tolerant profile uses the actual MuLIB
[textstream](../../../manual/mulib-html-textstream.md),
[tokenizer](../../../manual/mulib-html-tokenizer.md) and
[treebuilder](../../../manual/mulib-html-treebuilder.md) before normalizing
through [`mulib_html_parse` / `mulib_html_serialize`](../../../manual/mulib-html.md).
It repairs repeated active `li`, `p` and `option` nodes, unwinds to matching end
tags, ignores unmatched end tags, and closes remaining nodes at EOF. It accepts
the MuLIB tokenizer's bounded tag spelling, quoted/unquoted attributes, and
boolean attributes. Limits on the intermediate tree (2,048 nodes and the
treebuilder's event budget) apply before normalization.

With an explicit top-level `html`, only whitespace outside it is accepted after
the lexical omissions below. Without one, immediate `head`/`body` tags are
wrapped in `html` (no other nonwhitespace top-level content); otherwise content
is wrapped in `html/body`. `head` and `body` may occur at most once, in order,
directly under `html` when present. The profile does not guess browser insertion
points or compensate for missing `</head>` before `<body>`.

Tolerant parsing drops comments and declarations classified by the tokenizer
and drops complete `script` and `style` subtrees. Unterminated raw regions and
unterminated tags/comments fail. The profile escapes stray `&` and literal `<`
and preserves unknown named references as literal text; only `amp`, `lt`,
`gt`, `quot`, `apos`, `nbsp` and valid semicolon-terminated numeric references
are decoded. Duplicate folded attributes, non-void slash closing, foreign
`svg`/`math`, and unsupported raw-text elements such as `iframe` fail. This
is not WHATWG tree construction: no foster parenting, formatting adoption,
full character reference table, template semantics, or arbitrary
error recovery. The tolerant examples are qualified under the C locale.

The strict profiles follow the explicit-tag MuLIB grammar, including its
entity subset and validated comments. Neither profile is lossless source
storage: case, references, attribute quoting and doctype are canonicalized,
and tolerant parsing makes the omissions listed above. **This is not a
sanitizer.** For example, event-handler attributes and active URLs may remain.
Do not insert serialized untrusted input into an active page on the strength
of this API.

## Serialization, navigation and XPath

`basis_html_serialize(doc)` returns `(status, markup)`. Document serialization
begins with `<!doctype html>\n`; fragment output simply concatenates the
synthetic root's children. Canonical here means the deterministic serializer
for the qualified profile, not a byte-identical copy, a browser DOM, or a content
hash insensitive to attribute order.

- `basis_html_root(doc)` returns `BasisHtmlNode`. `basis_html_node_kind(node)`
  returns `(status, kind)`: document `0`, element `1`, text `2`, comment `3`
  (`4` is reserved for the kernel's CDATA kind, which these parsers do not
  create). `basis_html_node_name` reads element names;
  `basis_html_node_value` reads a leaf's stored content.
- `basis_html_node_parent`, `basis_html_node_first_child`, and
  `basis_html_node_next_sibling` return `Result<Option<BasisHtmlNode>, i64>`;
  `Ok(None)` is a traversal boundary, not an error. Child and sibling order
  includes text and comments present in the canonical snapshot.
- `basis_html_node_attr(node, name)` does a case-sensitive lookup (parsed
  attribute keys are lowercase). `basis_html_node_attr_count`,
  `basis_html_node_attr_name` and `basis_html_node_attr_value` support
  insertion-ordered, zero-based attributes. Empty/boolean value is a
  successful empty string; missing value is `-4`.
- `basis_html_node_serialize` serializes the subtree. `basis_html_node_text`
  concatenates descendant text and turns `br` into LF; it ignores comments
  and does not collapse whitespace or compute styles.
- `basis_html_xpath(doc, expression)` and `basis_html_node_xpath(node, expression)`
  return `Result<BasisHtmlSelection, i64>`; inspect with `basis_html_selection_count`
  and `basis_html_selection_get(selection, zero_based_index)`. Successful
  selections may be empty; nodes are unique and in document order. This is
  [MuLIB XPath](../../../manual/mulib-xpath.md), not a full XPath standard:
  child/descendant and self/parent steps; name, `*`, `node()`, `text()`, and
  `comment()` tests; positional and supported attribute/text predicates.
  For example, `//li[1]` selects the first matching child per parent, not the
  first global match. Absolute paths need a document; use `.//li` on a fragment
  root. This companion additionally caps expressions at 1,024 bytes, 16
  unquoted slash separators, and one descendant `//` axis to bound work.

`basis_html_walk(node, fn(BasisHtmlNode) -> bool)` visits the subtree in
preorder, including the supplied node. It returns `(status, completed)`;
`false` from the visitor stops successfully with `completed = false`.
`basis_html_fold(node, initial, fn(sexpr, BasisHtmlNode) -> sexpr)` returns
`Result<sexpr, i64>` and threads an owned accumulator in the same order.
These are local On1x callbacks, not C callbacks, tree mutation, or durable
closure IDs. Capturing callbacks and independent queries in callbacks were
tested with the C11 emitter.

Status codes: `0` success; `-1` invalid arguments, encoded text, framing, or
XPath syntax; `-2` explicit budget exceeded; `-3` MuLIB rejection or native
allocation failure (some kernel entry points combine these); `-4` missing
content or selection index where expected; `-5` inconsistent companion reply.
String/number fields in failing scalar tuples are `""`/`0`. `string` tuples
also work on the compiler profile that disallows `string` as a nominal
payload.

## Ownership, trust and linking

`BasisHtml` keeps canonical markup in an immutable shared On1x S-expression
string atom. A node or selection retains its own copy of the document
reference together with document-local preorder ordinals. Dropping the source
string, document variable, selection variable, or a separate parse cannot
invalidate it. There is no raw native owner, pointer in an integer, global
last-result slot, manual close, or mutable live DOM. Native operations rebuild a
private bounded DOM from the snapshot; there are no retained views into it.
Ordinals should not be reused against another snapshot. Record constructors
are not opaque on this compiler; all native operations revalidate the input
syntax and check node bounds, but record types are not provenance or a trust
boundary. No HTML-level authority is conveyed.

`_Trusted.on1x` declares two explicit raw C11 interfaces implemented in
`runtime.c` / `runtime.h`, with no automatic safe-FFI claim:

| Raw extern | ABI and obligations |
| --- | --- |
| `on1x_html_parse_v1` | Borrow a readable byte range of the stated signed length, followed by NUL; the On1x caller uses `text_len`. Supplies profile/hex selectors, not addresses. Owns and frees its MuLIB token tree, XML tree and serializer buffers. |
| `on1x_html_node_v1` | Borrow NUL-terminated snapshot/argument text, validate signed selectors and reconstruct the private tree. XPath results own only their native pointer array; it is freed before its owning XML document. Only copied bytes or ordinals cross back. |

Both return a NUL-terminated On1x text snapshot or static error (`+payload`
or `-[1–5]`); never free it from C or retain input addresses. The companion
releases all native objects before invoking the On1x text allocator; the
runtime itself may exit on managed allocation or pool exhaustion. On1x
reference-counts S-expression nodes, but its text snapshots remain in its
bounded process pool until exit (64 MiB by default); this is not an unbounded
long-lived cache. Keep calls on the qualified local runtime lane, not
concurrent foreign threads, and allow normal native stack space (at least
512 KiB; large bounded bridge staging buffers are stack-local).

For a standalone generated C11 consumer, link one MuLIB implementation TU:

```sh
on1xc --emit-c consumer.on1x -o consumer.c
cc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -Ion1x/runtime -Ikernels consumer.c on1x/runtime/on1x_runtime.c \
  on1x/stdlib/html/runtime.c on1x/stdlib/html/kernels.c -lm -o consumer
```

When composing with other companions, omit `html/kernels.c` and provide those
switches in the host's single shared implementation TU. Do not link multiple
`MULIB_IMPL_*` copies. The standalone Linux C11 test is
`python3 -B on1x/stdlib/html/tests/run.py /path/to/on1xc /usr/bin/clang`.
It compiles real On1x programs and covers realistic offline HTML,
strict/fragment canonicalization, traversal, relative and absolute XPath,
lifetime/capturing visitors, malformed and oversized input, byte-preserving
hex validation, native allocation failures, and cleanup before fatal On1x
snapshot allocation. It does not qualify other backends, Chisel/Wetstone
foreign-owner imports, or browser conformance.
