# X Layout Language (XLL)

[Manuals](README.md) · [Library index](index.md)

**Header:** `include/mulib-xll.h` — C11 implementation, C++ declarations.

XLL version 1 describes named Unicode text layouts. The same owned program can
be parsed from a `.xll` file, built with the C API, executed directly, serialized
back to XLL, or translated to C that calls `mulib-layout.h`.

The emitted file contains data and ordinary layout calls. It neither embeds an
XLL parser nor requires XLL at runtime. Font selection and rendering belong to
the embedding application, so the same generated layouts can feed a screen,
HTML-related renderer, or PDF backend.

## Language

```text
xll 1 {
  layout greeting {
    text "Hello سلام world";
    font "Noto Sans" 18;
    language "und";
    width 400;
    spacing 1.2;
    wrap word;
    align start;
    direction auto;
    origin 20 40;
  }
}
```

Grammar (whitespace/comments may separate tokens):

```text
program   = "xll" "1" "{" declaration* "}" EOF
declaration = "layout" identifier "{" property* "}"
property  = "text" string ";"
          | "font" string number ";"
          | "language" string ";"
          | "width" number ";"
          | "spacing" number ";"
          | "origin" number number ";"
          | "wrap" ("none" | "word" | "grapheme" | "word_grapheme") ";"
          | "align" ("start" | "end" | "center" | "left" | "right") ";"
          | "direction" ("auto" | "auto_rtl" | "ltr" | "rtl") ";"
identifier = [A-Za-z][A-Za-z0-9_]{0,62}
```

Keywords and identifiers are case-sensitive. Layout names must be unique.
Properties may appear in any order but cannot repeat. Unknown properties, enum
values, versions, missing punctuation and trailing tokens are errors. The version
must be spelled `1`, not `1.0`. There are no expressions, imports, substitutions,
implicit font loading or script execution.

Strings and numbers follow JSON syntax, using MuLIB's JSON parser: quoted UTF-8,
escapes including `\n`, `\"`, `\\`, `\uXXXX` and surrogate pairs; signed finite
decimal numbers with optional exponents. Leading plus, leading decimal point,
NaN and infinity are invalid. A text string can contain escaped NUL; raw NUL in
source or decoded NUL in metadata is rejected. Comments are `//` through end of
line or non-nesting `/* ... */`. Whitespace is space, tab, CR and LF.

Each declaration is independently laid out. Origins are renderer translations;
they do not move the coordinates stored inside the computed result. The language
models paragraph layout rather than automatic page flow or nested box placement.

### Defaults and mapping

| Property | Default | Meaning |
| --- | --- | --- |
| text | `""` | Explicit-length UTF-8 passed to `mulib_layout_set_text` |
| font | `"sans" 16` | Application-resolved family/pattern and pixel size |
| language | `"und"` | Shaping language, overriding the supplied font set |
| width | `0` | Unbounded; positive values constrain wrapping |
| spacing | `1.2` | Positive line-metric multiplier |
| origin | `0 0` | x/y supplied to `mulib_layout_draw` by the caller |
| wrap | `word_grapheme` | Unicode word wrapping with grapheme fallback |
| align | `start` | Logical alignment, respecting paragraph direction |
| direction | `auto` | `auto=-1`, `auto_rtl=-2`, `ltr=0`, `rtl=1` |

All shaping, bidi, fallback, line breaking and output ownership follow the
[`mulib-layout`](mulib-layout.md) contract. Widths are advances, not ink extents;
spaces and hard breaks are preserved. Empty text produces a metric-height line.

## API construction and ownership

```c
mulib_xll_t *program = mulib_xll_new();
mulib_xll_block_t block = mulib_xll_block_default();
mulib_xll_error_t error;
block.name = "greeting";
block.text = "Hello سلام world";
block.length = strlen(block.text);
block.font_family = "Noto Sans";
block.font_size = 18;
block.options.width = 400;
block.options.wrap = MULIB_LAYOUT_WRAP_WORD;
block.x = 20;
block.y = 40;
if (!program || mulib_xll_add(program, &block, &error)) {
    /* Handle allocation/validation failure. */
}
```

