# PE lookup utilities

Include `mulib-peutils.h`; define `MULIB_IMPL_PEUTILS`, `MULIB_IMPL_PE`,
`MULIB_IMPL_COFFUTILS`, `MULIB_IMPL_COFF`, `MULIB_IMPL_ENDIAN`.
[Example](../examples/modules/peutils.c).

`mulib_peutils_section(view, name, index, out)` finds the first exact named PE
section; the optional index is zero-based. Returned native section fields retain
the distinction between virtual size and padded raw file size.

`mulib_peutils_symbol(view, name, out)` searches the image's **COFF symbol table**,
not its export directory. Stripped images normally return ENOTFOUND. It skips
auxiliary, undefined and debug-only records, and includes defined absolute symbols.
DLL exports, forwarded exports and imported symbols are not resolved by this API.

Names are exact and case-sensitive. Return values are COFF result codes (zero
success, ENOTFOUND, or structural error). Outputs are unchanged on failure; no
allocation or ownership transfer occurs. The borrowed PE file must remain alive
and unchanged. See [PE reader](mulib-pe.md), [COFF lookup](mulib-coffutils.md),
and [shared contracts](binary-formats.md).
