#include "On1xSDK-AOT.h"
#include "On1xSDK-Artifact.h"
#include "On1xSDK-Backend.h"
#include "On1xSDK-CallingConvention.h"
#include "On1xSDK-DataLayout.h"
#include "On1xSDK-JIT.h"
#include "On1xSDK-MachineIR.h"
#include "On1xSDK-ObjectABI.h"
#include "On1xSDK-Target.h"
#include "On1xSDK-TargetMachine.h"
#include "On1xSDK-Toolchain.h"
#include "On1xSDK-ToolchainResolver.h"

#include <stddef.h>
#include <stdint.h>

_Static_assert(ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION == 2u,
               "backend ABI must be version 2");
_Static_assert(ON1X_SDK_TARGET_ABI_VERSION == 2u,
               "target ABI must be version 2");
_Static_assert(offsetof(On1xSDK_BackendProvider, abi_version) == 0,
               "ABI prefix must start at byte zero");
_Static_assert(offsetof(On1xSDK_BackendProvider, struct_size) == sizeof(uint32_t),
               "ABI size must be the second word");
_Static_assert(sizeof(((On1xSDK_ArtifactInfo *)0)->sha256) ==
                   ON1X_SDK_ARTIFACT_DIGEST_BYTES,
               "artifact digest width");

int main(void) {
    On1xSDK_Target *target = NULL;
    On1xSDK_ToolchainResolver *resolver = NULL;
    On1xSDK_Artifact *artifact = NULL;
    on1x_sdk_target_destroy(target);
    on1x_sdk_toolchain_resolver_destroy(resolver);
    on1x_sdk_artifact_destroy(artifact);
    return 0;
}
