#ifndef ON1X_ASYNC_FRAME_H
#define ON1X_ASYNC_FRAME_H
#include "on1x_concurrency.h"

/* Private C11 runtime ABI for compiler-owned frames, not a foreign, SDK,
 * or source-language interface. One native thread and the CSP supervisor. */
#ifdef ON1X_CSP_ABI_VERSION
#define ON1X_ASYNC_FRAME_ABI_VERSION 1
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
 ON1X_ASYNC_YIELD=1,
 ON1X_ASYNC_SLEEP_MS,
 ON1X_ASYNC_RETURN_I64
} on1x_async_step_kind;
typedef struct {
 on1x_async_step_kind kind;
 union { int64_t milliseconds, i64; } value;
} on1x_async_step;
typedef on1x_async_step (*on1x_async_resume)(void *frame);
typedef void (*on1x_async_drop)(void *frame);

typedef enum {
 ON1X_ASYNC_RETURNED=1,
 ON1X_ASYNC_CANCELLED,
 ON1X_ASYNC_JOIN_INTERRUPTED
} on1x_async_join_status;
typedef struct {
 on1x_async_join_status status;
 int64_t i64; /* Valid only when status == ON1X_ASYNC_RETURNED. */
} on1x_async_result;

/* Eager schedule, never inline resume. Transfers the initialized frame to the
 * current taskgroup. Each resume returns with no suspended native call stack.
 * The frame alone holds its continuation state and suspend-live fields.
 * Resume and drop must be finite, nonblocking and may not call fiber APIs.
 * Drop releases live fields and frame storage exactly once on return or
 * cooperative cancellation, including cancellation before the first resume.
 * Fatal runtime failures do not unwind. */
on1x_csp_task *on1x_async_launch_v1(on1x_async_resume,on1x_async_drop,void *,
                                   const char *,size_t,size_t);
void *on1x_async_frame_alloc_v1(size_t,const char *,size_t,size_t);
void on1x_async_frame_free_v1(void *);
/* Same-parent and consuming (also consumable with on1x_csp_cancel).
 * JOIN_INTERRUPTED withdraws the wait without consuming the child; the
 * cancelling parent's existing taskgroup exit must still drain it. */
on1x_async_result on1x_async_join_v1(on1x_csp_task *,const char *,size_t,size_t);
/* Compiler-only join for compatible affine ifiber<i64> handles. An
 * interrupted join must take the existing generated cancellation edge
 * before using the returned value. */
int64_t on1x_async_join_internal_v1(on1x_csp_task *,const char *,size_t,size_t);
#ifdef __cplusplus
}
#endif
#endif
#endif