`add` copies the name, explicit-length text, family and language. `set` replaces
an existing index, transactionally, and permits a borrowed `get` view as input.
Duplicate names are rejected except for replacement of that same index.
Failures leave the whole program unchanged. `count(NULL)` is zero; `get` returns
a borrowed immutable view or NULL out of range; `find` returns `SIZE_MAX` when
absent. Mutation invalidates borrowed views. `destroy(NULL)` is harmless.

Parsing creates the same owned representation; errors clean up partial programs.
`mulib_xll_error_t` contains a static message, byte offset and one-based byte
line/column. CRLF counts as one newline. Invalid source encoding/size reports
offset zero; semantic declaration errors point to the declaration's start.
API errors use offset zero. Error structures are optional and reset on entry.

`serialize(program,&length)` returns malloc-owned canonical XLL with every
property written explicitly. It preserves declaration order, values and source
bytes, including embedded NUL, but discards comments/formatting. Parsing this
output reconstructs the same program. JSON formatting supplies locale-independent
numbers with round-trip double precision. Free returned text with `free`.

## Direct execution

```c
const mulib_xll_block_t *request = mulib_xll_get(program, 0);
/* Resolve request->font_family and an ordered fallback list in the application.
 * Supply fontshape faces at (int)(request->font_size * 64 + 0.5). */
mulib_layout_result_t result = {0};
mulib_xml_error_t layout_error;
if (!mulib_xll_compute(program, 0, &fontset, &result, &layout_error)) {
    mulib_layout_draw(&result, request->x, request->y, draw_glyph, renderer);
    mulib_layout_result_destroy(&result);
}
mulib_xll_destroy(program);
```

`create_layout` returns an owned configured layout for applications needing
further changes; destroy with `mulib_layout_destroy`. `compute` creates, computes
and destroys that temporary layout. Both borrow ICU/fonts, check that every face
has the requested scale, and override the font set's language/base with the
declaration. No font is opened implicitly, and a family-name match is the caller's
responsibility. For fractional pixel sizes use `mulib_fontshape_open` with the
rounded scale; paired glyph-font helpers currently accept integer pixels.

Compute output must be zero-initialized. Results remain valid after program
destruction; rendering still requires their corresponding fonts. Runtime failures
use `mulib_xml_error_t`, matching layout. Existing results are unchanged on failure.
Contexts require the same caller synchronization as the native layout/font APIs.

## Translation to a C file

Build the runnable example as a translator:

```sh
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/modules/xll.c -lm -ldl -pthread -o xll
./xll examples/greeting.xll greeting.c page
```

Without arguments the example exercises parsing, API construction and emission.
With arguments it reads the source, validates and generates it fully, then opens
the output file. Invalid XLL or a bad symbol prefix does not truncate the output.
Read/write/close failures return nonzero and diagnostics go to stderr.

The library equivalents are `emit_c(program,prefix,&length,error)` (owned string)
and `write_c(program,prefix,FILE*,error)` (borrowed stream). A prefix follows the
same identifier grammar as layout names and is always prefixed by `mulib_xll_`.
Different files should use different prefixes. `write_c` generates completely
before writing, never flushes or closes the stream, and returns -1 on a write
error. I/O errors may leave partial bytes; callers also check flush/close errors.

For prefix `page`, generated exports are:

```c
typedef struct mulib_xll_page_block {
    const char *name, *text, *font_family, *language;
    size_t length;
    double font_size, x, y;
    mulib_layout_options_t options;
} mulib_xll_page_block_t;

size_t mulib_xll_page_count(void);
const mulib_xll_page_block_t *mulib_xll_page_get(size_t index);
mulib_layout_t *mulib_xll_page_create(size_t index,
    const mulib_pglayout_fontset_t *fonts, mulib_xml_error_t *error);
int mulib_xll_page_compute(size_t index, const mulib_pglayout_fontset_t *fonts,
    mulib_layout_result_t *result, mulib_xml_error_t *error);
```

