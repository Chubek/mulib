/** @file mulib-ecad-pcb-sync.h
 * @brief Stable bijective object-ID lookup between 256 schematic and PCB objects; no object model
 * introspection, reconciliation or automatic synchronization.
 * @details C11 single-header implementation; define MULIB_IMPL_ECAD_PCB_SYNC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ECAD_PCB_SYNC_H
#define MULIB_ECAD_PCB_SYNC_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_ecad_pcb_link {
    uint64_t schematic_id, pcb_id;
} mulib_ecad_pcb_link_t;
typedef struct mulib_ecad_pcb_sync {
    mulib_ecad_pcb_link_t links[256];
    size_t count;
} mulib_ecad_pcb_sync_t;
int mulib_ecad_pcb_sync_add(mulib_ecad_pcb_sync_t *s, uint64_t schematic_id, uint64_t pcb_id);
uint64_t mulib_ecad_pcb_sync_lookup(const mulib_ecad_pcb_sync_t *s, uint64_t schematic_id);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ECAD_PCB_SYNC_H */
#if defined(MULIB_IMPL_ECAD_PCB_SYNC) && !defined(MULIB_ECAD_PCB_SYNC_H_IMPLEMENTED)
#define MULIB_ECAD_PCB_SYNC_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_pcb_sync_add(mulib_ecad_pcb_sync_t *s, uint64_t a, uint64_t b) {
    if (!s || !a || !b || s->count >= 256)
        return 0;
    for (size_t i = 0; i < s->count; i++)
        if (s->links[i].schematic_id == a || s->links[i].pcb_id == b)
            return 0;
    s->links[s->count++] = (mulib_ecad_pcb_link_t){a, b};
    return 1;
}
uint64_t mulib_ecad_pcb_sync_lookup(const mulib_ecad_pcb_sync_t *s, uint64_t a) {
    if (!s || s->count > 256)
        return 0;
    for (size_t i = 0; i < s->count; i++)
        if (s->links[i].schematic_id == a)
            return s->links[i].pcb_id;
    return 0;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_ECAD_PCB_SYNC */
