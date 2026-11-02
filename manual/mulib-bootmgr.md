# mulib-bootmgr

Define `MULIB_IMPL_BOOTMGR` once before including `mulib-bootmgr.h`.
No transitive switches, allocation or hosted runtime are needed.
[Example](../examples/modules/bootmgr.c).

Entries borrow label, path and argument strings; enabled entries may select EFI,
BIOS or kernel backends. `mulib_bootmgr_select` returns the highest priority,
breaking ties by array order, or `SIZE_MAX` for no enabled entry.

`mulib_bootmgr_boot` tries an explicit preferred index (or automatic selection
when `SIZE_MAX`), then other enabled entries in array order. All three callbacks
are mandatory. `load` must validate, authenticate and load the image and return
zero with a temporary handle; on failure it must release any partial resources.
`start` receives the entry's arguments and handles platform-specific handoff.
If it returns, `unload` is called even on success. A zero start status stops
fallback; other statuses try the next entry. The last error is returned, or -1
for invalid input/no attempt. Optional selected output starts at `SIZE_MAX` and
records the last attempted entry. A real kernel handoff may never return.

The selection manager neither changes CPU mode, reads disks, persists
BootOrder, or implements an interactive menu. `mulib_bootmgr_sector_valid` checks
only bytes 510/511, not executable safety or bootability. All buffers, entries,
contexts and callback policies belong to the caller. Calls are synchronous.

## Static ELF64 loading

`mulib_bootmgr_elf_load` loads little-endian x86-64 ET_EXEC images. Supply file
bytes, a mapped writable physical destination window, capacity, physical base,
and a result. It validates all program headers, file/memory extents, alignment,
physical/virtual segment overlaps and executable entry before writing anything.
It copies PT_LOAD data and zeroes BSS, leaving holes unchanged. NULL destination
validates without copying; capacity/base constraints still apply. Outputs are
only updated on success. Invalid input returns -1.

Dynamic/interpreter images, extended program-header numbering and relocations
are rejected. PE is not supported. File and destination may not overlap. The
file must remain immutable during validation/copy; result storage must not alias
file/destination. The returned entry is virtual; loading uses p_paddr. The caller
must map p_vaddr, check canonicality for its paging mode, set up the calling
convention and transfer control. This does not authenticate images. Overlap
checking is quadratic in the 16-bit program-header count; applications should
impose an appropriate image/resource limit.
