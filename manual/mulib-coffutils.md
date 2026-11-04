# COFF lookup utilities

Include `mulib-coffutils.h`; define `MULIB_IMPL_COFFUTILS`, `MULIB_IMPL_COFF`,
`MULIB_IMPL_ENDIAN`. [Example](../examples/modules/coffutils.c).

`mulib_coffutils_section(view, name, index, out)` finds the first exact section
name, resolving short and long names through the checked reader. `index` may be
NULL; `out` is required. The name argument is a NUL-terminated C string.

`mulib_coffutils_symbol(view, name, out)` finds the first exact primary symbol
with a positive section number or the absolute section number -1. Undefined and
debug-only symbols are ignored; auxiliary entries are skipped, not interpreted.
There is no demangling, case folding, linker decoration rewriting or relocation.

Both return zero on success, `MULIB_COFF_ENOTFOUND` if absent, or the reader's
structural error. Outputs (including optional section index) are unchanged on
failure. Malformed encountered names are errors rather than silently skipped.
No allocations. Inputs must be valid borrowed [COFF views](mulib-coff.md).
See also [shared ownership/profile contracts](binary-formats.md).
