/** @file mulib-fpga-route.h
 * @brief Shortest Manhattan four-neighbour path on a rectangular 2D obstacle grid, BFS with
 * deterministic neighbour ordering.
 * @details C11 single-header implementation; define MULIB_IMPL_FPGA_ROUTE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FPGA_ROUTE_H
#define MULIB_FPGA_ROUTE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_fpga_grid {
    uint16_t width, height;
    const uint8_t *blocked;
} mulib_fpga_grid_t;
int mulib_fpga_route_bfs(mulib_fpga_grid_t grid, uint16_t sx, uint16_t sy, uint16_t gx, uint16_t gy,
                         uint16_t *path, size_t capacity, size_t *out_length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FPGA_ROUTE_H */
#if defined(MULIB_IMPL_FPGA_ROUTE) && !defined(MULIB_FPGA_ROUTE_H_IMPLEMENTED)
#define MULIB_FPGA_ROUTE_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_route_bfs(mulib_fpga_grid_t g, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey,
                         uint16_t *path, size_t cap, size_t *out_len) {
    size_t n = (size_t)g.width * g.height;
    if (!g.width || !g.height || n > 65536 || !g.blocked || sx >= g.width || ex >= g.width ||
        sy >= g.height || ey >= g.height || !path || !out_len)
        return 0;
    uint32_t *queue = malloc(n * sizeof(uint32_t)), *prev = malloc(n * sizeof(uint32_t));
    if (!queue || !prev) {
        free(queue);
        free(prev);
        return 0;
    }
    for (size_t i = 0; i < n; i++)
        prev[i] = UINT32_MAX;
    size_t start = (size_t)sy * g.width + sx, goal = (size_t)ey * g.width + ex, head = 0, tail = 0;
    int ok = 0;
    if (!g.blocked[start] && !g.blocked[goal]) {
        queue[tail++] = (uint32_t)start;
        prev[start] = (uint32_t)start;
        while (head < tail && prev[goal] == UINT32_MAX) {
            size_t at = queue[head++], x = at % g.width, y = at / g.width;
            size_t cand[4] = {x ? at - 1 : at, x + 1 < g.width ? at + 1 : at, y ? at - g.width : at,
                              y + 1 < g.height ? at + g.width : at};
            for (unsigned k = 0; k < 4; k++) {
                size_t b = cand[k];
                if (b != at && !g.blocked[b] && prev[b] == UINT32_MAX) {
                    prev[b] = (uint32_t)at;
                    queue[tail++] = (uint32_t)b;
                }
            }
        }
        if (prev[goal] != UINT32_MAX) {
            size_t count = 0;
            for (size_t i = goal; i != start; i = prev[i])
                count++;
            count++;
            if (count <= cap) {
                size_t i = goal;
                for (size_t k = count; k-- > 0;) {
                    path[k] = (uint16_t)i;
                    if (i != start)
                        i = prev[i];
                }
                *out_len = count;
                ok = 1;
            }
        }
    }
    free(queue);
    free(prev);
    return ok;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_FPGA_ROUTE */
