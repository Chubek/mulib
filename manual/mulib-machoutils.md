# Mach-O lookup utilities

Include `mulib-machoutils.h`; define `MULIB_IMPL_MACHOUTILS`, `MULIB_IMPL_MACHO`,
`MULIB_IMPL_ENDIAN`. [Example](../examples/modules/machoutils.c).

`mulib_machoutils_section(view, segment, name, index, out)` finds the first exact
section name in an exact segment. A NULL segment matches any segment; the optional
index is zero-based across all segment commands. Fixed-size file names are decoded
safely, including sixteen-character names without a NUL in the original bytes.

`mulib_machoutils_symbol(view, name, out)` finds an exact defined N_SECT or N_ABS
symbol, excluding STAB/debug and undefined/common symbols. It does not add/remove
leading underscores, demangle names, resolve reexports or interpret indirect
symbols. Names and sections are structurally checked during the search.

Return zero on success, `MULIB_MACHO_ENOTFOUND` when absent, or a Mach-O reader
error. Outputs remain unchanged on failure. No allocations or global state;
the input view borrows immutable file bytes. See [Mach-O reader](mulib-macho.md)
and [shared contracts](binary-formats.md).
