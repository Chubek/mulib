#ifndef ON1X_SDK_H
#define ON1X_SDK_H

#include "On1xSDK-Types.h"
#include "On1xSDK-Diagnostic.h"
#include "On1xSDK-Registry.h"
#include "On1xSDK-Analyzer.h"
#include "On1xSDK-Transformer.h"
#include "On1xSDK-Backend.h"
#include "On1xSDK-Contract.h"
#include "On1xSDK-Module.h"
#include "On1xSDK-CLIOpt.h"
#include "On1xSDK-Toolchain.h"
#include "On1xSDK-ExecEnv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Entry point for linked or isolated shared-library providers. Define it in the
 * extension, not the SDK, and return a descriptor with static lifetime. */
const On1xSDK_Extension *on1x_sdk_linked_extension(void);

#ifdef __cplusplus
}
#endif

#endif
