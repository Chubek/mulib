#ifndef ON1X_SDK_SESSION_H
#define ON1X_SDK_SESSION_H

#include "On1xSDK-Compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sessions and their mutable children are thread-confined to the creating
 * thread. request_cancel is the sole cross-thread mutable operation.
 * The diagnostic sink is borrowed until the session is released or replaced.
 * Immutable source, token, AST, and compilation snapshots retain internal
 * storage and remain readable on the creating thread after the last session
 * reference is released; mutating operations then reject.
 */
On1xSDK_Status on1x_sdk_session_create(
    const On1xSDK_DiagnosticSink *diagnostics,
    On1xSDK_Session **out_session);
On1xSDK_Status on1x_sdk_session_retain(On1xSDK_Session *session);
void on1x_sdk_session_release(On1xSDK_Session *session);

On1xSDK_Status on1x_sdk_session_set_diagnostics(
    On1xSDK_Session *session,
    const On1xSDK_DiagnosticSink *diagnostics);

/* Thread-safe, cooperative cancellation. Partial objects are never published. */
On1xSDK_Status on1x_sdk_session_request_cancel(On1xSDK_Session *session);
On1xSDK_Status on1x_sdk_session_reset_cancel(On1xSDK_Session *session);
On1xSDK_Status on1x_sdk_session_is_cancelled(
    const On1xSDK_Session *session,
    uint32_t *out_cancelled);

uint64_t on1x_sdk_session_revision(const On1xSDK_Session *session);
On1xSDK_CompilerCondition on1x_sdk_session_last_condition(
    const On1xSDK_Session *session);

/*
 * The returned record and its strings are borrowed until the next operation on
 * this session. ON1X_SDK_ERROR with condition
 * ON1X_SDK_COMPILER_NOT_FOUND means no diagnostic has been recorded.
 */
On1xSDK_Status on1x_sdk_session_last_diagnostic(
    const On1xSDK_Session *session,
    On1xSDK_Diagnostic *out_diagnostic);
On1xSDK_Status on1x_sdk_session_clear_diagnostic(On1xSDK_Session *session);

#ifdef __cplusplus
}
#endif

#endif
