# objfilereloc

`apply` patches one bounded little-endian ABS32, ABS64 or PC32 field. Absolute
formula S+A; relative formula S+A-P, with caller-resolved symbol/addend/place.
Unsigned addresses and signed addends are overflow-checked; ABS32/PC32 ranges
checked before writes. Returns 1 success, 0 invalid/overflow/range, without mutation.

Kind constants are MuLIB kinds, **not native ELF relocation numbers**. No batch
overlap resolution, symbol lookup, architecture decoder, GOT/PLT or file I/O.
Switches: `MULIB_IMPL_OBJFILERELOC`, `MULIB_IMPL_OBJFILEPOSINDEP`,
`MULIB_IMPL_BYCUTILS`. Example: `examples/modules/objfilereloc.c`.
