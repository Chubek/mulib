# mulib-docstring

[Manuals](README.md) · [Library index](index.md)

**Docstrings** is a structured documentation language and renderer. The single
public header is `include/mulib-docstring.h`; its compiler API uses the
`mulib_docstrings_` prefix. It defines DocstrIL, compiles markup or Lua into
that IR, runs Lua extensions/filters, applies stylesheets, and emits XML, HTML,
Pandoc JSON ASTs, token streams, event streams and a documentation DOM.
There are no third-party dependencies. Graphs and sequence diagrams render to
native SVG; syntax highlighting reuses MuLIB's Termsyn lexers.

## Integration

The [complete standalone example](../examples/modules/docstring.c) lists every
implementation switch. Define them in **one** implementation translation unit:

```c
#define MULIB_IMPL_DOCSTRING
#define MULIB_IMPL_TBL
#define MULIB_IMPL_CSV
#define MULIB_IMPL_TEX
#define MULIB_IMPL_DSDOM
#define MULIB_IMPL_TAS
#define MULIB_IMPL_XTAS
#define MULIB_IMPL_PANDOCAST
#define MULIB_IMPL_GVIZZ
#define MULIB_IMPL_DYEGRAM
#define MULIB_IMPL_XML
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_CSS
#define MULIB_IMPL_JSON
#define MULIB_IMPL_YAML
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_TERMSYN
#define MULIB_IMPL_EDBUF
#define MULIB_IMPL_TERMSTYLE
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TERMINFO
#include "mulib-docstring.h"
```

On Linux link `-lm -pthread -ldl`. The Lua implementation is exclusively
[Mune](mulib-mune.md); no upstream Lua library or executable is needed.

## Lifecycle and results

Create a reusable compiler with `mulib_docstrings_create(NULL)` for defaults.
Options are copied; callback userdata remains borrowed until compiler destruction.
`compile(compiler, source, length, format, error)` borrows the source for the call
and returns a new owned DocstrIL root, or NULL. Supported formats are
`docstrings` (also the NULL default), `markdown`, `asciidoc`, and `lua`.
Empty input succeeds; NULL is valid only at length zero. Inputs and IR text must
be XML-valid UTF-8 without embedded NUL. Lengths are bytes, excluding terminators.

Destroy the result with `mulib_xml_destroy` and the independent compiler with
`mulib_docstrings_destroy`, in either order. Destroy is NULL-safe and is a no-op
during an active operation. Objects require external synchronization. Public
operations on the same compiler cannot be reentered, including from host
callbacks. Different compilers have no shared mutable state.

Errors have a status and **owned** source/message arrays. Locations are byte
offsets and one-based line/column coordinates at the failing block/paragraph;
expanded fragments use local coordinates, and postprocessing/schema errors
usually use offset zero. Error pointers may be NULL. Compile never exposes a
partial tree. Language macro definitions are document-local, including definitions
from includes; Lua registrations and other Lua side effects persist, even after
an error. An already compiled IR is independent of the Lua state.

## DocstrIL

The IR schema and validation API are declared in this header itself. A document
is an XML-compatible owned semantic tree rooted at `<docstril version="1">`.
`mulib_docstril_t` and `mulib_dsdom_t` are the existing MuLIB XML node type, so
`mulib_xml_*` performs tree edits, attributes and destruction. Appending transfers
ownership only on success; detach returns ownership. Borrowed nodes/strings
remain valid only while their owners and the relevant storage remain unchanged.

Blocks include `paragraph`, `heading` (`level="1"` through `"6"`), `blockquote`,
`list` (`ordered="true"`/`"false"`), `item`, `code-block` (`language`), `table`,
`row`, `cell` (`header="true"`), `rule`, `figure`, `directive` (`name`, `argument`),
`toc`, `index` and `index-entry` (`term`, `id`). Inlines include `strong`,
`emphasis`, `underline`, `strike`, `superscript`, `subscript`, `code`, `link`
(`href`), `image` (`src`, `alt`), `span`, `break` and extension names. Text nodes
always contain literal text, not source markup. Figures contain actual SVG.
Extension attributes and element names are retained in XML/TAS/DOM outputs.

`mulib_docstril_validate` checks root/version, element/text-only nodes, headings,
table/list child shape, node/byte budgets and a hard depth of 96 edges. It does
not implement a closed schema: extension nodes are intentional. Zero limits
select 100000 nodes and 8 MiB of names, attributes and text. It returns 1/0 and
an optional static XML diagnostic; offset is a preorder node index.

## Source language

The native language combines a bounded Markdown-like block syntax with explicit
directives, macros and extensible inline elements. It is not a full CommonMark,
AsciiDoc or Doxygen implementation.

