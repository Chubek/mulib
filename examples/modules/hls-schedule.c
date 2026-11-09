/* Checked usage example for mulib-hls-schedule.h. */
#define MULIB_IMPL_HLS_SCHEDULE
#include "mulib-hls-schedule.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_hls_operation_t op[3] = {0};
    uint32_t makespan;
    op[0].latency = 3;
    op[1].latency = 5;
    op[2].latency = 2;
    op[2].deps[0] = 0;
    op[2].deps[1] = 1;
    op[2].dep_count = 2;
    assert(mulib_hls_schedule_asap(op, 3, &makespan) && makespan == 7 && op[2].start == 5);
    puts("PASS mulib-hls-schedule");
    return 0;
}
