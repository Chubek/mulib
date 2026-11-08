# Terminal syntax-highlighting backend

[Manuals](README.md) · [Library index](index.md)

**Header:** `mulib-termsyn.h`. In one TU define `MULIB_IMPL_TERMSYN`,
`MULIB_IMPL_EDBUF`, `MULIB_IMPL_TERMSTYLE`, `MULIB_IMPL_CSS`, `MULIB_IMPL_XML`,
`MULIB_IMPL_TERMINFO`, `MULIB_IMPL_VTERM`, `MULIB_IMPL_GRID`, `MULIB_IMPL_UTF8`,
and `MULIB_IMPL_STRBUF` before inclusion. No third-party dependencies.
[Runnable editor/theme example](../examples/modules/termsyn.c).

## Lexing and incremental updates

Create a built-in cache for PLAIN, C, CPP, PYTHON, JSON or SHELL, or use
`mulib_termsyn_create_custom(lexer, user)`. `update(cache, buffer)` snapshots any
`edbuf` backend. It owns the snapshot and token cache, borrowing the buffer's
identity; the buffer must outlive its association with the cache. No concurrent
buffer edits are allowed during update.

Each LF-delimited line stores its input/output uint64 lexical state and local
spans. Updates reuse unchanged prefix lines, align unchanged suffix lines across
insertions/deletions, and relex until suffix entry states agree. Removing a block
comment/string delimiter therefore propagates correctly through later lines.
Repeated identical-revision updates lex zero lines. Multiple edits or undo/redo
between updates work; no single-edit notification is required.

Snapshot copying and line indexing are **O(document bytes)** per changed update;
incrementality saves lexing work, not the snapshot copy. `lexed_lines` reports
actual lexer calls on the last successful update. Failure preserves the complete
previous cache and revision. There is no background worker or event-loop policy.

Tokens are TEXT, KEYWORD, TYPE, STRING, NUMBER, COMMENT, OPERATOR, PUNCTUATION,
PREPROCESSOR, CONSTANT, IDENTIFIER and ERROR. Spans are byte ranges with complete
nonoverlapping document coverage, never zero length. `span_count`/`span` enumerate
absolute spans; `at` finds the token covering a byte. Empty lines have no spans;
EOF has no covering span. Indexed enumeration scans line metadata; `at` uses
binary line lookup followed by a line-local span scan.

## Built-in lexical profiles

* C/C++: identifiers, keyword/type lists, numbers, escaped strings/characters,
  line/block comments and leading-# preprocessor lines (including backslash-LF
  continuation). Strings with escaped LF can continue across lines.
* Python: keyword/type/constant lists, # comments, escaped single/double strings
  and multiline triple quotes. Prefixes, raw/f-string semantics and interpolation
  are not parsed.
* JSON: double-quoted strings, numbers and true/false/null. It colorizes, not
  validates, JSON; malformed literals do not constitute a JSON parser result.
* Shell: keywords, # comments, quotes including multiline quotes, and simple
  `$name`/`${name}` coloring. Here-documents, nested substitutions and complete
  shell quote/comment grammar are not implemented.

These are deterministic **lexical colorizers, not complete language parsers**.
Numbers use a permissive alphanumeric/exponent scan; names accept high bytes,
not a full Unicode identifier database. Unterminated ordinary non-shell strings
are ERROR tokens, not a syntax diagnostic API. Arbitrary bytes, including NUL,
are safe to tokenize; rendering separately enforces UTF-8.

## Custom backends

A custom lexer receives one line (including its LF), previous state, an emit
callback/context, output state pointer and borrowed user data. State starts at
zero. Return 1 on success, 0 on failure. Emit ordered nonempty line-relative
ranges with valid token kinds. Gaps become TEXT and adjacent equal kinds merge.
Overlaps, out-of-range spans or ignored emit failures reject the update.

The lexer must be deterministic for its input/state/user configuration. Do not
mutate its configuration while a cache is active; create a new cache instead.
Callbacks must not retain input/output pointers or mutate the buffer/cache.
Custom UTF-8 token boundaries are required only if terminal rendering is used.

## CSS, rendering and terminal adapters

`default_theme` fills shared `mulib_term_style_t` entries. `theme(sheet, language,
styles, error)` computes styles through `termstyle` on synthetic nodes:

```css
code.c { color: #ddd; background-color: #181818; }
code.c .keyword { color: #c8f; font-weight: bold; }
.comment { color: #888; font-style: italic; }
.string { color: #9c6; }
```

The root has classes `syntax` and one of `plain/c/cpp/python/json/shell`; child
`span` classes are exactly `token_name(kind)`. CSS specificity, `!important`,
source order and terminal inheritance reuse the existing CSS machinery. NULL
sheet selects the default theme. Theme output is transactional.

`render(cache, offset, length, styles, info, flags, output)` appends a styled
range through `mulib_termstyle_write`, with resets around runs. NULL styles
select the default theme; NULL capabilities select safe plain text. An explicitly
loaded or explicitly selected ANSI terminfo record enables styling; TRUECOLOR
requires that ANSI profile. Range/token boundaries must preserve valid UTF-8.
Invalid UTF-8, missing capabilities and allocation failures preserve output.
Source escape/control bytes cannot become terminal commands: C0 except LF/TAB,
DEL and C1 are replaced with U+FFFD, even in plain mode.

For a screen editor, combine `span`/`at`, buffer ranges and theme entries with
`mulib_tui_text` or `mulib_curses_style`; the style representation is shared.
Those modules retain their own one-scalar-per-cell limits. This module supplies
no input handling, viewport wrapping, grapheme widths, LSP/semantic analysis,
tree-sitter binding, terminal probing or filesystem I/O.

Destroy the cache with `mulib_termsyn_destroy`; NULL is harmless. Zero is success;
negative EINVAL/ENOMEM/ERANGE/ENOTFOUND/ELEXER report cache failures. Theme/render
can also propagate negative termstyle errors; do not interpret overlapping enum
numbers without knowing which operation failed.

Build: `cc -std=c11 -Iinclude examples/modules/termsyn.c -lm -o termsyn-example`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-termsyn.h`](../include/mulib-termsyn.h) |
| Implementation switch | `MULIB_IMPL_TERMSYN` |
| Repository-local includes | [mulib-edbuf](mulib-edbuf.md), [mulib-termstyle](mulib-termstyle.md) |
| Complete example | [`examples/modules/termsyn.c`](../examples/modules/termsyn.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/termsyn.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
