# Core literals and formatting

These forms are built into On1x. They do not require a macro, a PPX, or an
extension flag. The generated and bootstrap scanners use the same recognizer
for complex literals. The default C11 backend executes the forms below.

## Characters and bytes

`char` is a Unicode **scalar value**, not a grapheme cluster, an integer type,
or a one-character `string`. It supports equality, structural literal matching,
and ordinary value passing. `char_codepoint(c)` returns its `i64` code point;
`println(c)` writes its UTF-8 encoding followed by a newline. Formatting a NUL
character into a `string` is rejected because the string ABI is NUL-free.

| Literal | Value |
|---|---|
| `#\\` | Backslash, U+005C |
| `#\newline` | Line feed, U+000A |
| `#\a` / `#\f` | The letters `a` / `f`, not C escapes |
| `#\u+ff22` / `#\Ｂ` | Fullwidth `Ｂ`, U+FF22 |
| `#\U+1f642` | `🙂`, U+1F642 |
| `#\x2211` | `∑`, U+2211 |
| `#\o112` | `J` (octal 112), U+004A |
| `#\"` | Double quote, U+0022 |
| `#\pound` | Pound sterling sign `£`, **U+00A3**, not `#` |

Other names are `space`, `tab`, `return`, `alarm`, `backspace`, `escape`,
`delete`, `nul`, and `null`. Names are case-sensitive. A literal source
character must be one valid UTF-8 scalar; use `#\space`, `#\tab`, or
`#\newline` to spell common whitespace. Lowercase `u+` takes a hexadecimal
value up to U+FFFF; uppercase `U+`, `x`/`X` (hexadecimal), and `o`/`O` (octal)
can represent any scalar up to U+10FFFF. All reject surrogates and values
outside the scalar range. For example, `#\U+ffffff`, `#\u+1f642`, and
`#\xd800` are errors. `#` still starts a line comment except when it is
immediately followed by `\`.

Place `b!` immediately before a character literal to get a `u8`. `b!#\a`
is `97u8`, **not** `97` or `"a"`. This is a byte value conversion, not UTF-8
encoding; `b!#\pound` is one byte `0xA3u8`, and `b!#\u+100` is rejected.

## Text, raw strings, and buffers

| Form | Type | Backslash escapes | Interpolation |
|---|---|---|---|
| `"text"` / `"""text"""` | `string` | Yes | Yes |
| `r!"text"` / `r!"""text"""` | `string` | No | No |
| `b!"data"` / `b!"""data"""` | `buffer` | Yes, including `\0` and `\xHH` | No |

The prefix and opening quote are adjacent. Triple quotes preserve every
newline and indentation byte between the delimiters; no de-indentation or
trimming is performed. They can contain individual quotes or pairs of quotes.
A raw single-quoted form cannot contain its closing `"`, even after a
backslash; use the raw triple-quoted form instead. Non-raw text retains the
existing `\\`, `\"`, `\n`, `\r`, and `\t` escapes and adds `\$`. A text
literal with `\0` is rejected. For text with a literal dollar use `\$`, `$$`,
or a raw string. File paths in import/PPX registration directives and
literal `launch exec(...)` arguments do not interpolate.

Byte buffers copy source bytes (UTF-8 bytes for non-ASCII source text) and
decode the byte escapes. Each evaluation creates a fresh managed `buffer`;
assigning it to another binding shares the same mutable buffer as with
`buffer_new()`. `buffer_len(b)` counts bytes, `buffer_at(b, i)` takes a
zero-based `i64` index and returns `u8`, and `buffer_push(b, x)` appends a
`u8` (including zero). `buffer_text(b)` snapshots a **NUL-free** buffer;
it rejects embedded NUL rather than producing a truncated string. It is not
a UTF-8 validator. Existing `buffer_byte(b, i64)` still accepts only 1–255.

```on1x
let buf = b!"A\0\xff";
assert(buffer_len(buf) == 3);
assert(buffer_at(buf, 0) == b!#\A);
assert(buffer_at(buf, 1) == 0u8);
assert(buffer_at(buf, 2) == 0xffu8);
```

## Interpolation and `format`

`$name` inserts a lexical binding (an ASCII identifier). `${expr}` evaluates
one On1x expression in the surrounding scope, including qualified references
and nested text literals. Expressions are evaluated exactly once, left to
right. `$$` and `\$` produce a literal `$`; other dollar signs not followed
by an identifier or `{` are literal. Values inserted into text are never
reinterpreted as source code. Regex literals with delimiter-sensitive content
are not supported inside `${…}`; bind the regex outside the string.

`format(text)` returns the given string, so `format("hello $name")` uses the
same lexical interpolation and does not reinterpret braces inserted by
`$name`. `format(pattern, arg, ...)` consumes one argument per `{}` from left
to right; `{{` and `}}` write literal braces. In this multi-argument form
unmatched braces and missing or extra arguments are terminal runtime errors.
No width, positional numbering, or arbitrary `printf` format codes are
accepted. A visible user binding or imported alias named `format` takes
priority over the builtin. Use a literal or raw pattern when substituted
values must never be read as positional placeholders. Interpolation holes and
positional arguments accept strings, booleans, `char`, and numeric scalars.
Non-string values render as text (UTF-8 for `char`, decimal for integers,
and `%.9g`/`%.17g` for `f32`/`f64`).

```on1x
let name = "Ada";
let score = 0b1010;
println("hello $name; next score ${score + 1}");
println(format("{{{}}}: {}", name, score));
```

Interpolation is limited to 256 literal/expression parts, 32 lexical splice
levels, and the ordinary parser/source budgets. `format` accepts at most 256
positional arguments. Formatted strings use the existing bounded,
process-lifetime text snapshot pool and are reclaimed at process exit.

## Numeric spellings

`0o`/`0O` introduce octal and `0b`/`0B` introduce binary. The default type is
checked `i64`; `u8`, `u16`, `u32`, and `u64` suffixes select the existing
unsigned types. There is no leading-zero octal convention (`010` is ten).
Decimal scientific notation already supports `e`/`E`, an optional exponent
sign, and mandatory decimal exponent digits: `4e2`, `1.5E-2`, and `3e+8f32`.
Without a suffix scientific literals are `f64`. Out-of-range values use the
existing checked diagnostics. Array/slice extents still require decimal.

## Tooling and limits

The formatter preserves literal bytes rather than rewriting their interiors.
PPX v1 retains its existing `StringLiteral` payload format: decoded text is
literal data, never interpolated when read back. `CharType`,
`CharacterLiteral`, `ByteBufferLiteral` (a hex payload), and
`InterpolatedString` are additive wire constructors. Byte literals and radix
integers reuse existing wire numeric constructors.

New `char`, buffer, and formatting operations do not extend scalar MIR.
Numeric spellings and scalar `u8` byte literals continue to work wherever their
existing numeric types were supported. The raw extern ABI does not admit
`char`. There is no Unicode normalization or grapheme handling and no general
Unicode validation for legacy `string` data.
