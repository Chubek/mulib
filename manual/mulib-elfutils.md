# elfutils

Named lookups over a validated borrowed ELF32/64 view. `section` returns the
first exact named section plus optional index. `symbol` searches SYMTAB/DYNSYM
for the first defined named symbol, including local symbols. Outputs change only
on success. Returns existing ELF result codes: 0 success, ENOTFOUND missing,
other negative structural/bounds/type/invalid errors.

No dynamic loader, cross-object resolution, weak/strong symbol arbitration or
runtime address mapping. Input must remain immutable/live after `elf_open`.
Switches: `MULIB_IMPL_ELFUTILS`, `MULIB_IMPL_ELF`, `MULIB_IMPL_ENDIAN`.
Example: `examples/modules/elfutils.c` also uses the object writer to build input.
