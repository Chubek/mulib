/** @file mulib-hwinfo.h
 * @brief x86 CPUID inventory with injectable queries for hosted testing.
 * Define MULIB_IMPL_HWINFO once. No OS, allocation or privileged instructions.
 */
#ifndef MULIB_HWINFO_H
#define MULIB_HWINFO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_hwinfo_query_t)(void *, uint32_t, uint32_t, uint32_t[4]);
typedef struct {
    char vendor[13], brand[49];
    uint32_t family, model, stepping, features_ecx, features_edx;
    unsigned physical_bits, virtual_bits;
} mulib_hwinfo_cpu_t;
/** @brief Native CPUID on GCC/Clang x86; returns -1 elsewhere. */
int mulib_hwinfo_native(void *context, uint32_t leaf, uint32_t subleaf, uint32_t out[4]);
/** @brief Fill CPU inventory, zeroing unavailable optional fields; query NULL uses native. */
int mulib_hwinfo_cpu(mulib_hwinfo_query_t query, void *context, mulib_hwinfo_cpu_t *out);
typedef struct {
    uint64_t bytes;
    uint32_t line_bytes, ways, partitions;
    uint64_t sets;
    unsigned level, type, sharing_threads;
} mulib_hwinfo_cache_t;
/** @brief Decode deterministic cache CPUID leaf 4 (Intel) or 0x8000001d (AMD).
 * Caller chooses a vendor-appropriate leaf; index is subleaf. 0 result,
 * 1 end, -1 unsupported/invalid/overflow. Output unchanged on failure.
 */
int mulib_hwinfo_cache(mulib_hwinfo_query_t query, void *context, uint32_t leaf,
                        uint32_t index, mulib_hwinfo_cache_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWINFO) && !defined(MULIB_HWINFO_IMPLEMENTED)
#define MULIB_HWINFO_IMPLEMENTED
int mulib_hwinfo_cache(mulib_hwinfo_query_t q, void *ctx, uint32_t leaf,
                        uint32_t index, mulib_hwinfo_cache_t *out) {
    uint32_t r[4]; uint64_t size; mulib_hwinfo_cache_t c;
    if (!out || (leaf != 4 && leaf != UINT32_C(0x8000001d))) return -1;
    if (!q) q = mulib_hwinfo_native;
    if (q(ctx, leaf & UINT32_C(0x80000000), 0, r) || r[0] < leaf || q(ctx, leaf, index, r)) return -1;
    if (!(r[0] & 31)) return 1;
    c.type = r[0] & 31; c.level = (r[0] >> 5) & 7;
    if (c.type > 3 || !c.level) return -1;
    c.sharing_threads = ((r[0] >> 14) & 4095) + 1;
    c.line_bytes = (r[1] & 4095) + 1; c.partitions = ((r[1] >> 12) & 1023) + 1;
    c.ways = (r[1] >> 22) + 1; c.sets = (uint64_t)r[2] + 1;
    size = (uint64_t)c.line_bytes * c.partitions * c.ways;
    if (size > UINT64_MAX / c.sets) return -1;
    c.bytes = size * c.sets; *out = c; return 0;
}
#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
#include <cpuid.h>
#endif
int mulib_hwinfo_native(void *context, uint32_t leaf, uint32_t subleaf, uint32_t out[4]) {
    (void)context; if (!out) return -1;
#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
    if (__get_cpuid_max(leaf & UINT32_C(0x80000000), 0) < leaf) return -1;
    __cpuid_count(leaf, subleaf, out[0], out[1], out[2], out[3]); return 0;
#else
    (void)leaf; (void)subleaf; return -1;
#endif
}
static void mulib_hwinfo_word_(char *p, uint32_t v) {
    unsigned j; for (j = 0; j < 4; ++j) p[j] = (char)(v >> (j * 8));
}
int mulib_hwinfo_cpu(mulib_hwinfo_query_t q, void *ctx, mulib_hwinfo_cpu_t *out) {
    uint32_t r[4], max, ext = 0, base; unsigned i, j;
    mulib_hwinfo_cpu_t empty = {{0}, {0}, 0, 0, 0, 0, 0, 0, 0};
    if (!out) return -1;
    *out = empty; if (!q) q = mulib_hwinfo_native;
    if (q(ctx, 0, 0, r)) return -1;
    max = r[0]; mulib_hwinfo_word_(out->vendor, r[1]);
    mulib_hwinfo_word_(out->vendor + 4, r[3]); mulib_hwinfo_word_(out->vendor + 8, r[2]);
    if (max >= 1 && !q(ctx, 1, 0, r)) {
        base = (r[0] >> 8) & 15; out->family = base;
        if (base == 15) out->family += (r[0] >> 20) & 255;
        out->model = (r[0] >> 4) & 15;
        if (base == 6 || base == 15) out->model |= ((r[0] >> 16) & 15) << 4;
        out->stepping = r[0] & 15; out->features_ecx = r[2]; out->features_edx = r[3];
    }
    if (!q(ctx, UINT32_C(0x80000000), 0, r)) ext = r[0];
    if (ext >= UINT32_C(0x80000004)) for (i = 0; i < 3; ++i) {
        if (q(ctx, UINT32_C(0x80000002) + i, 0, r)) return -1;
        for (j = 0; j < 4; ++j) mulib_hwinfo_word_(out->brand + i * 16 + j * 4, r[j]);
    }
    if (ext >= UINT32_C(0x80000008) && !q(ctx, UINT32_C(0x80000008), 0, r)) {
        out->physical_bits = r[0] & 255; out->virtual_bits = (r[0] >> 8) & 255;
    }
    return 0;
}
#endif
