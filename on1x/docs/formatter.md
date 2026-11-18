# On1x source formatter

```sh
on1xc fmt src/main.on1x                 # formatted source to stdout
on1xc fmt src/main.on1x --check         # 0 if unchanged, 1 otherwise
on1xc fmt src/main.on1x -o src/main.on1x
on1xc fmt src/main.on1x --target latex -o main.tex
on1xc fmt src/main.on1x --target troff -o main.roff
```

`--format`, `--emit-latex`, and `--emit-troff` are leading command aliases.
For source using a compiler-enabled extension without `@enable`, pass the
matching `--enable NAME`. Formatting does not load imports, check types,
expand macros, or run PPX programs. Syntactically invalid input is rejected
before any output is published. Markup targets produce listings, not On1x
source; use `--check` and in-place `-o` with the plain target only.

## Configuration and directives

The nearest `.on1xfmt` in the input's directory or its ancestors wins; files
do not inherit missing keys from more distant config files. Configuration is
independent of the invoking directory. The file is UTF-8/ASCII-compatible
`key = decimal_integer` lines; blanks and `#` comments are allowed. Keys may
appear at most once; unknown keys and values outside the ranges are errors.

```ini
width = 80   # 1..4096, preferred maximum display columns (not a hard limit)
tabstop = 2  # 1..16, spaces per block or continuation indentation
```

These are the defaults. The formatter wraps at eligible list commas. Closing
delimiters, braces, and statement terminators guide its conservative line
layout; arbitrary expressions and long comments or literals may exceed `width`.
Tabs already inside literal or protected text are not expanded.

Use standalone comments to preserve a region byte for byte:

```on1x
fn main() {
  # on1xfmt: off
     println( 42 ); # preserve this layout
  # on1xfmt: on
  println(43);
}
```

The opening and closing lines are part of the preserved region. Regions must
be paired and cannot nest. They may span statements and delimiters but cannot
hide invalid syntax. Inline comments are not directives. Text inside a string,
multiline backtick command, or `@extension` carrier is never interpreted as a
formatter directive. Ordinary `#` comments retain their text and whether they
trail a token or appear on their own line. Whitespace outside comments,
literals and protected regions is canonicalized, including blank lines.
Nonempty output ends with LF.

To select the default rendering target from the source itself, use one
standalone `# on1xfmt: target = plain|latex|troff` comment outside protected
regions (write one of those values). The compiler ignores the comment in
ordinary builds. An explicit CLI/project target takes precedence; `--check`
always checks the plain source. With a markup default, use an explicit
`--target plain` for source rewrites.

## Project commands

Declare a named, shell-free formatter command in the root `On1xfile`:

```text
command "listing" format latex "src/main.on1x"
```

Run `on1xc exec listing > listing.tex` at the project root. `plain` and
`troff` are also supported. The source must be inside the project's declared
directory; the command uses the source's `.on1xfmt`. It writes to stdout
without building the project or running manifest policy.

## Implementation and limits

The adapter uses the canonical scanner's exact token spans and the canonical
parser; it recovers only trivia from gaps. It keeps the byte spelling of all
tokens (including multiline commands), preserves the adjacency of dots
(important for qualified names versus macro dotted tails), and preserves
`@extension` carriers verbatim because those source strings are observable to
their processors. Before emitting, the adapter re-scans the plain layout to
check that token spellings/order and dot adjacency are unchanged. The
formatter does not promise unchanged byte locations, diagnostic positions,
source digests, or behavior from external PPX code that depends on those.

The language adapter ends at PrettyPrint's document nodes. The language-neutral
PrettyDoc/PrettyEmit kernels perform layout and markup escaping. LaTeX and
groff interpret the outputs as ordinary listings with escaped text, not
verbatim source passed through to a second interpreter. Unicode glyph coverage
depends on the chosen TeX engine, groff device and fonts. Groff terminal devices
use their native fixed-width output; typesetting devices request Courier Roman.

The config is limited to 4 KiB. Source and plain output are each limited to
2 MiB, and document construction accepts at most 200,000 source items. Source
text embedded in the document must be valid UTF-8 without disallowed control
characters. PrettyPrint traversal, output and indentation budgets still apply.
Only regular files are accepted as input; `-o` uses atomic publication on
supported POSIX hosts. There is no whole-project recursive formatter, editor
incrementality, or syntax error recovery in this profile.
