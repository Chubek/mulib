#include "on1x_async_frame.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/* Observe the runtime's actual calls to generated resume/drop callbacks. Do
 * not execute a step inline or change any action, deadline or drop decision. */
static struct {
    void *frame;
    on1x_async_resume resume;
    on1x_async_drop drop;
} records[128];
static unsigned count;

static unsigned find(void *frame) {
    for (unsigned i = 0; i < count; ++i)
        if (records[i].frame == frame) return i;
    abort();
}
static on1x_async_step resume(void *frame) {
    const unsigned i = find(frame);
    const on1x_async_step step = records[i].resume(frame);
    if (step.kind == ON1X_ASYNC_SLEEP_MS)
        printf("frame %u sleep %" PRId64 "\n", i, step.value.milliseconds);
    else if (step.kind == ON1X_ASYNC_RETURN_I64)
        printf("frame %u return %" PRId64 "\n", i, step.value.i64);
    else abort();
    return step;
}
static void drop(void *frame) {
    const unsigned i = find(frame);
    records[i].frame = NULL;
    printf("frame %u drop\n", i);
    records[i].drop(frame);
}
on1x_csp_task *__real_on1x_async_launch_v1(on1x_async_resume, on1x_async_drop, void *,
                                         const char *, size_t, size_t);
on1x_csp_task *__wrap_on1x_async_launch_v1(on1x_async_resume next, on1x_async_drop release, void *frame,
                                         const char *file, size_t line, size_t column) {
    if (!frame || count >= sizeof(records) / sizeof(records[0])) abort();
    records[count].frame = frame;
    records[count].resume = next;
    records[count].drop = release;
    ++count;
    return __real_on1x_async_launch_v1(resume, drop, frame, file, line, column);
}
