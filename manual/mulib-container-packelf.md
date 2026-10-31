# mulib-container-packelf

## NAME AND INTEGRATION

ELF64 image objects and non-destructive ELF file image overlays.
Define `MULIB_IMPL_CONTAINER_PACKELF`, the `imgformat` closure,
`MULIB_IMPL_ELF` and `MULIB_IMPL_ENDIAN`. Example:
`examples/modules/container-packelf.c`.

## RELOCATABLE OBJECTS

`object(image,length,machine,out,limit)` validates an image and creates a deterministic
ELF64 little-endian ET_REL file with a null section, `.shstrtab`, and nonallocated
`.mulib.image` PROGBITS section. machine is ELF e_machine (62 AMD64, 183 AArch64,
or another caller-selected value). Image bytes and section table are 8-byte aligned.
There are no program headers, executable entry point, relocations or symbol table.

The object is a metadata carrier inspectable by ELF tools. It does not convert
container code into a native executable. Image architecture remains independently
encoded inside the payload; callers choose matching deployment policy.

## APPENDED OVERLAYS

`append(elf,elf_length,image,image_length,out,limit)` validates the existing ELF and
image, copies the original ELF bytes exactly, then appends image and a 32-byte
locator. Existing segments, sections, entry point and native bytes are unchanged.
No executable stub is synthesized or launched.

Locator fields are eight-byte `MELFPK1\0` magic, u64 original ELF length, u64 image
length, u32 image CRC and u32 CRC of the first 28 locator bytes, all LE. A new
overlay can be appended to a previously packed ELF; the final locator selects the
latest payload. Earlier bytes remain preserved.

## EXTRACTION

`open(elf,length,max_image,&image,&image_length,&view)` validates ELF bounds and
prefers a valid final overlay locator. Without one, it searches section names for
exact `.mulib.image`, rejecting duplicates and wrong section types. Missing images
return ENOENT. Locator CRCs and image envelope are checked before returning
borrowed byte ranges and view. Output remains unchanged on failure.

Input may be ELF32/ELF64 and either endian as supported by the existing reader;
new objects are ELF64 LE. Section-name errors, truncated ranges and malformed
payloads fail. No relocation, loading, dynamic linking or code execution occurs.

## OWNERSHIP AND ERRORS

Object/append replace initialized CT blobs atomically and allow input to refer to
old output bytes. open outputs borrow immutable ELF storage. Limits bound final
packaging or extracted image size; allocation/overflow/format/checksum errors use
CT codes. Tests verify byte-preserving overlays and section/trailer round-trips.