| Construct | Syntax |
| --- | --- |
| Paragraphs | Consecutive nonblank ordinary lines; blank line separates blocks |
| Heading | `# Title` through `###### Title`; optional trailing `{#explicit-id}` |
| Emphasis | `*italic*`, `_italic_`, `**bold**`, `__underline__`, `~~strike~~` |
| Scripts | `^superscript^`, `~subscript~` |
| Literal code | One or more matched backticks; no macro expansion inside |
| Link/image | `[label](https://example.org)` / `![alt](image.svg)` |
| Inline element | `@span[class="accent"]{nested **content**}` |
| Quote | Consecutive `> quoted` lines, recursively parsed (including `>>`) |
| Flat list | `- item`, `* item`, `+ item`, or `1. item` |
| Table | Pipe-delimited rows; optional dash/colon separator after first row |
| Rule | A standalone `---` |
| Code block | Triple-backtick fences with optional language, or `.code LANG` / `.endcode` |

Backslash escapes the next byte in inline source. Intraword underscores remain
literal. Unmatched ordinary style/code markers remain literal. Explicit malformed
macro calls, directives and element bodies produce diagnostics. Nested lists and
indented list items are deliberately rejected; use Lua/IR constructors for nested
lists. List items are single-line. Tables are rectangular, with one row per line;
escaped pipes and pipes inside code/element bodies do not split cells. Separator
rows mark the preceding first row's cells as headers. There are no cell spans.

Table scanning and validation are provided by [mulib-tbl](mulib-tbl.md).
Separator colons set left/center/right column alignment, preserved in DOM/streams
and emitted by HTML/Pandoc writers. `.table csv` or `.table pipe`, closed with
`.endtable`, imports a literal table; `ldocstr.table(source,format)` provides the
same reader to Lua extensions. The ordinary pipe syntax still parses rich inline
Docstrings in each cell. The other notation modules remain independent.

Builtin inline elements are `strong`, `emphasis`, `underline`, `strike`, `code`,
`span`, `link`, `image`, `superscript`, `subscript`, `break`, and `index-entry`.
Attributes use unique `key="value"` or `key='value'` pairs separated by whitespace.
Code element contents are literal. Unknown inline names require Lua registration.
Links parsed from bracket syntax accept http/https/mailto and relative URLs;
controls, whitespace and active schemes are rejected. Raw HTML source is text.

### Macros and builtins

```text
.define PRODUCT MuLIB
.define greet(who) Hello, **${who}**!
.macro warning(message)
> **Warning:** ${message}
.endmacro

# {{PRODUCT}}
{{greet(reader)}}
.use warning(Read the contract)
```

Object macros use `{{NAME}}`; function macros use `{{NAME(args)}}`, including
`NAME()` for zero arguments. Call form and arity must match. At most 16 unique
identifier parameters/arguments are supported. Commas outside balanced brackets
and quoted strings separate arguments. Quoted arguments unescape backslash
quotes, `\n` and `\t`; bare arguments are trimmed. `${parameter}` substitutes
inside the definition; unknown placeholders fail. Later definitions shadow earlier
ones. `.use` expands a macro as blocks; inline invocation reparses inline markup.
There are no language-defined functions or user-defined directive implementations:
use macros in source, and Lua for semantic extensions.

`{{eval(6 * 7)}}` evaluates a Lua expression in Mune. Strings from eval are
literal; scalars become text and node results become IR. `{{exec("command")}}`
requires an explicit host callback, and its output is always literal text.
Neither builtin invokes a system shell by default.

### Documentation directives and indices

Builtin `.file`, `.brief`, `.param`, `.return`, `.returns`, `.note`, `.warning`,
`.see`, `.author`, `.since`, `.deprecated`, `.version` and `.title` retain their
name/argument and parsed inline contents in a directive node. These do not infer
a programming-language symbol table. `.section ID Title` and `.subsection ID Title`
produce level-one/two headings. Unknown directives fail unless registered in Lua.

`.toc` inserts an automatically generated heading index. `.index TERM` creates
an anchor, `@index-entry{TERM}` creates a visible inline anchor, and `.indices`
inserts the term index. Heading IDs use ASCII slugs, with deterministic numeric
suffixes on collisions; explicit duplicate/empty/whitespace IDs fail. Terms sort
lexicographically by UTF-8 bytes. Repeated terms retain separate links. TOCs are
flat and preserve heading order. Filtering regenerates indices and link labels;
existing heading IDs remain stable even if their titles change.

### Graphs, diagrams and highlighting

`.graph` / `.endgraph` and `.diagram` / `.enddiagram` embed respectively
[GVizz](mulib-gvizz.md) graphs and [Dyegram](mulib-dyegram.md) sequence diagrams.
Fenced languages `graph` and `diagram` do the same. Example body:

```text
Client: User interface
Client -> Server: Request
Server -> Client: Response
```

