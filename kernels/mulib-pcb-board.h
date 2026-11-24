/** @file mulib-pcb-board.h
 * @brief Owned flat straight-track PCB container with 1..64 declared copper layers and validated
 * real geometry.
 * @details C11 single-header implementation; define MULIB_IMPL_PCB_BOARD in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_PCB_BOARD_H
#define MULIB_PCB_BOARD_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-pcb-geometry.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_pcb_board {
    uint32_t layers;
    mulib_pcb_segment_t *tracks;
    size_t count, cap;
} mulib_pcb_board_t;
int mulib_pcb_board_init(mulib_pcb_board_t *b, uint32_t layers);
int mulib_pcb_board_add_trace(mulib_pcb_board_t *b, mulib_pcb_segment_t trace);
void mulib_pcb_board_free(mulib_pcb_board_t *b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PCB_BOARD_H */
#if defined(MULIB_IMPL_PCB_BOARD) && !defined(MULIB_PCB_BOARD_H_IMPLEMENTED)
#define MULIB_PCB_BOARD_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_board_init(mulib_pcb_board_t *b, uint32_t layers) {
    if (!b || layers < 1 || layers > 64)
        return 0;
    memset(b, 0, sizeof *b);
    b->layers = layers;
    return 1;
}
int mulib_pcb_board_add_trace(mulib_pcb_board_t *b, mulib_pcb_segment_t t) {
    if (!b || !b->layers || !isfinite(t.width) || t.width <= 0 || !isfinite(t.a.x) ||
        !isfinite(t.a.y) || !isfinite(t.b.x) || !isfinite(t.b.y))
        return 0;
    if (b->count == b->cap) {
        size_t n = b->cap ? b->cap * 2 : 16;
        if (n <= b->cap || n > SIZE_MAX / sizeof *b->tracks)
            return 0;
        void *p = realloc(b->tracks, n * sizeof *b->tracks);
        if (!p)
            return 0;
        b->tracks = p;
        b->cap = n;
    }
    b->tracks[b->count++] = t;
    return 1;
}
void mulib_pcb_board_free(mulib_pcb_board_t *b) {
    if (!b)
        return;
    free(b->tracks);
    memset(b, 0, sizeof *b);
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_PCB_BOARD */
