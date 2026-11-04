# Mach-O reader

Include `mulib-macho.h`; define `MULIB_IMPL_MACHO` and `MULIB_IMPL_ENDIAN`.
[Example](../examples/modules/macho.c), [shared contracts](binary-formats.md).

`mulib_macho_open` borrows a thin Mach-O32/Mach-O64 file in either byte order.
It checks the header, exact command-table consumption/alignment, segment payload
ranges, section/relocation ranges and LC_SYMTAB table spans. LC_SEGMENT versus
LC_SEGMENT_64 must match the file class; duplicate symbol-table commands fail.
Unknown load commands remain bounded opaque bytes rather than being interpreted.

`command` returns a type, size and borrowed command span. `section` uses a
zero-based index flattened across segment commands and copies names into
NUL-terminated 17-byte arrays. `section_name` instead borrows up to sixteen bytes.
Alignment is the native base-two exponent. Zero-fill section types 1, 12 and 18
have virtual sizes without file payload. `bytes` always checks actual file bounds.

`symbol` decodes nlist/nlist_64, preserving type, section ordinal, description
and value. `symbol_name` checks the string index and terminating NUL on access.
Symbol section ordinals are native one-based values, not the section API's index.
Debug and undefined symbols remain visible to dump tools; lookup filters them.

Results are `OK=0`, `EINVAL=-1`, `EFORMAT=-2`, `EBOUNDS=-3`,
`EUNSUPPORTED=-4`, `ENOTFOUND=-5`. No allocation or I/O. Fat/universal files
are rejected; select a thin slice externally. This is not a dyld loader, relocation
engine, codesign verifier or parser of every command's private payload.
