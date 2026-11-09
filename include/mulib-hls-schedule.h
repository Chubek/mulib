/** @file mulib-hls-schedule.h
 * @brief Resource-unconstrained ASAP scheduling of a topologically sorted operation DAG with
 * integral cycle latencies and up to eight dependencies.
 * @details C11 single-header implementation; define MULIB_IMPL_HLS_SCHEDULE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_HLS_SCHEDULE_H
#define MULIB_HLS_SCHEDULE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_hls_operation {
    uint32_t latency;
    uint16_t deps[8];
    uint8_t dep_count;
    uint32_t start;
} mulib_hls_operation_t;
int mulib_hls_schedule_asap(mulib_hls_operation_t *ops, size_t count, uint32_t *makespan);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HLS_SCHEDULE_H */
#if defined(MULIB_IMPL_HLS_SCHEDULE) && !defined(MULIB_HLS_SCHEDULE_H_IMPLEMENTED)
#define MULIB_HLS_SCHEDULE_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hls_schedule_asap(mulib_hls_operation_t *ops, size_t count, uint32_t *makespan) {
    if ((!ops && count) || !makespan || count > UINT16_MAX)
        return 0;
    uint32_t *start = count ? malloc(count * sizeof *start) : NULL;
    if (count && !start)
        return 0;
    uint64_t finish = 0;
    int ok = 1;
    for (size_t i = 0; i < count && ok; i++) {
        if (!ops[i].latency || ops[i].dep_count > 8) {
            ok = 0;
            break;
        }
        uint64_t at = 0;
        for (size_t j = 0; j < ops[i].dep_count; j++) {
            size_t d = ops[i].deps[j];
            if (d >= i) {
                ok = 0;
                break;
            }
            uint64_t end = (uint64_t)start[d] + ops[d].latency;
            if (end > at)
                at = end;
        }
        if (!ok || at + ops[i].latency > UINT32_MAX) {
            ok = 0;
            break;
        }
        start[i] = (uint32_t)at;
        if (at + ops[i].latency > finish)
            finish = at + ops[i].latency;
    }
    if (ok) {
        for (size_t i = 0; i < count; i++)
            ops[i].start = start[i];
        *makespan = (uint32_t)finish;
    }
    free(start);
    return ok;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_HLS_SCHEDULE */
