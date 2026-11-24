/** @file mulib-soc-memorymap.h
 * @brief Bounded 128-entry nonoverlapping physical address map with overflow-checked half-open
 * regions, device IDs and offset lookup.
 * @details C11 single-header implementation; define MULIB_IMPL_SOC_MEMORYMAP in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SOC_MEMORYMAP_H
#define MULIB_SOC_MEMORYMAP_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_soc_range {
    uint64_t start, length;
    uint32_t device;
} mulib_soc_range_t;
typedef struct mulib_soc_memorymap {
    mulib_soc_range_t ranges[128];
    size_t count;
} mulib_soc_memorymap_t;
int mulib_soc_memorymap_add(mulib_soc_memorymap_t *map, uint64_t start, uint64_t length,
                            uint32_t device);
int mulib_soc_memorymap_lookup(const mulib_soc_memorymap_t *map, uint64_t address, uint32_t *device,
                               uint64_t *offset);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SOC_MEMORYMAP_H */
#if defined(MULIB_IMPL_SOC_MEMORYMAP) && !defined(MULIB_SOC_MEMORYMAP_H_IMPLEMENTED)
#define MULIB_SOC_MEMORYMAP_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_soc_memorymap_add(mulib_soc_memorymap_t *m, uint64_t start, uint64_t len,
                            uint32_t device) {
    if (!m || !len || !device || m->count >= 128 || start > UINT64_MAX - (len - 1))
        return 0;
    uint64_t end = start + len - 1;
    for (size_t i = 0; i < m->count; i++) {
        mulib_soc_range_t r = m->ranges[i];
        if (start <= r.start + r.length - 1 && r.start <= end)
            return 0;
    }
    m->ranges[m->count++] = (mulib_soc_range_t){start, len, device};
    return 1;
}
int mulib_soc_memorymap_lookup(const mulib_soc_memorymap_t *m, uint64_t addr, uint32_t *device,
                               uint64_t *offset) {
    if (!m || m->count > 128 || !device || !offset)
        return 0;
    for (size_t i = 0; i < m->count; i++) {
        mulib_soc_range_t r = m->ranges[i];
        if (addr >= r.start && addr - r.start < r.length) {
            *device = r.device;
            *offset = addr - r.start;
            return 1;
        }
    }
    return 0;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SOC_MEMORYMAP */
