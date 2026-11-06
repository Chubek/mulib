/** @file mulib-bios.h
 * @brief PC BIOS thunk interface, E820 records, EDD packets and MBR partitions.
 * Define MULIB_IMPL_BIOS once. Wire data is decoded without packed C structs.
 */
#ifndef MULIB_BIOS_H
#define MULIB_BIOS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_BIOS_OK = 0, MULIB_BIOS_INVALID = -1, MULIB_BIOS_UNSUPPORTED = -2,
       MULIB_BIOS_CARRY = -3, MULIB_BIOS_FORMAT = -4 };
typedef struct { uint32_t eax, ebx, ecx, edx, esi, edi, ebp; uint16_t ds, es, flags; } mulib_bios_regs_t;
/** Caller supplies real-mode transition and low-memory mapping. A zero thunk
 * result means registers are valid; interrupt() separately tests carry. */
typedef struct { void *context; int (*interrupt)(void *, uint8_t, mulib_bios_regs_t *); } mulib_bios_t;
typedef struct { uint64_t base, length; uint32_t type, attributes; } mulib_bios_memory_t;
typedef struct { uint8_t status, type, first_chs[3], last_chs[3]; uint32_t first_lba, sectors; } mulib_bios_partition_t;
/** @brief Call supplied thunk; preserve returned registers even on carry. */
int mulib_bios_interrupt(mulib_bios_t *bios, uint8_t interrupt, mulib_bios_regs_t *regs);
/** @brief Decode 20-byte legacy or 24+-byte ACPI E820 record. Legacy attributes
 * default to enabled (1). Zero length and disabled records remain visible. */
int mulib_bios_e820(const void *record, size_t bytes, mulib_bios_memory_t *out);
/** @brief INT 15h/E820h query using 24 bytes of caller-mapped low memory.
 * physical is the real-mode address of scratch; it must fit below 1 MiB.
 * Start continuation at zero; stop after a successful result sets it to zero,
 * or return 1 (carry after a nonzero continuation). Caller bounds iterations.
 * Disabled/zero-length records remain visible for caller policy. */
int mulib_bios_e820_next(mulib_bios_t *bios, uint32_t *continuation,
    void *scratch, uint32_t physical, mulib_bios_memory_t *out);
/** @brief Build 16-byte EDD disk-address packet for AH=42h/43h. count 1..127;
 * conventional-memory buffer must not cross a 64 KiB segment or 1 MiB.
 * sector_bytes describes the drive's logical sector size. Output atomic. */
int mulib_bios_dap(void *out, size_t capacity, uint64_t lba, unsigned count,
                   uint16_t segment, uint16_t offset, size_t sector_bytes);
/** @brief Decode one of four primary MBR entries; validate signature and status.
 * CHS bytes preserved, including saturated values; no extended/GPT traversal. */
