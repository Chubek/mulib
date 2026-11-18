#ifndef ON1X_CONCURRENCY_H
#define ON1X_CONCURRENCY_H
#include "on1x_runtime.h"
/* Private generated-code ABI. These are NOT copyable foreign handles, an
 * embedding API, native-thread tasks, or an async/await implementation. */
#if defined(__linux__) && defined(__GLIBC__) && defined(__x86_64__)
#define ON1X_CSP_ABI_VERSION 4
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
/* One MuLIB atomic select: operation 1 sends, 2 receives. i64 channels and
 * captured u8 process streams may be mixed; byte payloads widen exactly in this
 * transport and are staged as uint8_t objects inside the adapter. No deadline
 * in this entrypoint; selected==count denotes default. Generated code must
 * observe on1x_csp_cancelled() and unwind before using cancelled results. */
void on1x_csp_select(on1x_csp_offer *,size_t,bool,int64_t *,size_t *,bool *,const char *,size_t,size_t);
/* Monotonic milliseconds, >=0. selected==count is the deadline;
 * selected==count+1 is default. Cancellation is observed by generated edges. */
void on1x_csp_select_after(on1x_csp_offer *,size_t,bool,int64_t,int64_t *,size_t *,bool *,const char *,size_t,size_t);
bool on1x_csp_cancelled(void);
void on1x_csp_shield_enter(void);
void on1x_csp_shield_leave(void);
bool on1x_csp_cancel(on1x_csp_task *,const char *,size_t,size_t);
int64_t on1x_csp_run_v4(on1x_csp_callback,on1x_env *,const char *,size_t,size_t);
on1x_csp_group *on1x_csp_group_enter(const char *,size_t,size_t);
/* A nonnegative relative monotonic duration. Check cancellation immediately
 * after entry (including duration zero) and do not execute a cancelled body. */
on1x_csp_group *on1x_csp_group_enter_within(int64_t,const char *,size_t,size_t);
void on1x_csp_group_finish(on1x_csp_group *);
on1x_csp_task *on1x_csp_launch(on1x_csp_callback,on1x_env *,const char *,size_t,size_t);
/* Legacy name retained for callers of the earlier pre-dispatch entrypoint.
 * Now delegates to cooperative consuming cancellation. */
bool on1x_csp_cancel_ready(on1x_csp_task *,const char *,size_t,size_t);
/* Direct absolute-path exec; at most nine constant argv entries (including
 * argv[0]), 4096 bytes each and 16384 bytes total including terminators. No host shell,
 * interpolation, stream capture or detached process. */
on1x_csp_process *on1x_csp_launch_exec(const char *const *,size_t,const char *,size_t,size_t);
/* Captured command result: process, byte stdin sender, byte stdout receiver,
 * byte stderr receiver. Streams are bounded MuLIB channels with pipe backpressure;
 * argv has already been parsed by the compiler and is never host-shell text. */
void on1x_csp_launch_capture(const char *const *,size_t,on1x_csp_process **,
                            on1x_csp_channel **,on1x_csp_channel **,on1x_csp_channel **,
                            const char *,size_t,size_t);
/* Consume a scoped xfiber. Exit 0..255, signal termination -signum.
 * cancel returns true only if SIGTERM was requested; it always reaps. */
int64_t on1x_csp_process_join(on1x_csp_process *,const char *,size_t,size_t);
bool on1x_csp_process_cancel(on1x_csp_process *,const char *,size_t,size_t);
int64_t on1x_csp_join(on1x_csp_task *,const char *,size_t,size_t);
on1x_csp_channel *on1x_csp_channel_create(int64_t,const char *,size_t,size_t);
/* Send: 0 success, 1 closed. Receive: true value, false drained close.
 * Runtime/OOM/deadlock/work exhaustion are located terminal failures (exit 73).
 * No per-operation timeout or exceptional unwinding. Generated cancellation
 * edges must run before interpreting a result when on1x_csp_cancelled() is true. */
int64_t on1x_csp_send(on1x_csp_channel *,int64_t,const char *,size_t,size_t);
bool on1x_csp_receive(on1x_csp_channel *,int64_t *,const char *,size_t,size_t);
bool on1x_csp_close(on1x_csp_channel *,const char *,size_t,size_t);
int64_t on1x_csp_stream_send(on1x_csp_channel *,uint8_t,const char *,size_t,size_t);
bool on1x_csp_stream_receive(on1x_csp_channel *,uint8_t *,const char *,size_t,size_t);
bool on1x_csp_stream_close(on1x_csp_channel *,const char *,size_t,size_t);
void on1x_csp_yield(const char *,size_t,size_t);
void on1x_csp_call_enter(const char *,size_t,size_t);
void on1x_csp_call_leave(void);
#ifdef __cplusplus
}
#endif
#endif
