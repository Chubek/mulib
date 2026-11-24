#include "TimerABI.h"
#include "on1x_async_frame.h"

#if !defined(ON1X_ASYNC_FRAME_ABI_VERSION) || ON1X_ASYNC_FRAME_ABI_VERSION != 1
#error "Archon timers require the Linux/glibc AMD64 On1x frame ABI v1"
#endif

typedef int64_t (*archon_resume)(int64_t *words, int64_t *kind);
struct archon_frame {
    archon_resume resume;
    int64_t words[];
};

_Static_assert(sizeof(int64_t) == 8 && _Alignof(int64_t) == 8, "Archon i64 word layout");
_Static_assert(sizeof(size_t) == 8 && sizeof(void *) == 8, "Archon LP64 layout");

/* Only translate the callback ABI, not scheduling, state or source
 * operations. The actual step (including dispatch) is compiled by Archon. */
static on1x_async_step resume_frame(void *raw) {
    struct archon_frame *frame = raw;
    int64_t kind = 0;
    const int64_t value = frame->resume(frame->words, &kind);
    if (kind == ON1X_ARCHON_TIMER_SLEEP)
        return (on1x_async_step){ON1X_ASYNC_SLEEP_MS, {.milliseconds = value}};
    if (kind == ON1X_ARCHON_TIMER_RETURN)
        return (on1x_async_step){ON1X_ASYNC_RETURN_I64, {.i64 = value}};
    return (on1x_async_step){0}; /* supervisor reports a located protocol fault */
}

on1x_csp_task *on1x_archon_timer_launch_v1(archon_resume resume, size_t words,
                                        const char *file, size_t line, size_t column) {
    if (!resume || !words || words > ON1X_ARCHON_TIMER_MAX_WORDS)
        return on1x_async_launch_v1(NULL, NULL, NULL, file, line, column);
    struct archon_frame *frame = on1x_async_frame_alloc_v1(
        sizeof(*frame) + words * sizeof(int64_t), file, line, column);
    frame->resume = resume;
    frame->words[0] = 0;
    return on1x_async_launch_v1(resume_frame, on1x_async_frame_free_v1, frame,
                                file, line, column);
}

/* C bool does not promise all high return-register bits are zero. */
int64_t on1x_archon_timer_cancelled_v1(void) {
    return on1x_csp_cancelled() ? 1 : 0;
}
int64_t on1x_archon_timer_cancel_v1(on1x_csp_task *task, const char *file,
                                    size_t line, size_t column) {
    return on1x_csp_cancel(task, file, line, column) ? 1 : 0;
}