This is the native labelled-edge language, not DOT, Mermaid or PlantUML.
Layout is deterministic and intentionally simple; long labels may overlap.
Code languages `c`, `cpp`/`c++`, `python`, `json`, and `sh`/`shell` use
[Termsyn](mulib-termsyn.md) lexical highlighting. Other language labels preserve
literal code without highlighting. Highlight spans keep complete source coverage.

## Embedding and host capabilities

`.include markdown "other.md"`, `.include asciidoc other.adoc`,
`.include docstrings other.docstr` and `.include lua authored.lua` call the
configured loader. It appends bytes to a borrowed strbuf and returns nonzero on
success. The library does not read arbitrary files, resolve relative paths, or
fetch URLs: host userdata supplies path resolution and authorization. Includes
share macro definitions, have bounded depth, and reject repeated active path
strings; canonical path/cycle policy beyond literal names belongs to the host.

The Markdown profile supports the block/inline constructs above, but `__text__`
means strong, and native directives/macros/`@elements` are **not executed**.
There are no raw HTML, reference links, nested lists, Setext headings or full
CommonMark delimiter rules. The AsciiDoc profile adds `= Title` headings,
single-asterisk strong text, `[source,c]` plus `----` code fences, `|===` tables
with one row per line, and `include::path[]` using the host loader. Block
attributes other than `[source,LANG]` fail. Attribute declarations/substitution,
conditional directives, passthroughs and other full-AsciiDoc features are outside
this profile and are not interpreted. Unknown ordinary markup stays literal.

## Lua extensions and documentation

The sources in [library/ldocstr](../library/ldocstr/README.md) are embedded in the
header and automatically installed as globals `ldocstr` and `ldocstr.style` in
each compiler's lazily created Mune state. There is no runtime working-directory
dependency and no `require` is needed. `require('ldocstr')` and
`require('ldstr-style')` are also supported for Lua-authored documents.

```lua
ldocstr.element('kbd', function(content, attrs)
  attrs.inline = 'true'
  return ldocstr.Element('kbd', content, attrs)
end)
ldocstr.directive('todo', function(argument)
  return ldocstr.Para('TODO: ' .. argument)
end)
ldocstr.macro('upper', function(text) return ldocstr.Str(text:upper()) end)
```

Pass registration scripts to `mulib_docstrings_extend`, or use `.lua` / `.endlua`
blocks. Block scripts may also return a node/list/string to emit. Lua document
format executes a chunk returning `ldocstr.Doc({...})`, a node/list, or source
text. For example, `return ldocstr.Doc({ldocstr.Header(1,'Title'),
ldocstr.Para('Written in Lua')})`. `mulib_docstrings_lua` borrows the underlying
state for explicit host bindings/mode selection; never destroy that state yourself.

IR nodes are ordinary Lua tables: `{t='paragraph',attrs={},children={...}}` and
`{t='Text',text='literal'}`. Constructors include Doc, Element, Text/Str, Para,
Header, Strong, Emph, Underline, Strikeout, Code, CodeBlock, Link, Image, Span,
Div, BlockQuote, BulletList, OrderedList, Item, Table, Row, Cell, TOC and Index.
An element extension receives parsed child nodes and attributes; a directive
receives its argument; a Lua macro receives separate string arguments. Return
nil for no emitted content, a node, a dense node list, or markup text. Builtin
directives/elements/macros are not overridden by registry entries.

## Lua filters

`mulib_docstrings_filter` runs a chunk that returns one of these contracts:

| Mode | Chunk result and invocation |
| --- | --- |
| `walk` (default) | Table of postorder handlers, e.g. `{Str=function(n) ... end}` |
| `document` | Function receiving/returning the whole IR document |
| `tokens` | Function receiving/returning a TAS token array |
| `events` | Function receiving/returning an XTAS-style event array |

Walk handlers can use canonical element names or Pandoc-like aliases such as
Str, Para, Header, Strong and CodeBlock. `Element` is a fallback handler. Returning
nil keeps mutations to the node; false deletes it; a dense list splices replacements.
The root must remain a valid DocstrIL document. This is Pandoc-like traversal,
not upstream Pandoc userdata/API compatibility. Document functions may return nil
to keep their mutated snapshot. Token/event functions must return their arrays.

All modes operate on detached Lua snapshots. Only a validated result replaces
and destroys the old C tree. On failure `*document` and its nodes are unchanged;
Lua state side effects persist. Cycles, sparse arrays, invalid attributes, wrong
return types, malformed token/event balance and excess work/depth fail boundedly.
Lua tables/strings are copied to owned C nodes; no Lua pointer escapes into IR.

## Stylesheets

`.style css` / `.endstyle` embeds rules. Formats `json` and `yaml` map selectors
to declaration maps. `.stylesheet FORMAT path` loads one of those through the
explicit host loader. Embedded/file sheets combine in document order and apply
**after** IDs and indices are generated. Example YAML:

