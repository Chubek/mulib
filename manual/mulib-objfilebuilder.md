# objfilebuilder

Owned section builder over the focused ELF64 writer. Zero initialize; `add`
copies a unique section's name/data and validates flags/alignment. Returns 1
success, 0 invalid/duplicate/limit/OOM, preserving prior sections. Inputs may
alias existing sections because copies precede publication.

`finish` appends a complete object without consuming the builder; `free` releases
all names/bytes and resets. Same PROGBITS-only limits as object writer; no symbol
or relocation construction. Do not copy live builder ownership by assignment.
Switches: `MULIB_IMPL_OBJFILEBUILDER`, `MULIB_IMPL_OBJFILEWRITER`,
`MULIB_IMPL_BINFILEWRITER`, `MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_BUFFER`.
Example: `examples/modules/objfilebuilder.c`.
