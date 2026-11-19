/* Manifest formatting and authorized adapters to the canonical source formatter. */
#ifndef ON1X_SDK_FORMATTER_H
#define ON1X_SDK_FORMATTER_H

#include "On1xSDK-Manifest.h"
#include "On1xSDK-Process.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Validates and canonically formats a plain root On1xfile.  It executes no
 * template or policy.  The result has one LF final newline and is idempotent.
 */
ON1X_SDK_PROJECT_API On1xSDK_Status on1x_sdk_manifest_format(
    On1xSDK_ProjectBytes input,
    const On1xSDK_ProjectAllocator *allocator,
    const On1xSDK_ProjectDiagnosticSink *sink,
    On1xSDK_ProjectBuffer **out_buffer);

/*
 * Requests the existing compiler formatter as:
 *   COMPILER fmt --target plain INPUT
 * Both paths must be explicit absolute paths.  The host remains the sole
 * process authority and receives the exact argv array without a shell.
 */
ON1X_SDK_PROJECT_API On1xSDK_Status on1x_sdk_formatter_dispatch_source(
    const On1xSDK_ProcessHost *host,
    On1xSDK_ProjectString compiler,
    On1xSDK_ProjectString input,
    On1xSDK_ProjectString working_directory,
    const On1xSDK_ProcessCancellation *cancellation,
    On1xSDK_ProcessResult *result);

#ifdef __cplusplus
}
#endif

#endif