```yaml
heading:
  color: navy
  font-weight: bold
'.key':
  border: 1px solid gray
```

CSS supports [MuLIB CSS selectors/cascade](mulib-css.md): types, classes, IDs,
attributes, child/descendant combinators, groups, specificity and `!important`.
Resolved declarations become `style` attributes; values are preserved for output,
not interpreted as a full layout engine. JSON/YAML values must be strings or
numbers, with no declaration separators/braces. There are no CSS at-rules,
imports, network fetches or implicit inheritance expansion. Matching uses the
unmodified tree in a two-phase pass, so materialized styles cannot change later
selector matches within that pass.

`mulib_docstrings_style` atomically styles an existing tree. Format `lua` is a
document-filter chunk; `return ldocstr.style.filter({heading={color='navy'}})`
creates a Lua stylesheet through the automatically installed
`library/ldocstr/ldstr-style.lua`. `style.apply`, `style.css` and `style.set`
are also available. `style.filter` defers application until generated indices
are refreshed; `style.apply` styles its current snapshot immediately. Reapply
immediate snapshot styles if a later transform replaces those styled nodes.
A later explicit styling operation treats previously
materialized styles as inline declarations, honoring their priority.

## Outputs

`mulib_docstrings_render` returns malloc-owned NUL-terminated `xml` (default),
`html`, or `pandoc` JSON, or NULL with a diagnostic. Its optional length excludes
NUL and is zero on failure. XML is the semantic DocstrIL serialization. HTML maps
known semantic nodes to elements, includes SVG and highlighting spans, escapes
text/attributes and excludes event-handler attributes. Unknown nodes use Div/Span
wrappers; `kbd` has a native HTML mapping. This is **not** a general-purpose HTML
or CSS sanitizer; validate untrusted extensions/styles according to host policy.

Use `mulib_tas_from_dom`, `mulib_xtas_stream`, `mulib_dsdom_clone` and
`mulib_pandocast_from_docstril` for structured C outputs. Lua exposes `parse`,
`xml`, `html`, `render`, `dom`, `pandoc`, `tokens`, `events`, `from_tokens`, and
`from_events`. `dom` is an independent table copy. Pandoc output is an ordinary
Lua AST table; JSON null positions use the singleton `ldocstr.null` rather than
nil. TAS entries have `kind`, `name`, `value`; XTAS entries have `kind`
(enter/text/leave), `name`, `attrs`, `text`, `depth`. Reverse conversion validates
balance; derived depth fields are informative, not trusted for reconstruction.

## Limits and safety

Zero options select 1 MiB per source fragment, 8 MiB cumulative generated bytes,
100000 constructed nodes, 1000000 work units, parser/expansion depth 64 and
include depth 16. Nonzero values can lower but not raise these limits. Final IR
validation separately counts all nodes/bytes, including generated SVG. Rendering
can expand escaped serialization beyond the tree's byte count. Compiler-side
work includes macro scans and stylesheet matching; complex documents can hit
the work limit before a storage bound. ID uniqueness checking is quadratic in
element count but bounded by that work budget.

Mune has separate configurable heap/step/call/result limits. Parser AST/compiler
storage is outside its runtime heap budget. Default states remove io, os,
package, debug, dynamic source/file loading, print and warn; restricted require
only exposes ldocstr. `MULIB_DOCSTRINGS_TRUSTED_LUA` restores full Mune standard
capabilities. Capability reduction and resource limits are **not** a hardened
security sandbox. Host bindings, loaders and exec callbacks must enforce their
own authorization, memory and execution policy. Callback buffers are checked
after return; the host must bound allocations while producing them.

Evidence: [functional/hostile cases](../tests/test_docstring.c),
[moving-allocation/failure injection](../tests/test_hardening.c),
[optional real-Pandoc/XML interoperability](../tools/check_docstring.py), and
the runnable per-module examples. `tools/embed_ldocstr.py --check` verifies the
embedded default modules; edit their Lua sources, not the generated include.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-docstring.h`](../include/mulib-docstring.h) |
| Implementation switch | `MULIB_IMPL_DOCSTRING` |
| Repository-local includes | [mulib-css](mulib-css.md), [`include/mulib-docstring-data.inc`](../include/mulib-docstring-data.inc), [mulib-dsdom](mulib-dsdom.md), [mulib-dyegram](mulib-dyegram.md), [mulib-mune](mulib-mune.md), [mulib-pandocast](mulib-pandocast.md), [mulib-tas](mulib-tas.md), [mulib-tbl](mulib-tbl.md), [mulib-termsyn](mulib-termsyn.md), [mulib-yaml](mulib-yaml.md) |
| Complete example | [`examples/modules/docstring.c`](../examples/modules/docstring.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/docstring.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
