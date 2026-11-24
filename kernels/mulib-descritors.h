/** @file mulib-descritors.h
 * @brief x86-64 GDT, IDT, TSS and descriptor-pointer byte encoders.
 * Requested filename spelling retained. Define MULIB_IMPL_DESCRITORS once.
 * Buffers are hardware layouts, not compiler-packed structs. No implicit loads.
 */
#ifndef MULIB_DESCRITORS_H
#define MULIB_DESCRITORS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Encode an 8-byte segment; flags is the high nibble (G/D/L/AVL).
 * Rejects L+D and L on non-code descriptors. limit is the raw 20-bit limit.
 */
int mulib_descritors_segment(uint8_t out[8], uint32_t base, uint32_t limit, uint8_t access, uint8_t flags);
/** @brief Encode a 64-bit interrupt/trap gate; dpl 0..3, ist 0..7, type 14 or 15. */
int mulib_descritors_gate(uint8_t out[16], uint64_t handler, uint16_t selector, unsigned ist, unsigned dpl, unsigned type);
/** @brief Encode an available 64-bit TSS system descriptor, limit at least 103. */
int mulib_descritors_tss(uint8_t out[16], uint64_t base, uint32_t limit);
/** @brief Encode a 10-byte GDTR/IDTR pseudo-descriptor; size 1..65536 bytes. */
int mulib_descritors_pointer(uint8_t out[10], uint64_t base, uint32_t bytes);
/** @brief Initialize a 104-byte x64 TSS with RSP0 and seven optional IST pointers.
 * Rejects noncanonical 48/57-bit addresses. I/O bitmap is disabled (offset 104).
 */
int mulib_descritors_tss_init(uint8_t out[104], uint64_t rsp0, const uint64_t ist[7], unsigned address_bits);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DESCRITORS) && !defined(MULIB_DESCRITORS_IMPLEMENTED)
#define MULIB_DESCRITORS_IMPLEMENTED
int mulib_descritors_tss_init(uint8_t out[104], uint64_t rsp0, const uint64_t ist[7], unsigned bits) {
    unsigned i, j; uint64_t addresses[8], top;
    if (!out || (bits != 48 && bits != 57)) return -1;
    addresses[0] = rsp0;
    for (i = 0; i < 7; ++i) addresses[i + 1] = ist ? ist[i] : 0;
    for (i = 0; i < 8; ++i) {
        top = addresses[i] >> (bits - 1);
        if (top && top != (UINT64_MAX >> (bits - 1))) return -1;
    }
    for (i = 0; i < 104; ++i) out[i] = 0;
    for (j = 0; j < 8; ++j) out[4 + j] = (uint8_t)(rsp0 >> (j * 8));
    for (i = 0; i < 7; ++i) for (j = 0; j < 8; ++j)
        out[36 + i * 8 + j] = (uint8_t)(addresses[i + 1] >> (j * 8));
    out[102] = 104; return 0;
}
static void mulib_descritors_store_(uint8_t *p, uint64_t v, unsigned n) {
    unsigned i; for (i = 0; i < n; ++i) p[i] = (uint8_t)(v >> (i * 8));
}
int mulib_descritors_segment(uint8_t o[8], uint32_t b, uint32_t l, uint8_t a, uint8_t f) {
    if (!o || l > 0xfffff || f > 15 || ((f & 2) && ((f & 4) || (a & 0x18) != 0x18))) return -1;
    mulib_descritors_store_(o, l, 2); mulib_descritors_store_(o + 2, b, 3);
    o[5] = a; o[6] = (uint8_t)((l >> 16) | (f << 4)); o[7] = (uint8_t)(b >> 24); return 0;
}
int mulib_descritors_gate(uint8_t o[16], uint64_t h, uint16_t s, unsigned ist, unsigned dpl, unsigned type) {
    if (!o || !s || (s & 7) || ist > 7 || dpl > 3 || (type != 14 && type != 15)) return -1;
    mulib_descritors_store_(o, h, 2); mulib_descritors_store_(o + 2, s, 2);
    o[4] = (uint8_t)ist; o[5] = (uint8_t)(0x80 | dpl << 5 | type);
    mulib_descritors_store_(o + 6, h >> 16, 6); mulib_descritors_store_(o + 12, 0, 4); return 0;
}
int mulib_descritors_tss(uint8_t o[16], uint64_t b, uint32_t l) {
    if (!o || l < 103 || l > 0xfffff) return -1;
    (void)mulib_descritors_segment(o, (uint32_t)b, l, 0x89, 0);
    mulib_descritors_store_(o + 8, b >> 32, 4); mulib_descritors_store_(o + 12, 0, 4); return 0;
}
int mulib_descritors_pointer(uint8_t o[10], uint64_t b, uint32_t n) {
    if (!o || !n || n > 65536) return -1;
    mulib_descritors_store_(o, n - 1, 2); mulib_descritors_store_(o + 2, b, 8); return 0;
}
#endif
