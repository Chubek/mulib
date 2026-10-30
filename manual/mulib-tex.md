# tex

`init` borrows validated UTF-8 source, rejecting NUL/control characters except
tab/CR/LF. `next` returns 1 token, 0 EOF, -1 invalid/trailing backslash; error
preserves cursor/output. Tokens carry kind and byte offset/length. Fixed lexical
rules recognize ASCII control words, escaped control symbols, braces, whitespace,
percent comments and ordinary text. No TeX execution, catcodes or macro expansion.

`escape` appends literal LaTeX text-mode source atomically to a strbuf, escaping
backslash/braces/#/$/%/&/_/^/~. It validates the same text profile, preserves
UTF-8 and whitespace, and does not perform typography or math rendering.
Switches: `MULIB_IMPL_TEX`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`.
Example: `examples/modules/tex.c`.
