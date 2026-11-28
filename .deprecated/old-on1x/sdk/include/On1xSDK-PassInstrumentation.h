#ifndef ON1X_SDK_PASS_INSTRUMENTATION_H
#define ON1X_SDK_PASS_INSTRUMENTATION_H

#include "On1xSDK-Pass.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_PassStage;
enum {
    ON1X_SDK_PASS_STAGE_BEFORE = 1,
    ON1X_SDK_PASS_STAGE_AFTER = 2
};
typedef uint32_t On1xSDK_PassOutcome;
enum {
    ON1X_SDK_PASS_UNCHANGED = 1,
    ON1X_SDK_PASS_COMMITTED = 2,
    ON1X_SDK_PASS_LIMIT = 3,
    ON1X_SDK_PASS_CANCELLED = 4,
    ON1X_SDK_PASS_DEADLINE = 5,
    ON1X_SDK_PASS_CALLBACK_FAILED = 6
};

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_PassKind pass;
    On1xSDK_PassStage stage;
    On1xSDK_PassOutcome outcome;
    uint32_t reserved;
    uint64_t round;
    uint64_t input_revision;
    uint64_t output_revision;
    uint64_t deterministic_work;
    uint64_t attempted_rewrites;
} On1xSDK_PassEvent;

/*
 * Same status contract as On1xSDK_AnalysisControlFn. event is callback-scoped
 * and may not be retained; mutating manager re-entry returns ON1X_SDK_BUSY.
 */
typedef uint32_t (*On1xSDK_PassInstrumentationFn)(
    void *userdata, const On1xSDK_PassEvent *event);

#ifdef __cplusplus
}
#endif

#endif
