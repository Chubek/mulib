/** @file mulib-paging.h
 * @brief Pure x86-64 four/five-level page address and 4 KiB PTE helpers.
 * Define MULIB_IMPL_PAGING once. Does not allocate tables or modify CR3/TLBs.
 */
#ifndef MULIB_PAGING_H
#define MULIB_PAGING_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_PAGING_PRESENT UINT64_C(1)
#define MULIB_PAGING_WRITE UINT64_C(2)
#define MULIB_PAGING_USER UINT64_C(4)
#define MULIB_PAGING_NX (UINT64_C(1) << 63)
/** @brief True for a canonical 48- or 57-bit virtual address. */
int mulib_paging_canonical(uint64_t address, unsigned bits);
/** @brief Index at level 1..5; UINT32_MAX for invalid level. */
uint32_t mulib_paging_index(uint64_t address, unsigned level);
/** @brief Construct 4 KiB leaf PTE, rejecting alignment, physical width and flags.
 * bits is CPUID physical width 12..52; NX availability is caller responsibility.
 */
int mulib_paging_entry(uint64_t physical, unsigned bits, uint64_t flags, uint64_t *entry);
typedef int (*mulib_paging_read_t)(void *, uint64_t table_physical, unsigned index, uint64_t *entry);
typedef struct { uint64_t physical, page_bytes; unsigned writable, user, executable; } mulib_paging_translation_t;
/** @brief Walk existing 4/5-level tables via a physical-read callback. Supports
 * 4 KiB, 2 MiB and 1 GiB leaves; combines permissions across all levels.
 * 0 mapped, 1 not present, -1 malformed, callback errors propagate.
 * Caller stabilizes tables and checks CPU huge-page/NX feature availability.
 */
int mulib_paging_translate(mulib_paging_read_t read, void *context, uint64_t root,
                           unsigned levels, unsigned physical_bits, uint64_t address,
                           mulib_paging_translation_t *result);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PAGING) && !defined(MULIB_PAGING_IMPLEMENTED)
#define MULIB_PAGING_IMPLEMENTED
int mulib_paging_translate(mulib_paging_read_t read, void *ctx, uint64_t root,
                           unsigned levels, unsigned bits, uint64_t address,
                           mulib_paging_translation_t *result) {
    unsigned level; uint64_t e, mask, physical, size, reserved; int rc;
    mulib_paging_translation_t out = {0, 0, 1, 1, 1};
    if (!read || !result || (levels != 4 && levels != 5) || bits < 12 || bits > 52 ||
        !mulib_paging_canonical(address, levels == 4 ? 48 : 57) || (root & 4095) || (root >> bits)) return -1;
    mask = ((UINT64_C(1) << bits) - 1) & ~UINT64_C(4095);
    for (level = levels; level; --level) {
        rc = read(ctx, root, mulib_paging_index(address, level), &e); if (rc) return rc;
        if (!(e & 1)) return 1;
        if (e & (UINT64_C(0x000ffffffffff000) & ~mask)) return -1;
        out.writable &= (unsigned)((e >> 1) & 1); out.user &= (unsigned)((e >> 2) & 1);
        out.executable &= !(e & MULIB_PAGING_NX); physical = e & mask;
        if (level == 1 || (e & 128)) {
            if (level > 3) return -1;
            size = UINT64_C(1) << (12 + (level - 1) * 9);
            /* Large pages allow PAT at bit 12 but reserve other low address bits. */
            reserved = level == 1 ? 0 : (size - 1) & ~UINT64_C(8191);
            if (physical & reserved) return -1;
            out.physical = (physical & ~(size - 1)) | (address & (size - 1));
            out.page_bytes = size; *result = out; return 0;
        }
        root = physical;
    }
    return -1;
}
int mulib_paging_canonical(uint64_t a, unsigned bits) {
    uint64_t top; if (bits != 48 && bits != 57) return 0;
    top = a >> (bits - 1); return top == 0 || top == (UINT64_MAX >> (bits - 1));
}
uint32_t mulib_paging_index(uint64_t a, unsigned level) {
    return level < 1 || level > 5 ? UINT32_MAX : (uint32_t)((a >> (12 + 9 * (level - 1))) & 511);
}
int mulib_paging_entry(uint64_t p, unsigned bits, uint64_t flags, uint64_t *out) {
    if (!out || bits < 12 || bits > 52 || (p & 4095) || (p >> bits) ||
        (flags & ~(UINT64_C(0xfff) | MULIB_PAGING_NX))) return -1;
    *out = p | flags; return 0;
}
#endif
