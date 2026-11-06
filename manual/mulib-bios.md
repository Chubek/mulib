# mulib-bios

Define `MULIB_IMPL_BIOS` once. No transitive switches or allocations.
[Example](../examples/modules/bios.c).

## Interfaces and wire formats

This module covers PC BIOS conventions plus ACPI E820 and EDD packet formats.
There is no single universal BIOS ABI across all machines. C structs here are
host-side models; wire records are explicitly little-endian, with no packing or
unaligned-cast assumptions.

* `mulib_bios_interrupt` invokes the supplied real-mode thunk and returns its
  error, or `CARRY` when the returned carry bit is set. Registers retain firmware
  outputs on carry. A missing thunk returns `UNSUPPORTED`. It never issues a
  BIOS interrupt from protected/long mode.
* `mulib_bios_e820` decodes the 20-byte legacy or 24-byte extended record prefix.
  Legacy records default the enabled attribute to one. Disabled, zero-length and
  unknown-type records remain visible. Address wrap is rejected. Output is atomic.
* `mulib_bios_e820_next` prepares EAX=E820h, EDX=`SMAP`, ECX=24 and ES:DI for a
  mapped low-memory scratch buffer, initializes extended attributes, checks the
  returned signature and record size, and updates the continuation. Start at
  zero; stop when a successful call returns zero continuation or result 1 denotes
  carry at the end. Bound calls independently: hostile firmware can cycle tokens.
* `mulib_bios_dap` serializes a 16-byte EDD disk address packet with a 64-bit LBA,
  1–127 sectors and segment:offset buffer. It rejects arithmetic wrap and buffers
  crossing 64 KiB or 1 MiB. Supply the actual logical sector size. The caller must
  also place the packet in aligned, BIOS-addressable memory, probe EDD support
  with INT 13h/AH=41h, then invoke AH=42h/43h with DS:SI pointing to it. It does
  not pretend a host pointer is a real-mode address.
* `mulib_bios_partition` decodes four primary MBR entries at offset 446, checking
  status and the signature. CHS bytes and empty entries are preserved. Extended
  partitions and GPT are not traversed. `sector_valid` checks only `55 AA` at 510.

Results are 0 success; `INVALID=-1`, `UNSUPPORTED=-2`, `CARRY=-3`, `FORMAT=-4`.
Callbacks may return their own error codes. Buffers/context are caller-owned and
must remain valid through synchronous calls. Scratch must not alias outputs.
These helpers do not install a thunk, change CPU mode, allocate low memory or
infer bootability from a signature.

## References and tests

ACPI 6.5 §15.1, INT 15h E820h System Address Map; BIOS Enhanced Disk Drive
Services (EDD), 16-byte disk address packet; PC/AT MBR conventions. UEFI-hosted
programs cannot assume legacy BIOS services exist. Tests in
`tests/test_boot_standards.c` check exact packet bytes, boundary failures,
E820 register contracts, MBR decoding and explicit chainloader handoff.
Hardware/real-mode transitions remain integration responsibilities.
