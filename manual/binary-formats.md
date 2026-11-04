# Binary formats and dump tools

The binary modules are byte-oriented C11 tools, not native-structure casts or
operating-system loaders. Files can be inspected on a host of either byte order.
No platform SDK, binutils, LLVM, curses or terminfo library is required.

| Format | Reader | Owned builder | Lookup | Container packing | Dump |
|---|---|---|---|---|---|
| ELF | [elf](mulib-elf.md) | [elfbuilder](mulib-elfbuilder.md) | [elfutils](mulib-elfutils.md) | [packelf](mulib-container-packelf.md) | [elf-objdump](mulib-elf-objdump.md) |
| COFF | [coff](mulib-coff.md) | [coffbuilder](mulib-coffbuilder.md) | [coffutils](mulib-coffutils.md) | [packcoff](mulib-container-packcoff.md) | [coff-objdump](mulib-coff-objdump.md) |
| PE | [pe](mulib-pe.md) | [pebuilder](mulib-pebuilder.md) | [peutils](mulib-peutils.md) | [packpe](mulib-container-packpe.md) | [pe-objdump](mulib-pe-objdump.md) |
| Mach-O | [macho](mulib-macho.md) | [machobuilder](mulib-machobuilder.md) | [machoutils](mulib-machoutils.md) | [packmacho](mulib-container-packmacho.md) | [macho-objdump](mulib-macho-objdump.md) |

## Ownership and failures

Readers borrow immutable input. Views, returned names and byte spans are valid
only while that input remains alive and unchanged. Open/access functions return
zero on success and a negative format-specific error otherwise. Outputs are
unchanged on failure. Table indexes are zero-based; native symbol section numbers
retain their format-specific meaning. Validated structure does **not** imply a
loadable, correctly relocated, signed or semantically valid program. Names and
symbol details are checked on access; readers do not interpret instruction bytes.

Builders start at `{0}`, copy section names/data, and append a complete file to
an initialized `mulib_buffer_t`. They return 1/0 and leave output unchanged on
failure. `finish` does not consume the builder. Free both builder and output.
The shared section-storage flags are WRITE=1, ALLOC=2, EXEC=4; these are not native
COFF/Mach-O flag values. ELF's `.shstrtab` name remains reserved by shared storage.

Container packers replace initialized `mulib_ct_blob` outputs atomically. Their
limit includes the whole generated file; `open`'s limit covers the embedded image.
Overlays retain the original file, append an image and a 32-byte little-endian
locator (8-byte magic, u64 original length, u64 image length, u32 image CRC32,
u32 CRC32 of the first 28 locator bytes). A recognized malformed trailer is an
error, not a request to fall back to a section. CRC is accidental-corruption
detection, **not authentication**. PE/Mach-O append reject known signing metadata.
These helpers do not provide executable startup stubs or container isolation.

## Shared dump rendering

[objdump](mulib-objdump.md) supplies metadata, escaped names and hex/ASCII rows.
Format adapters use the same options, limits and optional
[termstyle](mulib-termstyle.md) context. They append a complete dump atomically,
returning 1/0. They do not invoke external commands or disassemble instructions.

`tests/test_binary_formats.c` covers readers, invalid/truncated files, native
tables, builders, dump limits/escaping, style output, and packing/overlay roundtrips.
`tests/test_binary_formats_alloc.c` checks injected allocation failures.
Standalone examples are in `examples/modules/`; the dependency-complete fixture
`examples/binary-formats-common.h` shows composition, not a mandatory dependency
on all formats. Define all required implementation switches before any include,
in exactly one translation unit.
