/** @file mulib-asic-sta.h
 * @brief Arrival-time max-plus propagation on a topologically ordered timing DAG with up to eight
 * predecessors per node, plus slack against a common deadline.
 * @details C11 single-header implementation; define MULIB_IMPL_ASIC_STA in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ASIC_STA_H
#define MULIB_ASIC_STA_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_asic_timing_node {
    double delay, arrival;
    uint16_t pred[8];
    uint8_t pred_count;
} mulib_asic_timing_node_t;
int mulib_asic_sta_propagate(mulib_asic_timing_node_t *nodes, size_t count, double *critical_path);
int mulib_asic_sta_slack(const mulib_asic_timing_node_t *nodes, size_t count, double deadline,
                         double *slack);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ASIC_STA_H */
#if defined(MULIB_IMPL_ASIC_STA) && !defined(MULIB_ASIC_STA_H_IMPLEMENTED)
#define MULIB_ASIC_STA_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_asic_sta_propagate(mulib_asic_timing_node_t *nodes, size_t count, double *critical) {
    if ((!nodes && count) || !critical || count > UINT16_MAX)
        return 0;
    double *arrival = count ? malloc(count * sizeof *arrival) : NULL;
    if (count && !arrival)
        return 0;
    double longest = 0;
    int ok = 1;
    for (size_t i = 0; i < count && ok; i++) {
        if (!isfinite(nodes[i].delay) || nodes[i].delay < 0 || nodes[i].pred_count > 8) {
            ok = 0;
            break;
        }
        double value = 0;
        for (size_t j = 0; j < nodes[i].pred_count; j++) {
            size_t p = nodes[i].pred[j];
            if (p >= i) {
                ok = 0;
                break;
            }
            value = fmax(value, arrival[p]);
        }
        if (!ok)
            break;
        arrival[i] = value + nodes[i].delay;
        if (!isfinite(arrival[i])) {
            ok = 0;
            break;
        }
        longest = fmax(longest, arrival[i]);
    }
    if (ok) {
        for (size_t i = 0; i < count; i++)
            nodes[i].arrival = arrival[i];
        *critical = longest;
    }
    free(arrival);
    return ok;
}
int mulib_asic_sta_slack(const mulib_asic_timing_node_t *nodes, size_t count, double deadline,
                         double *slack) {
    if ((!nodes && count) || !slack || !isfinite(deadline) || count > UINT16_MAX)
        return 0;
    for (size_t i = 0; i < count; i++)
        if (!isfinite(nodes[i].arrival) || !isfinite(deadline - nodes[i].arrival))
            return 0;
    for (size_t i = 0; i < count; i++)
        slack[i] = deadline - nodes[i].arrival;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_ASIC_STA */
