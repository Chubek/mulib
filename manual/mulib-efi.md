# mulib-efi

Define `MULIB_IMPL_EFI` in exactly one C11 translation unit before including
`mulib-efi.h`. `mulib-efi` is also provided as an extensionless include.
There are no transitive implementation dependencies, allocations, or libc calls.
See [example](../examples/modules/efi.c).

## Firmware boundary

`mulib_efi_t` borrows its context and callbacks. It is an adapter API, **not** a
binary-compatible replacement for an EFI SDK. Custom adapters translate EFI statuses to zero (success),
`MULIB_EFI_BUFFER` (buffer too small), `MULIB_EFI_STALE` (invalid map key), or another
nonzero error. Native firmware handles stay inside the context.
`locate_protocol` receives a fieldwise GUID and returns a borrowed interface;
the adapter must convert that interface's ABI before subsequent calls.

`mulib_efi_exit` receives caller-owned writable map storage, byte capacity, output
descriptor stride, and a nonzero retry limit. It obtains the map immediately
before exit and retries only stale keys. It never allocates in between.
On buffer-too-small it reports required bytes; the caller must allocate slack and
retry. Successful exit invalidates subsequent service calls on this context.
After a failed exit, obey the firmware's restricted retry rules: do not log,
allocate, or call other protocols through this adapter.

`mulib_efi_memory_at` decodes the standard little-endian 40-byte prefix at each
stride (including extended strides), without casting packed structs. Invalid
stride, partial descriptor, null arguments and out-of-range indices return -1.
The caller must interpret memory types/attributes and reserve live allocations.

`mulib_efi_bios` invokes only the supplied thunk. It cannot issue real-mode BIOS
interrupts directly from long mode. The thunk must manage CPU mode, segment
registers, low-memory buffers and interrupt state; register flags include carry.
Missing adapters return -2. No CSM, secure boot implementation, disk driver,
firmware entry point or mode transition assembly is included.

## Direct native binding

`mulib_efi_bind` binds a caller-owned `mulib_efi_native_t` containing an image
handle, native GetMemoryMap, ExitBootServices and optional LocateProtocol pointers.
These are real ABI calls: GCC/Clang x64 pointers use `ms_abi`. Assign pointers from
an SDK's validated boot-services table. No custom trampoline is needed. Both
context objects must remain alive through exit.

The wrapper handles UINTN-sized map keys, BUFFER_TOO_SMALL and stale-key
INVALID_PARAMETER on ExitBootServices. Other EFI errors return -3 and preserve
the exact native status in `last_status`. Warnings count as success. The native
descriptor version is available in `descriptor_version`. Rebinding an exited
context does not restore services; never reuse it after firmware exit.
