# objfileinjector

In-place, same-size replacement of a named ELF PROGBITS section, not process
injection. `replace` first validates the complete ELF file, name/type/range and
length, rejecting section ranges overlapping the declared ELF header (including
extensions), header tables, or another file-backed section. Zero-sized replacement
is a successful no-op after section/type checks. Then
memmove permits aliased replacement bytes. Returns 0 success, negative ELF error;
failure leaves bytes unchanged.

Does not resize sections, adjust offsets, update signatures/digests, relocate
code or change executable-memory protections. Caller owns writable object bytes.
Switches: `MULIB_IMPL_OBJFILEINJECTOR`, `MULIB_IMPL_ELFUTILS`, `MULIB_IMPL_ELF`,
`MULIB_IMPL_ENDIAN`. Example: `examples/modules/objfileinjector.c`.
