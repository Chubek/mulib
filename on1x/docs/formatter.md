# On1x source formatter

```sh
on1xc fmt src/main.on1x                 # formatted source to stdout
on1xc fmt src/main.on1x --check         # 0 if unchanged, 1 otherwise
on1xc fmt src/main.on1x --inplace       # or -i; atomically replace the input
on1xc fmt src/main.on1x --target latex -o main.tex
on1xc fmt src/main.on1x --target troff -o main.roff
```

`--format`, `--emit-latex`, and `--emit-troff` are leading command aliases.
For source using a compiler-enabled extension without `@enable`, pass the
matching `--enable NAME`. The formatter can discover headers from
[operator](operators.md) imports and the pervasive prelude, using the same
search paths and fixity rules as the compiler. It does not parse dependency
bodies, check types, expand macros, or run PPX or declaration extensions.
Use `--stdlib DIR`, `-I DIR`, `--prelude FILE`, or `--no-prelude` to match
your build's operator environment. `--isolated` restricts formatting to the
input file and is intended for scratch or untrusted editor buffers.
Syntactically invalid input is rejected before any output is published.
`--inplace` (`-i`) writes plain source back
to the input file without writing to stdout. It cannot be combined with
`--check`, `-o`, or an explicit LaTeX or troff target. Markup targets produce
listings, not On1x source; use the plain target if passing the input path to
`-o`.

## Configuration and directives

The nearest `.on1xfmt` in the input's directory or its ancestors wins; files
do not inherit missing keys from more distant config files. Configuration is
independent of the invoking directory. The file uses UTF-8/ASCII-compatible
`key = value` lines; blanks and `#` comments are allowed. Keys may appear at
most once; unknown keys and invalid values are errors.

```ini
width = 80   # 1..4096, preferred maximum display columns (not a hard limit)
tabstop = 2  # 1..16, spaces per block or continuation indentation
algorithm = fits          # fits or search
brace_style = same-line   # same-line or allman
nested_block_newlines = 1    # 1..16
top_level_block_newlines = 1 # 1..16
```

These are the defaults. The On1x checkout's `on1x/.on1xfmt` selects `search`
and `allman`, keeps nested blocks one newline apart, and separates top-level
blocks with two newlines for sources under `on1x/`. The newline settings count
literal line breaks after a completed brace-delimited block: one puts the next
line immediately below it, while two leaves one blank line. Block continuations
such as `else` and expression/list punctuation remain attached.

`search` uses PrettyPrint's bounded group search at eligible list commas,
falling back to `fits` when the document has more than nine layout groups. If a
search exhausts the layout budget, no output is written; selecting `fits`
avoids that search.
`allman` puts opening curly braces on a new line and splits empty brace
pairs and `} else`. Closing delimiters, braces, and statement terminators
guide the conservative layout;
arbitrary expressions and long comments or literals may exceed `width`.
Tabs and braces inside literals, extension carriers and protected text
are left alone.

```on1x
fn foo() -> i64
{
  return 42;
}
```

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
and `--inplace` always use plain source. With a markup default, use an
explicit `--target plain` if rewriting the source via `-o`.

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
Only regular files are accepted as input. File output from `-o` or `--inplace`
uses atomic replacement on supported POSIX hosts and refuses to replace
symbolic links. There is no whole-project recursive formatter, editor
incrementality, or syntax error recovery in this profile.
