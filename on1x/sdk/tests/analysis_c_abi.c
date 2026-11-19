#include "On1xSDK-AnalysisManager.h"
#include "On1xSDK-CFG.h"
#include "On1xSDK-Dominator.h"
#include "On1xSDK-EffectAnalysis.h"
#include "On1xSDK-Liveness.h"
#include "On1xSDK-Loop.h"
#include "On1xSDK-PassManager.h"
#include "On1xSDK-TranslationValidation.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) return __LINE__; } while (0)

_Static_assert(ON1X_SDK_OK == 0, "v1 OK changed");
_Static_assert(ON1X_SDK_ERROR == 1, "v1 ERROR changed");
_Static_assert(ON1X_SDK_UNSUPPORTED == 2, "v1 UNSUPPORTED changed");
_Static_assert(ON1X_SDK_LIMIT == 3, "v1 LIMIT changed");
_Static_assert(ON1X_SDK_INVALID_ARGUMENT == 4, "v1 INVALID_ARGUMENT changed");
_Static_assert(ON1X_SDK_ABI_MISMATCH == 5, "v1 ABI_MISMATCH changed");
_Static_assert(ON1X_SDK_DUPLICATE == 6, "v1 DUPLICATE changed");
_Static_assert(ON1X_SDK_BUSY == 7, "v1 BUSY changed");

int main(void) {
    const size_t alignment = on1x_sdk_analysis_manager_storage_alignment();
    const size_t requested = on1x_sdk_analysis_manager_storage_size();
    const size_t allocated = (requested + alignment - 1u) / alignment * alignment;
    void *storage = aligned_alloc(alignment, allocated);
    On1xSDK_AnalysisManager *manager = NULL;
    On1xSDK_AnalysisManagerConfig config = {
        ON1X_SDK_ANALYSIS_ABI_VERSION, sizeof(config), UINT64_C(7), 2, 4
    };
    CHECK(storage != NULL);
    CHECK(on1x_sdk_analysis_manager_initialize(
              storage, allocated, &config, &manager) == ON1X_SDK_OK);
    CHECK(manager != NULL);

    On1xSDK_AnalysisManagerStats stats = {
        ON1X_SDK_ANALYSIS_ABI_VERSION, sizeof(stats), 0, 0, 0, 0, 0
    };
    CHECK(on1x_sdk_analysis_manager_stats(manager, &stats) == ON1X_SDK_OK);
    CHECK(stats.generation == 1 && stats.observed_sources == 0);

    On1xSDK_Pipeline pipeline;
    memset(&pipeline, 0, sizeof(pipeline));
    pipeline.abi_version = ON1X_SDK_ANALYSIS_ABI_VERSION;
    pipeline.struct_size = sizeof(pipeline);
    CHECK(on1x_sdk_pipeline_parse(
              "constant-fold, dead-pure", 24, 8, &pipeline) == ON1X_SDK_OK);
    CHECK(pipeline.pass_count == 2 && pipeline.round_limit == 8);

    On1xSDK_PassDescriptor descriptor;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.abi_version = ON1X_SDK_ANALYSIS_ABI_VERSION;
    descriptor.struct_size = sizeof(descriptor);
    CHECK(on1x_sdk_pass_descriptor(
              ON1X_SDK_PASS_DEAD_PURE, &descriptor) == ON1X_SDK_OK);
    CHECK(descriptor.pass_version != 0 && descriptor.name.size != 0);

    CHECK(on1x_sdk_analysis_manager_dispose(manager) == ON1X_SDK_OK);
    CHECK(on1x_sdk_analysis_manager_dispose(manager) ==
          ON1X_SDK_ANALYSIS_INVALID_STATE);
    manager = NULL;
    CHECK(on1x_sdk_analysis_manager_initialize(
              storage, allocated, &config, &manager) == ON1X_SDK_OK);
    CHECK(on1x_sdk_analysis_manager_dispose(manager) == ON1X_SDK_OK);
    free(storage);
    return 0;
}
