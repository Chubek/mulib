# mulib-efi — `.efi` files

Define `MULIB_IMPL_EFI`, `MULIB_IMPL_PE`, `MULIB_IMPL_COFF` and
`MULIB_IMPL_ENDIAN` in one C11 translation unit.
[Example](../examples/modules/efi.c).

## Format and API

`mulib_efi_open` borrows immutable PE32/PE32+ file bytes and produces a
`mulib_efi_image_t` (the shared `mulib_pe_t` view). It checks executable-image
characteristics, EFI application/boot-driver/runtime-driver subsystem (10–12),
machine/optional-header width, image/header/section alignment, section and raw
payload overlaps, file-backed executable entry, directory ranges and certificate
record framing. The output is unchanged on failure. PE result codes apply:
0 success; negative `EINVAL`, `EFORMAT`, `EBOUNDS`, `EUNSUPPORTED`.

Supported machines: IA32, x64, ARM Thumb (`0x1c2`), AArch64, EBC, RISC-V 32/64,
LoongArch 32/64. Unsupported machines return `EUNSUPPORTED`. The implementation
uses the UEFI 4-KiB page alignment profile. ROM subsystem 13, TE images and
non-PE images are outside this file API. This is a structural validator, not a
complete loader, relocation/import decoder, Authenticode verifier or proof that
firmware can execute the code. No bytes are executed or modified.

`mulib_efi_entry` borrows the first entry-point byte. Shared PE functions inspect
sections, map file-backed RVAs and query directories. `mulib_efi_certificate`
iterates aligned `WIN_CERTIFICATE` records: initialize cursor to zero; 0 returns
a borrowed payload and revision/type, 1 ends iteration, negative reports a
malformed record. The certificate directory contains **file offsets**, not RVAs.
Unknown certificate types remain visible without claiming they authenticate.
`mulib_efi_boot_name` returns a static standard removable-media filename, or NULL.

There are no allocations. Inputs, views and outputs must not alias mutable
storage. Sections are checked quadratically in the COFF section count; callers
should bound file size/count for their workload. Successful parsing does not
impose a trust policy; launch through firmware `LoadImage` for platform policy.

## Migration

The old firmware-oriented `mulib_efi_t`, native binding, memory-map and exit APIs
have moved to **`mulib-uefi.h`**, with `mulib_uefi_` names and native EFI status
results. BIOS register/thunk helpers moved to **`mulib-bios.h`**. There are no
implicit compatibility aliases: `.efi` file handling no longer includes BIOS
or boot-service callbacks. See the UEFI manual for lifecycle/status migration.

## References and verification

Format facts follow [Microsoft PE/COFF](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
(accessed 2026-11-06), and the UEFI image/subsystem conventions. No third-party
implementation was copied. `tests/test_boot_standards.c` covers truncated images,
entry and certificate corruption. `tools/check_boot_formats.py` independently
links an EFI application with Clang/LLD and parses it. Firmware execution and
Secure Boot validation are not exercised by these host tests.
