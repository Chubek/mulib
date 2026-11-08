# mulib-bootmgr

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_BOOTMGR`, `MULIB_IMPL_EFI`, `MULIB_IMPL_PE`,
`MULIB_IMPL_COFF`, `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_BIOS`,
`MULIB_IMPL_UEFIUTILS` and `MULIB_IMPL_UEFI` once. No allocations are made
by the selection manager; firmware may allocate during UEFI image launch.
[Example](../examples/modules/bootmgr.c).

Entries borrow label, path and argument strings; enabled entries may select EFI,
BIOS or kernel backends. `mulib_bootmgr_select` returns the highest priority,
breaking ties by array order, or `SIZE_MAX` for no enabled entry.

`mulib_bootmgr_boot` tries an explicit preferred index (or automatic selection
when `SIZE_MAX`), then other enabled entries in array order. All three callbacks
are mandatory. `load` must validate, authenticate and load the image and return
zero with a non-NULL temporary handle. Any non-NULL handle returned even on
failure transfers ownership and is passed to unload. Failed load with NULL
retains responsibility for its own partial resources.
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
are rejected by the ELF loader. File and destination may not overlap. The
file must remain immutable during validation/copy; result storage must not alias
file/destination. The returned entry is virtual; loading uses p_paddr. The caller
must map p_vaddr, check canonicality for its paging mode, set up the calling
convention and transfer control. This does not authenticate images. Overlap
checking is quadratic in the 16-bit program-header count; applications should
impose an appropriate image/resource limit.

## UEFI and BIOS

`boot_platform` adds explicit firmware routing. Supply EFI or BIOS firmware kind
and matching backend, plus an optional kernel backend. Automatic selection uses
the highest eligible priority; an explicit preferred index must be eligible.
Incompatible firmware entries are skipped without invoking a backend. Generic
backend handles are wrapper objects: their unload callback must understand
whether firmware already consumed/unloaded the native image.

`uefi_application` checks an in-memory PE EFI **application**, then calls firmware
LoadImage (retaining platform authentication policy), obtains LoadedImage, sets
binary load options and calls StartImage. It returns full native EFI_STATUS.
Denied handles are unloaded without starting. After StartImage returns it checks
whether LoadedImage still exists before unloading, because applications are
normally already unloaded. ExitData is freed via FreePool. The original service
failure wins over cleanup errors; cleanup failures after success are returned.
Inputs/options remain valid for the synchronous call; this helper is not for
resident drivers. Calls must be serialized, before ExitBootServices. Optional
device path provides source provenance; use a full device path when firmware
policy needs it. On a failed initial LoadedImage query, the returned error takes
precedence over unload failure.

`bios_chain` checks a 512-byte sector and calls an explicit handoff callback.
That callback must copy it to physical 0x7c00, set up the platform's real-mode
entry environment and pass the boot drive in DL. Its result is returned if it
returns. No C function-pointer cast can substitute for the required mode switch.
The BIOS header supplies EDD/E820/MBR preparation; disk I/O is caller-owned.

Tests in `test_boot_standards.c` verify cross-firmware filtering, failed-load
ownership, security rejection, application auto-unload and exit-data release.
Host mocks do not establish actual firmware boot success.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bootmgr.h`](../include/mulib-bootmgr.h) |
| Implementation switch | `MULIB_IMPL_BOOTMGR` |
| Repository-local includes | [mulib-bios](mulib-bios.md), [mulib-efi](mulib-efi.md), [mulib-uefiutils](mulib-uefiutils.md) |
| Complete example | [`examples/modules/bootmgr.c`](../examples/modules/bootmgr.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bootmgr.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
