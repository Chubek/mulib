# Literal orthographic rules

Define `MULIB_IMPL_ORTHOGRAPHIC_RULES`.
[Example](../examples/modules/orthographic_rules.c).

Rules borrow NUL-terminated match/replacement strings. Apply validates all
rules, then substitutes only the first matching prefix, suffix or whole word.
`minimum_stem` is the number of bytes left after removing the match; whole-word
rules normally require zero. Empty matches/invalid kinds are rejected. Empty
replacement and empty input word are valid. If no rule matches, return a copy.

Result is freshly malloc-owned; caller frees it. NULL indicates invalid input,
size overflow or allocation failure. No caller state is modified. Matching is
case-sensitive bytes, not a regex, Unicode normalizer or built-in language
stemmer. Rules never recurse, avoiding rewrite loops.
