# latex

Zero initialize an owning document. `begin` writes an article preamble with
escaped title; `section` supports levels 1..3; `paragraph` appends escaped text.
`finish` appends end-document idempotently and rejects further content. All return
1 success, 0 invalid/state/allocation failure, without partial append. Borrow
`source.data`/`source.length` until mutation; `free` releases/reset.

This is source construction, not a TeX parser/compiler/renderer. No external
command, shell escape, package loading, custom raw commands or PDF generation.
Switches: `MULIB_IMPL_LATEX`, `MULIB_IMPL_TEX`, `MULIB_IMPL_STRBUF`,
`MULIB_IMPL_UTF8`. Example: `examples/modules/latex.c`.