int mulib_bios_partition(const void *sector, size_t bytes, size_t index, mulib_bios_partition_t *out);
/** @brief Recognize the 55 AA signature at byte 510; not a bootability test. */
int mulib_bios_sector_valid(const void *sector, size_t bytes);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BIOS) && !defined(MULIB_BIOS_IMPLEMENTED)
#define MULIB_BIOS_IMPLEMENTED
#include <string.h>
static uint64_t mulib_bios_le_(const uint8_t *p, unsigned n) {
    uint64_t v = 0; unsigned i; for (i = 0; i < n; ++i) v |= (uint64_t)p[i] << (8 * i); return v;
}
int mulib_bios_interrupt(mulib_bios_t *b, uint8_t interrupt, mulib_bios_regs_t *r) {
    int rc; if (!b || !r) return MULIB_BIOS_INVALID;
    if (!b->interrupt) return MULIB_BIOS_UNSUPPORTED;
    rc = b->interrupt(b->context, interrupt, r);
    return rc ? rc : (r->flags & 1) ? MULIB_BIOS_CARRY : 0;
}
int mulib_bios_e820(const void *record, size_t bytes, mulib_bios_memory_t *out) {
    const uint8_t *p = (const uint8_t *)record; mulib_bios_memory_t v;
    if (!p || !out || (bytes != 20 && bytes < 24)) return MULIB_BIOS_INVALID;
    v.base = mulib_bios_le_(p, 8); v.length = mulib_bios_le_(p + 8, 8);
    v.type = (uint32_t)mulib_bios_le_(p + 16, 4);
    v.attributes = bytes >= 24 ? (uint32_t)mulib_bios_le_(p + 20, 4) : 1;
    if (v.length && v.length - 1 > UINT64_MAX - v.base) return MULIB_BIOS_FORMAT;
    *out = v; return 0;
}
int mulib_bios_e820_next(mulib_bios_t *b, uint32_t *continuation,
    void *scratch, uint32_t physical, mulib_bios_memory_t *out) {
    mulib_bios_regs_t r; mulib_bios_memory_t v; int rc;
    if (!b || !continuation || !scratch || !out || physical > 0x100000u - 24) return MULIB_BIOS_INVALID;
    memset(&r, 0, sizeof(r)); memset(scratch, 0, 24); ((uint8_t *)scratch)[20] = 1;
    r.eax = 0xe820; r.edx = 0x534d4150; r.ecx = 24; r.ebx = *continuation;
    r.es = (uint16_t)(physical >> 4); r.edi = physical & 15;
    rc = mulib_bios_interrupt(b, 0x15, &r);
    if (rc == MULIB_BIOS_CARRY && *continuation) { *continuation = 0; return 1; }
    if (rc) return rc;
    if (r.eax != 0x534d4150 || (r.ecx != 20 && r.ecx != 24)) return MULIB_BIOS_FORMAT;
    rc = mulib_bios_e820(scratch, r.ecx, &v); if (rc) return rc;
    *out = v; *continuation = r.ebx; return 0;
}
int mulib_bios_dap(void *out, size_t capacity, uint64_t lba, unsigned count,
                   uint16_t segment, uint16_t offset, size_t sector_bytes) {
    uint8_t p[16] = {16, 0}; size_t bytes; unsigned i;
    if (!out || capacity < 16 || !count || count > 127 || !sector_bytes ||
        sector_bytes > 65536u / count || count - 1 > UINT64_MAX - lba) return MULIB_BIOS_INVALID;
    bytes = sector_bytes * count;
    if (bytes > 65536u - offset || (uint32_t)segment * 16 + offset > 0x100000u - bytes) return MULIB_BIOS_INVALID;
    p[2] = (uint8_t)count; p[4] = (uint8_t)offset; p[5] = (uint8_t)(offset >> 8);
    p[6] = (uint8_t)segment; p[7] = (uint8_t)(segment >> 8);
    for (i = 0; i < 8; ++i) p[8 + i] = (uint8_t)(lba >> (8 * i));
    memcpy(out, p, 16); return 0;
}
int mulib_bios_sector_valid(const void *sector, size_t bytes) {
    const uint8_t *p = (const uint8_t *)sector;
    return p && bytes >= 512 && p[510] == 0x55 && p[511] == 0xaa;
}
int mulib_bios_partition(const void *sector, size_t bytes, size_t index, mulib_bios_partition_t *out) {
    const uint8_t *p; mulib_bios_partition_t v;
    if (!out || index >= 4 || !mulib_bios_sector_valid(sector, bytes)) return MULIB_BIOS_INVALID;
    p = (const uint8_t *)sector + 446 + 16 * index;
    if (p[0] != 0 && p[0] != 0x80) return MULIB_BIOS_FORMAT;
    v.status = p[0]; v.type = p[4]; memcpy(v.first_chs, p + 1, 3); memcpy(v.last_chs, p + 5, 3);
    v.first_lba = (uint32_t)mulib_bios_le_(p + 8, 4); v.sectors = (uint32_t)mulib_bios_le_(p + 12, 4);
    *out = v; return 0;
}
#endif
