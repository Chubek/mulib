/* Authorized adapter to the canonical compiler's diagnostic-catalog validator. */
#ifndef ON1X_SDK_DIAGNOSTIC_CATALOG_H
#define ON1X_SDK_DIAGNOSTIC_CATALOG_H

#include "On1xSDK-Process.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Requests:
 *   COMPILER --check --diagnostics json --color never
 *            --diagnostic-catalog CATALOG SOURCE
 * This SDK layer does not create a second catalog parser and does not execute
 * anything named by catalog data.  The explicitly selected compiler process is
 * still subject to host authorization.
 */
ON1X_SDK_PROJECT_API On1xSDK_Status
on1x_sdk_diagnostic_catalog_dispatch_check(
    const On1xSDK_ProcessHost *host,
    On1xSDK_ProjectString compiler,
    On1xSDK_ProjectString catalog,
    On1xSDK_ProjectString source,
    On1xSDK_ProjectString working_directory,
    const On1xSDK_ProcessCancellation *cancellation,
    On1xSDK_ProcessResult *result);

#ifdef __cplusplus
}
#endif

#endif
