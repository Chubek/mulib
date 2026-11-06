# mulib-uefi — firmware framework

Define `MULIB_IMPL_UEFI` once. No transitive switches, allocation or OS calls.
[Example](../examples/modules/uefi.c).

## Native binding

Zero-initialize `mulib_uefi_native_t`, set the current image handle and the
services you use, then `mulib_uefi_bind`. The bundle is **not** a native
`EFI_BOOT_SERVICES` or `EFI_SYSTEM_TABLE`; never cast a firmware table to it.
Use your SDK to obtain service pointers and adapt nominal C types if needed.
The bundle/context are borrowed and must outlive the binding. Optional missing
services return `EFI_UNSUPPORTED`.

`MULIB_UEFI_API` specifies Microsoft x64 ABI on GCC/Clang x64 and cdecl on IA32.
It is overridable before inclusion. Other platforms must be compiled with the
native UEFI ABI (including their float/register rules); host architecture and
firmware word size must agree. This is not a cross-bitness trampoline. CHAR16
is `uint16_t`, BOOLEAN is `uint8_t`, UINTN/status are native pointer width, and
physical addresses and memory-page counts are 64-bit. Firmware buffers passed
to native functions must meet native alignment requirements.

## Status and lifecycle

All service wrappers return **full-width native EFI_STATUS**, not normalized
negative integers. `mulib_uefi_is_error` tests the high bit, so positive warnings
are distinguished from errors. `last_status` records the most recent actual
firmware return; locally detected invalid/unsupported arguments are returned
directly and need not update it. Firmware owns returned handles and protocol
storage; pool allocations are explicitly freed by the caller.

`mulib_uefi_exit` gets the memory map and calls `ExitBootServices`. Supply an
8-byte-aligned writable buffer, capacity in `*bytes`, stride output and a bounded
attempt count. `BUFFER_TOO_SMALL` reports required bytes. Descriptor version 1
and a complete stride (at least 40 bytes) are required; larger stride tails are
preserved. Allocate spare descriptors **before** calling. No allocation, log,
protocol lookup or other service occurs between map acquisition and exit.

The context progresses BOOT → EXIT_PENDING at the **first exit attempt** →
RUNTIME on success. Only `INVALID_PARAMETER` is retried with a fresh map key.
Even if retries fail or a later map needs more storage, other boot-service
wrappers stay blocked in EXIT_PENDING because firmware may have partly shut
down. You can retry with caller-prepared storage. Do not rebind/reset the same
live firmware context to bypass this restriction. Caller serialization is
required, and exit must run at TPL_APPLICATION.

`locate` and `handle` query protocols. `load`, `start`, `unload`, `free_pool`
expose image lifecycle and exact firmware policy results. Load can return a
handle with `SECURITY_VIOLATION`: unload it without starting. A returning
application is already unloaded by firmware; drivers can remain resident.
Start's optional exit-data allocation belongs to the caller.

Variable get/set wrappers preserve native attribute, sizing, authentication and
deletion semantics. They are usable in BOOT and RUNTIME, not EXIT_PENDING.
The caller handles runtime pointer conversion, memory mapping and serialization.
No SetVirtualAddressMap or reset orchestration is provided.

## Data validation and migration

`memory_at` safely decodes an unaligned little-endian descriptor prefix at the
reported stride and validates page ranges/alignment, retaining unknown types and
attributes. `table_check` checks a supplied table's bounded header size,
signature, reserved field and IEEE CRC32 (CRC field zeroed). It is not an
automatic validator of pointers referenced by a table.

Old `mulib-efi.h` firmware callers must rename types/functions to `mulib_uefi_`,
populate the native bundle, use EFI status values and inspect `phase` instead of
`exited`. Generic callbacks should be wrapped in native-ABI adapter functions.
BIOS callbacks now belong to `mulib-bios.h`.

## Sources and verification

Targets UEFI 2.x service definitions (UEFI 2.10/2.11 chapters 4, 7, 8).
The UEFI site's HTML returned HTTP 403 during this work; native signatures and
lifecycle were cross-checked with [EDK II UefiSpec.h](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Uefi/UefiSpec.h),
accessed 2026-11-06. Code is original, not vendored EDK II. Host tests check ABI
sizes, x64 calling conventions, retries, phase restrictions, warning/error
distinction and CRC against Python zlib. This focused binding is not a complete
firmware SDK or a claim of firmware conformance certification.
