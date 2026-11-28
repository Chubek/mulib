#ifndef ON1X_SDK_BUDGET_H
#define ON1X_SDK_BUDGET_H

#include "On1xSDK-ABI.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_BudgetKind;
enum {
    ON1X_SDK_BUDGET_WORK = 0,
    ON1X_SDK_BUDGET_BYTES = 1,
    ON1X_SDK_BUDGET_ALLOCATIONS = 2,
    ON1X_SDK_BUDGET_DIAGNOSTICS = 3,
    ON1X_SDK_BUDGET_DEPTH = 4,
    ON1X_SDK_BUDGET_IPC = 5,
    ON1X_SDK_BUDGET_ARTIFACTS = 6,
    ON1X_SDK_BUDGET_KIND_COUNT = 7
};

typedef struct On1xSDK_BudgetConfig {
    uint32_t abi_version;
    uint32_t struct_size;
    /* UINT64_MAX is unbounded. Zero permits no consumption. */
    uint64_t limits[ON1X_SDK_BUDGET_KIND_COUNT];
} On1xSDK_BudgetConfig;

typedef struct On1xSDK_BudgetSnapshot {
    uint64_t limits[ON1X_SDK_BUDGET_KIND_COUNT];
    uint64_t used[ON1X_SDK_BUDGET_KIND_COUNT];
} On1xSDK_BudgetSnapshot;

On1xSDK_Status on1x_sdk_budget_create(
    const On1xSDK_BudgetConfig *config, On1xSDK_Budget **out
);
void on1x_sdk_budget_retain(On1xSDK_Budget *budget);
void on1x_sdk_budget_release(On1xSDK_Budget *budget);
On1xSDK_Status on1x_sdk_budget_consume(
    On1xSDK_Budget *budget, On1xSDK_BudgetKind kind, uint64_t amount
);
On1xSDK_Status on1x_sdk_budget_snapshot(
    const On1xSDK_Budget *budget, On1xSDK_BudgetSnapshot *out
);

#ifdef __cplusplus
}
#endif

#endif
