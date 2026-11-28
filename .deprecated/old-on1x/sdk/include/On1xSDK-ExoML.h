/* Public ExoML projection entry points; extraction remains a build-time tool.
 */
#ifndef ON1X_SDK_EXOML_H
#define ON1X_SDK_EXOML_H

#include "On1xSDK-CDecl.h"
#include "On1xSDK-ContractCheck.h"

#ifdef __cplusplus
extern "C" {
#endif

On1xSDK_Status
on1x_sdk_exoml_projection_check(const On1xSDK_CDeclFunction *function,
                                const On1xSDK_ForeignContractClaim *claim);

#ifdef __cplusplus
}
#endif

#endif
