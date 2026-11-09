/* Checked usage example for mulib-dsim-scheduler.h. */
#define MULIB_IMPL_DSIM_LOGIC4
#define MULIB_IMPL_DSIM_SCHEDULER
#include "mulib-dsim-scheduler.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    mulib_dsim_scheduler_t s = {0};
    mulib_dsim_event_t e;
    assert(mulib_dsim_scheduler_push(&s, 7, 0, 0, 1, MULIB_DSIM_1));
    assert(mulib_dsim_scheduler_push(&s, 2, 1, 0, 2, MULIB_DSIM_0));
    assert(mulib_dsim_scheduler_push(&s, 2, 0, 0, 3, MULIB_DSIM_X));
    assert(mulib_dsim_scheduler_pop(&s, &e) && e.signal == 3);
    assert(mulib_dsim_scheduler_pop(&s, &e) && e.signal == 2);
    assert(mulib_dsim_scheduler_pop(&s, &e) && e.signal == 1);
    assert(!mulib_dsim_scheduler_pop(&s, &e));
    mulib_dsim_scheduler_free(&s);
    puts("PASS mulib-dsim-scheduler");
    return 0;
}
