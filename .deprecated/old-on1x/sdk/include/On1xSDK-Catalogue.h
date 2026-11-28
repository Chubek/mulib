#ifndef ON1X_SDK_CATALOGUE_H
#define ON1X_SDK_CATALOGUE_H

#include "On1xSDK-Capability.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_CATALOGUE_MAX_ENTRIES 64u
#define ON1X_SDK_CATALOGUE_MAX_ITEMS_PER_ENTRY 64u

typedef struct On1xSDK_CatalogueCapability {
    On1xSDK_StringView name;
    uint32_t revision;
    uint32_t reserved;
} On1xSDK_CatalogueCapability;

typedef struct On1xSDK_CatalogueRequirement {
    On1xSDK_StringView name;
    uint32_t min_revision;
    uint32_t max_revision;
} On1xSDK_CatalogueRequirement;

typedef struct On1xSDK_CatalogueEntry {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_StringView id;
    On1xSDK_StringView category;
    /* Declarative label only. It is never resolved to executable code. */
    On1xSDK_StringView implementation_label;
    uint32_t revision;
    uint32_t cost;
    const On1xSDK_CatalogueCapability *provides;
    size_t provide_count;
    const On1xSDK_CatalogueRequirement *requirements;
    size_t requirement_count;
    const On1xSDK_StringView *conflicts;
    size_t conflict_count;
} On1xSDK_CatalogueEntry;

typedef struct On1xSDK_CatalogueProblem {
    uint32_t abi_version;
    uint32_t struct_size;
    const On1xSDK_CatalogueRequirement *goals;
    size_t goal_count;
    const On1xSDK_CatalogueCapability *available;
    size_t available_count;
    const On1xSDK_StringView *allowed_entry_ids;
    size_t allowed_entry_count; /* zero means all */
    uint64_t max_cost;
    size_t max_search_nodes;
} On1xSDK_CatalogueProblem;

typedef struct On1xSDK_CataloguePlanInfo {
    uint64_t catalogue_revision;
    uint64_t cost;
    size_t entry_count;
    size_t expanded_nodes;
    uint32_t found;
    uint32_t optimal;
} On1xSDK_CataloguePlanInfo;

On1xSDK_Status on1x_sdk_catalogue_create(On1xSDK_Catalogue **out);
void on1x_sdk_catalogue_destroy(On1xSDK_Catalogue *catalogue);
On1xSDK_Status on1x_sdk_catalogue_add(
    On1xSDK_Catalogue *catalogue, const On1xSDK_CatalogueEntry *entry
);
uint64_t on1x_sdk_catalogue_revision(const On1xSDK_Catalogue *catalogue);
size_t on1x_sdk_catalogue_count(const On1xSDK_Catalogue *catalogue);
On1xSDK_Status on1x_sdk_catalogue_entry(
    const On1xSDK_Catalogue *catalogue,
    size_t index,
    On1xSDK_CatalogueEntryView **out_view
);
/* The view borrows the catalogue; mutation makes its accessors return STALE. */
void on1x_sdk_catalogue_entry_view_destroy(On1xSDK_CatalogueEntryView *view);
On1xSDK_Status on1x_sdk_catalogue_entry_identity(
    const On1xSDK_CatalogueEntryView *view,
    On1xSDK_StringView *id,
    On1xSDK_StringView *category,
    uint32_t *revision,
    uint32_t *cost
);
On1xSDK_Status on1x_sdk_catalogue_entry_provide_count(
    const On1xSDK_CatalogueEntryView *view, size_t *count
);
On1xSDK_Status on1x_sdk_catalogue_entry_provide_at(
    const On1xSDK_CatalogueEntryView *view,
    size_t index,
    On1xSDK_CatalogueCapability *out
);
On1xSDK_Status on1x_sdk_catalogue_entry_requirement_count(
    const On1xSDK_CatalogueEntryView *view, size_t *count
);
On1xSDK_Status on1x_sdk_catalogue_entry_requirement_at(
    const On1xSDK_CatalogueEntryView *view,
    size_t index,
    On1xSDK_CatalogueRequirement *out
);

/*
 * LIMIT may return a feasible incumbent with info.found=1 and info.optimal=0.
 * It is never an optimality certificate.
 */
On1xSDK_Status on1x_sdk_catalogue_resolve(
    const On1xSDK_Catalogue *catalogue,
    const On1xSDK_CatalogueProblem *problem,
    On1xSDK_CataloguePlan **out
);
void on1x_sdk_catalogue_plan_destroy(On1xSDK_CataloguePlan *plan);
On1xSDK_Status on1x_sdk_catalogue_plan_info(
    const On1xSDK_CataloguePlan *plan, On1xSDK_CataloguePlanInfo *out
);
On1xSDK_Status on1x_sdk_catalogue_plan_entry(
    const On1xSDK_CataloguePlan *plan,
    size_t order_index,
    size_t *catalogue_index,
    On1xSDK_StringView *entry_id
);

#ifdef __cplusplus
}
#endif

#endif