The generated metadata is immutable with static lifetime. Include the generated
file in exactly one application TU to use its type and functions directly, or
compile it separately and supply matching declarations to clients. It includes
`mulib-layout.h`, emits no `main`, and enables no implementation switches. Its
functions have C linkage, including when compiled as C++. Link against the
application's layout implementation TU. Runtime behavior and failure semantics
match `mulib_xll_create_layout`/`mulib_xll_compute`.

Text and metadata are emitted as bounded-line unsigned-byte arrays rather than
interpolated C string literals. This preserves embedded NUL and UTF-8 without
trigraph, quote, hexadecimal-escape or compiler string-length hazards. Numeric
data uses normalized JSON decimal spelling accepted by C. Emission is
deterministic and independent of machine font availability.

## Integration switches

Define these in exactly one TU before including `mulib-xll.h`:

```c
#define MULIB_IMPL_XLL
#define MULIB_IMPL_LAYOUT
#define MULIB_IMPL_GLYPH
#define MULIB_IMPL_LIGATURE
#define MULIB_IMPL_PGLAYOUT
#define MULIB_IMPL_FONTDRAW
#define MULIB_IMPL_FONTSHAPE
#define MULIB_IMPL_SYSFONTS
#define MULIB_IMPL_BIDI
#define MULIB_IMPL_UNIBREAK
#define MULIB_IMPL_UNIPROC
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_STYLESHEET
#define MULIB_IMPL_CSS
#define MULIB_IMPL_XML
#define MULIB_IMPL_YAML
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-xll.h"
```

Generated-code-only applications omit XLL and include `mulib-layout.h` instead.
Native font/Unicode backends are needed for computation, not parsing/emission.
Linux links use `-lm -ldl -pthread`; other platforms use their native loader setup.

## Limits and tests

* Up to 1024 declarations; 16 MiB of decoded text across a program.
* Source up to 128 MiB, allowing canonical escaping of a maximum-size program.
* Identifiers up to 63 ASCII bytes; family up to 255 UTF-8 bytes; language up to
  63 UTF-8 bytes. Metadata is nonempty and rejects ASCII control characters.
* Font size in `[1/64, 1000000]` pixels; width `[0,1000000]`; spacing
  `(0,1000000]`; origins `[-1000000,1000000]`. Values must be finite.
* Numeric tokens up to 128 bytes. No recursive grammar or user-defined nesting.
* Runtime computation additionally follows layout's work, line and backend limits.

Allocation products are bounded by these limits. Public functions return NULL or
-1 on allocation failure, without changing caller-owned programs/results.
Emission builds a complete in-memory output (byte arrays expand text about 5x).

`tests/test_xll.c` covers parsing, JSON Unicode escapes, exact serialization,
API ownership/replacement, limits, diagnostics, emission and allocation failures.
`tools/check_xll.py` compiles and runs emitted C: it compares all metadata and
native layout/glyph results with direct execution, then links without an XLL
implementation and exercises empty/long-text programs. CMake/CTest and
`tools/check.py` both run this generated-code check. Set
`MULIB_TEXT_REQUIRE_BACKENDS=1` to require real ICU/HarfBuzz/FreeType/Fontconfig
coverage instead of allowing a backend-unavailable skip.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-xll.h`](../include/mulib-xll.h) |
| Implementation switch | `MULIB_IMPL_XLL` |
| Repository-local includes | [mulib-json](mulib-json.md), [mulib-layout](mulib-layout.md), [mulib-strbuf](mulib-strbuf.md) |
| Complete example | [`examples/modules/xll.c`](../examples/modules/xll.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/xll.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
