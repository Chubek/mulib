/** @file mulib-acpi.h
 * @brief Bounds-checked ACPI RSDP/SDT validation and root table address iteration.
 * Define MULIB_IMPL_ACPI once. Physical addresses are never dereferenced.
 */
#ifndef MULIB_ACPI_H
#define MULIB_ACPI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Validate RSDP signature, revision-dependent length and checksums.
 * Returns RSDT (v1) or XSDT (v2+) address and corresponding entry width 4 or 8.
 */
int mulib_acpi_rsdp(const void *data, size_t bytes, uint64_t *root, unsigned *width);
/** @brief Validate a mapped SDT (36-byte header, length, checksum, optional 4-byte signature). */
int mulib_acpi_sdt(const void *data, size_t bytes, const char *signature, size_t *length);
/** @brief Extract an address from a validated RSDT/XSDT. Returns -1 on invalid input. */
int mulib_acpi_root_at(const void *data, size_t bytes, size_t index, uint64_t *address);
/** @brief Validate MADT and iterate variable-size records. cursor starts at 44;
 * returns 0 record, 1 end, -1 malformed. Returned record borrows table storage.
 */
int mulib_acpi_madt_next(const void *data, size_t bytes, size_t *cursor,
                         const uint8_t **record, size_t *record_bytes);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ACPI) && !defined(MULIB_ACPI_IMPLEMENTED)
#define MULIB_ACPI_IMPLEMENTED
int mulib_acpi_madt_next(const void *data, size_t bytes, size_t *cursor,
                         const uint8_t **record, size_t *record_bytes) {
    const uint8_t *p = (const uint8_t *)data; size_t n, at, length;
    if (!cursor || !record || !record_bytes || mulib_acpi_sdt(data, bytes, "APIC", &n) || n < 44) return -1;
    /* Walk from the beginning to reject cursors into the middle of records. */
    at = 44;
    while (at < *cursor && at < n) {
        if (n - at < 2 || p[at + 1] < 2 || p[at + 1] > n - at) return -1;
        at += p[at + 1];
    }
    if (at != *cursor || at > n) return -1;
    if (at == n) { *record = NULL; *record_bytes = 0; return 1; }
    if (n - at < 2) return -1;
    length = p[at + 1]; if (length < 2 || length > n - at) return -1;
    /* Fixed minimums for local APIC, IO APIC, ISO, LAPIC override, x2APIC. */
    if ((p[at] == 0 && length < 8) || (p[at] == 1 && length < 12) ||
        (p[at] == 2 && length < 10) || (p[at] == 5 && length < 12) ||
        (p[at] == 9 && length < 16)) return -1;
    *record = p + at; *record_bytes = length; *cursor = at + length; return 0;
}
static uint64_t mulib_acpi_le_(const uint8_t *p, unsigned n) {
    unsigned i; uint64_t v = 0; for (i = 0; i < n; ++i) v |= (uint64_t)p[i] << (8 * i); return v;
}
static int mulib_acpi_sum_(const uint8_t *p, size_t n) {
    uint8_t sum = 0; size_t i; for (i = 0; i < n; ++i) sum = (uint8_t)(sum + p[i]); return sum == 0;
}
int mulib_acpi_rsdp(const void *data, size_t bytes, uint64_t *root, unsigned *width) {
    const uint8_t *p = (const uint8_t *)data; const char *sig = "RSD PTR "; unsigned i; uint32_t n;
    if (!p || !root || !width || bytes < 20) return -1;
    for (i = 0; i < 8; ++i) if (p[i] != (uint8_t)sig[i]) return -1;
    if (!mulib_acpi_sum_(p, 20)) return -1;
    if (p[15] >= 2) {
        if (bytes < 36) return -1;
        n = (uint32_t)mulib_acpi_le_(p + 20, 4);
        if (n < 36 || n > bytes || !mulib_acpi_sum_(p, n)) return -1;
        *root = mulib_acpi_le_(p + 24, 8); *width = 8;
    } else { *root = mulib_acpi_le_(p + 16, 4); *width = 4; }
    return 0;
}
int mulib_acpi_sdt(const void *data, size_t bytes, const char *sig, size_t *length) {
    const uint8_t *p = (const uint8_t *)data; size_t n; unsigned i;
    if (!p || !length || bytes < 36) return -1;
    n = (size_t)mulib_acpi_le_(p + 4, 4); if (n < 36 || n > bytes) return -1;
    if (sig) for (i = 0; i < 4; ++i) if (p[i] != (uint8_t)sig[i]) return -1;
    if (!mulib_acpi_sum_(p, n)) return -1;
    *length = n; return 0;
}
int mulib_acpi_root_at(const void *data, size_t bytes, size_t index, uint64_t *address) {
    const uint8_t *p = (const uint8_t *)data; size_t n; unsigned w;
    if (!address || mulib_acpi_sdt(data, bytes, NULL, &n)) return -1;
    if (p[0] == 'X' && p[1] == 'S' && p[2] == 'D' && p[3] == 'T') w = 8;
    else if (p[0] == 'R' && p[1] == 'S' && p[2] == 'D' && p[3] == 'T') w = 4;
    else return -1;
    if ((n - 36) % w || index >= (n - 36) / w) return -1;
    *address = mulib_acpi_le_(p + 36 + index * w, w); return 0;
}
#endif
