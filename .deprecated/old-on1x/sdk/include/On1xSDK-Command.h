/* Namespaced extension command validation; built-ins cannot be shadowed. */
#ifndef ON1X_SDK_COMMAND_H
#define ON1X_SDK_COMMAND_H

#include "On1xSDK-Project.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Built-in names are build, release, init, exec, format, fetch, lock, info,
 * inspect and validate.  Extension commands must instead be
 * provider:command, with both components matching
 * [a-z][a-z0-9]*(?:-[a-z0-9]+)* and at most 64 bytes each.
 */
ON1X_SDK_PROJECT_API On1xSDK_Status
on1x_sdk_command_validate_extension_name(
    On1xSDK_ProjectString name);
ON1X_SDK_PROJECT_API int on1x_sdk_command_is_builtin(
    On1xSDK_ProjectString name);

#ifdef __cplusplus
}
#endif

#endif
