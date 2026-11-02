# mulib-acpi

Define `MULIB_IMPL_ACPI` once; no dependencies, allocation or OS calls.
[Example](../examples/modules/acpi.c).

All inputs are borrowed **already mapped** byte buffers. Functions return zero
on success and -1 for null, truncated, invalid-signature, length or checksum
failures. Outputs are written only on success. No physical address is dereferenced
and no firmware search or AML interpreter is included.

`mulib_acpi_rsdp` checks the 8-byte signature and 20-byte checksum, plus v2+
length (at least 36, within supplied buffer) and extended checksum. It returns
RSDT address/width 4 for v1, XSDT address/width 8 for v2+. A zero XSDT address
is returned as zero; choosing an RSDT fallback is caller policy.

`mulib_acpi_sdt` validates the 36-byte header, bounded advertised length and full
checksum. Optional signature points to four readable bytes; output is advertised
length, not the mapped capacity. `mulib_acpi_root_at` additionally requires RSDT
or XSDT signature, whole entries, and in-range index, then decodes an address.
Callers map each referenced table independently and validate it before use.

Checksums provide corruption detection, not authentication. Bounds checks cannot
protect against a concurrently modified firmware buffer; copy or stabilize the
mapping before validation. No FADT/MADT semantic decoder is claimed.

`mulib_acpi_madt_next` validates an APIC table and iterates bounded MADT records.
Initialize cursor to 44. Returns 0 with a borrowed record/length, 1 at end (null
record, zero length), or -1 malformed. It rejects mid-record cursors, zero/short
records, truncation and undersized standard LAPIC/IOAPIC/ISO/override/x2APIC
records. Unknown types are exposed for forward compatibility. Fields inside
records must be decoded as little-endian bytes; no MMIO is touched. Each call
revalidates checksum and cursor boundaries; retain a stable buffer.
