# Terminal styling

Include `mulib-termstyle.h`; define `MULIB_IMPL_TERMSTYLE`, `MULIB_IMPL_TERMINFO`
and `MULIB_IMPL_STRBUF` in one TU. No curses/terminfo runtime library is linked.
[Example](../examples/modules/termstyle.c).

Initialize a context with `mulib_termstyle_init(&style, &info, enabled)`. The
terminfo record is borrowed and must remain alive. NULL info forces plain mode.
The caller decides TERM database loading, isatty and NO_COLOR policy. ANSI output
is never inferred: call `mulib_terminfo_ansi` explicitly if that profile is wanted.

A `mulib_termstyle_spec_t` contains attribute flags BOLD, DIM, ITALIC, UNDERLINE,
REVERSE and foreground/background indexed colors. Set each color to -1 for
terminal default (a zero-initialized spec instead requests color zero).
`apply` emits sgr0, then requested capabilities and colors. `reset` emits sgr0.
`text` emits apply + literal text bytes + reset; it can safely borrow text from
the output buffer. It does not escape control characters in application text.

Enabled styling requires the capabilities actually requested (`bold`, `dim`,
`sitm`, `smul`, `rev`, `setaf`, `setab`) and `sgr0`. Color indexes must be smaller
than the record's `colors` capability. Unsupported/missing capabilities, malformed
expansion, invalid specs and OOM return 0 without appending partial escapes.
Disabled contexts emit no escapes and text is copied unchanged. Successful calls
return 1 and append to an initialized `mulib_strbuf_t`; free it normally.

No truecolor approximation, attribute state tracking, nested-style restoration,
terminal probing, or terminfo `ncv` combination policy is implemented. Callers
must choose combinations supported by their terminal. Expansion uses
[terminfo's documented bounded subset](mulib-terminfo.md).
