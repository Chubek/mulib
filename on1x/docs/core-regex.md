# Core regular expressions

The direct C11 backend supports immutable, statically compiled `regex` values.
This is language syntax, not a PPX or a library import.

```on1x
fn accepts(p: regex, text: string) -> bool {
    return match p ~ text;
}

fn main() {
    println(match /foo+/ ~ "foo");       # true
    let p: regex = /ll([a-z])/;
    let hit: captures = capture p ~ "llk";
    if hit[] > 0 {
        println(hit[0]);                 # llk (whole match)
        println(hit:1);                  # k
    }
    println((capture /雪(.)/ ~ "雪🌱")[1]);
}
```

`match pattern ~ text` tests the **whole** string and returns `bool`.
For example, `/foo+/` does not match `"afoo"`; `/.*foo+.*/` does.
`capture pattern ~ text` returns
`captures`. On success `hit[]` counts the whole match plus the numbered
groups; on failure it returns zero. `hit[i]` accepts an `i64`, returns a
`string`, and traps with a located diagnostic for a negative or out-of-range
index. `hit:1` is shorthand for `hit[1]` on a named capture value and a
nonnegative decimal literal. Inside `[...]`, unparenthesized `lo:hi` remains
a slice. An empty match or an empty group is distinct from no match.
The pattern and text operands are each evaluated once, left to right.
The operand on the right of `~` is a postfix expression; parenthesize it
to include an infix expression. Parenthesize the whole `capture` expression
to subscript it directly.

## Slash literals and division

`/…/` is a single-line literal, with `\/` for a slash even inside a class.
`//` is the empty regex in a literal position, not a comment. `#` inside the
literal is pattern data. String and shell literals retain their own syntax.
To preserve existing divisions, the common scanner adapter only opens a
slash literal at input start or after `(`, `[`, `{`, `,`, `;`, `=`, `:=`,
`=>`, `return`, `!`, `&&`, `||`, `==`, `!=`, or `~`. Immediately after the
contextual `match` or `capture`, the closing slash must be followed by `~`
(layout and comments are allowed in between). Write `(/…/)` in other
positions. A slash after an ordinary operand remains division, including in
existing expressions such as `match / 2 / 3` where `match` is a local name.
`match value with { … }` and `value ~ { … }` are structural matches.
Regex literals containing quotes, braces, or `#` should be bound outside a
`${…}` interpolation, whose delimiter recognizer is not regex-aware.

## UTF-8 pattern profile

- Literals denote Unicode scalar values; `.` matches exactly one scalar,
  including newline. A postfix `*`, `+`, or `?` applies to the preceding
  atom or `(...)` group, never to part of a UTF-8 encoding.
- Alternation `|`, concatenation, grouping, and positive ASCII classes
  (`[abc]`, `[a-z]`) are supported. Empty expressions and alternatives are
  permitted. Escapes include `\n`, `\r`, `\t`, `\/`, `\xHH` for non-NUL ASCII,
  and a backslash before regex punctuation to quote it. Write non-ASCII
  literals directly.
- Negated/non-ASCII classes, explicit anchors, counted repetition, flags,
  lookaround, backreferences, named groups, grapheme and Unicode-property
  classes, case folding, and normalization are not supported. Invalid UTF-8
  in a pattern is a compile error. Malformed UTF-8 in input cannot match;
  bytes are not replaced. Input uses On1x's NUL-terminated `string` ABI.

## Exact capture subset

The configured LibStone Firestone `liblr` exposes a byte Thompson NFA and a
UTF-8 decoder/scalar DFA, not an engine that records group participation or
capture spans. On1x admits up to 16 unnested, unquantified groups whose scalar
width and starting offset are fixed in every match. Alternatives inside a
group are allowed if they have the same scalar width; top-level alternation
with groups is not. For example, `/雪(a|é)(.)/` is capturable, but `/(a+)b/`,
`/a*(b)/`, `/(a)?/`, and `/((a))/` are valid **match** patterns only.
When there are no groups, capture zero is still the whole match.

`capture` rejects an ineligible literal during checking. A first-class
pattern loaded from another value may not be known statically, so `capture`
checks its descriptor and traps if it is not capturable. Capture never selects
an approximate span or guesses which path a regex took. The DFA establishes
that a whole match exists; the compiler-proved scalar offsets then produce
exact slices. The underlying text is snapshotted on success using the
bounded, process-lifetime C-string pool, so capture results survive function
returns, including when the original text came from a C string argument.

## Compilation and limits

The compiler uses `liblr`'s `compile_regex`, `utf8_error_offset`,
`decode_utf8_prefix`, and `utf8_scalar_dfa`. It applies a bounded subset
construction to the resulting NFA: the pinned public `determinize` has no
state/work cap. Generated C contains constant tables and calls the On1x C11
runtime; it neither builds regexes at run time nor links LibStone.

Patterns allow 256 source bytes and 32 group levels, with limits of 8,192
translated bytes, 2,048 NFA states and 512 DFA states per pattern. Checking
and emission each cap the program at 1,024 distinct patterns, 8,192 total
DFA states, 65,536 byte ranges, and 32 Mi work units for determinization.
Match/capture input is bounded by the C-string text
limit (4 MiB by default). Match has no backtracking or heap allocation;
capture snapshots and indexed results share the existing bounded string
pool. Resource limits are errors, not false matches.

Regex types cannot be passed directly through `extern`. The scalar MIR and
PPX wire/`--emit-ast` paths do not support this profile. Without the Firestone
compiler SDK, attempting to check a literal reports `ON1X-REGEX-SDK`; the
fallback scanner can still use the SDK when it is available.
