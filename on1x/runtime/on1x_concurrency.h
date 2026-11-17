#ifndef ON1X_CONCURRENCY_H
#define ON1X_CONCURRENCY_H
#include "on1x_runtime.h"
/* Private generated-code ABI. These are NOT copyable foreign handles, an
 * embedding API, native-thread tasks, or an async/await implementation. */
#if defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
#define ON1X_CSP_ABI_VERSION 1
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct on1x_csp_task on1x_csp_task;
typedef struct on1x_csp_group on1x_csp_group;
typedef struct on1x_csp_channel on1x_csp_channel;
typedef struct on1x_csp_process on1x_csp_process;
typedef int64_t (*on1x_csp_callback)(on1x_env *);
typedef struct {on1x_csp_channel *channel;int operation;int64_t payload;} on1x_csp_offer;
/* One MuLIB atomic select: operation 1 sends, 2 receives. No deadline or
 * cancellation in this profile; selected==count denotes default. */
void on1x_csp_select(on1x_csp_offer *,size_t,bool,int64_t *,size_t *,bool *,const char *,size_t,size_t);
int64_t on1x_csp_run(on1x_csp_callback,on1x_env *,const char *,size_t,size_t);
on1x_csp_group *on1x_csp_group_enter(const char *,size_t,size_t);
void on1x_csp_group_finish(on1x_csp_group *);
on1x_csp_task *on1x_csp_launch(on1x_csp_callback,on1x_env *,const char *,size_t,size_t);
/* Only an immediately preceding launch on this single native thread can
 * guarantee the child has not started; consumes and joins without invoking it. */
bool on1x_csp_cancel_ready(on1x_csp_task *,const char *,size_t,size_t);
/* Direct absolute-path exec; at most nine constant argv entries (including
 * argv[0]). No host shell, interpolation, stream capture or detached process. */
on1x_csp_process *on1x_csp_launch_exec(const char *const *,size_t,const char *,size_t,size_t);
/* Consume a scoped xfiber. Exit 0..255, signal termination -signum.
 * cancel returns true only if SIGTERM was requested; it always reaps. */
int64_t on1x_csp_process_join(on1x_csp_process *,const char *,size_t,size_t);
bool on1x_csp_process_cancel(on1x_csp_process *,const char *,size_t,size_t);
int64_t on1x_csp_join(on1x_csp_task *,const char *,size_t,size_t);
on1x_csp_channel *on1x_csp_channel_create(int64_t,const char *,size_t,size_t);
/* Send: 0 success, 1 closed. Receive: true value, false drained close.
 * Runtime/OOM/deadlock/work exhaustion are located terminal failures (exit 73).
 * No timeout, guessed clock, cancellation or exceptional unwinding. */
int64_t on1x_csp_send(on1x_csp_channel *,int64_t,const char *,size_t,size_t);
bool on1x_csp_receive(on1x_csp_channel *,int64_t *,const char *,size_t,size_t);
bool on1x_csp_close(on1x_csp_channel *,const char *,size_t,size_t);
void on1x_csp_yield(const char *,size_t,size_t);
void on1x_csp_call_enter(const char *,size_t,size_t);
void on1x_csp_call_leave(void);
#ifdef __cplusplus
}
#endif
#endif
