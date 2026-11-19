#include "On1xSDK-Catalogue.h"

#include "mulib-cdd-resolve.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t maximum_search_nodes = 1u << 20;

struct Capability {
    std::string name;
    std::uint32_t revision = 0;
};

struct Requirement {
    std::string name;
    std::uint32_t minimum = 0;
    std::uint32_t maximum = 0;
};

struct Entry {
    std::string id;
    std::string category;
    std::string implementation;
    std::uint32_t revision = 0;
    std::uint32_t cost = 0;
    std::vector<Capability> provides;
    std::vector<Requirement> requirements;
    std::vector<std::string> conflicts;
};

On1xSDK_Status map_status(mulib_cdd_status_t status) noexcept {
    switch (status) {
    case MULIB_CDD_OK:
        return ON1X_SDK_OK;
    case MULIB_CDD_INVALID:
        return ON1X_SDK_INVALID_ARGUMENT;
    case MULIB_CDD_DUPLICATE:
        return ON1X_SDK_DUPLICATE;
    case MULIB_CDD_NOT_FOUND:
        return ON1X_SDK_NOT_FOUND;
    case MULIB_CDD_CONFLICT:
        return ON1X_SDK_CONFLICT;
    case MULIB_CDD_UNSAT:
        return ON1X_SDK_UNSATISFIABLE;
    case MULIB_CDD_LIMIT:
    case MULIB_CDD_NOMEM:
        return ON1X_SDK_LIMIT;
    case MULIB_CDD_CALLBACK:
        return ON1X_SDK_CALLBACK_FAILED;
    default:
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status validate_capability(
    const On1xSDK_CatalogueCapability &capability
) noexcept {
    return capability.revision &&
        !capability.reserved &&
        on1x_sdk_identifier_validate(capability.name) == ON1X_SDK_OK
        ? ON1X_SDK_OK : ON1X_SDK_INVALID_ARGUMENT;
}

On1xSDK_Status validate_requirement(
    const On1xSDK_CatalogueRequirement &requirement
) noexcept {
    return requirement.min_revision &&
        (!requirement.max_revision ||
         requirement.max_revision >= requirement.min_revision) &&
        on1x_sdk_identifier_validate(requirement.name) == ON1X_SDK_OK
        ? ON1X_SDK_OK : ON1X_SDK_INVALID_ARGUMENT;
}

On1xSDK_Status copy_entry(
    const On1xSDK_CatalogueEntry *source, Entry &destination
) {
    const auto header = on1x_sdk_abi_header_validate(
        source, sizeof(On1xSDK_CatalogueEntry),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (header != ON1X_SDK_OK) {
        return header;
    }
    if (on1x_sdk_identifier_validate(source->id) != ON1X_SDK_OK ||
        on1x_sdk_identifier_validate(source->category) != ON1X_SDK_OK ||
        on1x_sdk_identifier_validate(source->implementation_label) !=
            ON1X_SDK_OK ||
        !source->revision ||
        (!source->provides && source->provide_count) ||
        (!source->requirements && source->requirement_count) ||
        (!source->conflicts && source->conflict_count)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (source->provide_count > ON1X_SDK_CATALOGUE_MAX_ITEMS_PER_ENTRY ||
        source->requirement_count >
            ON1X_SDK_CATALOGUE_MAX_ITEMS_PER_ENTRY ||
        source->conflict_count >
            ON1X_SDK_CATALOGUE_MAX_ITEMS_PER_ENTRY) {
        return ON1X_SDK_LIMIT;
    }

    destination.id.assign(source->id.data, source->id.size);
    destination.category.assign(
        source->category.data, source->category.size
    );
    destination.implementation.assign(
        source->implementation_label.data,
        source->implementation_label.size
    );
    destination.revision = source->revision;
    destination.cost = source->cost;
    destination.provides.reserve(source->provide_count);
    destination.requirements.reserve(source->requirement_count);
    destination.conflicts.reserve(source->conflict_count);

    for (std::size_t i = 0; i < source->provide_count; ++i) {
        const auto &capability = source->provides[i];
        const auto status = validate_capability(capability);
        if (status != ON1X_SDK_OK) {
            return status;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (on1x_sdk_string_equal(
                    capability.name, source->provides[j].name) &&
                capability.revision == source->provides[j].revision) {
                return ON1X_SDK_DUPLICATE;
            }
        }
        destination.provides.push_back({
            std::string(capability.name.data, capability.name.size),
            capability.revision
        });
    }
    for (std::size_t i = 0; i < source->requirement_count; ++i) {
        const auto &requirement = source->requirements[i];
        const auto status = validate_requirement(requirement);
        if (status != ON1X_SDK_OK) {
            return status;
        }
        destination.requirements.push_back({
            std::string(requirement.name.data, requirement.name.size),
            requirement.min_revision, requirement.max_revision
        });
    }
    for (std::size_t i = 0; i < source->conflict_count; ++i) {
        const auto conflict = source->conflicts[i];
        if (on1x_sdk_identifier_validate(conflict) != ON1X_SDK_OK ||
            on1x_sdk_string_equal(conflict, source->id)) {
            return ON1X_SDK_INVALID_ARGUMENT;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (on1x_sdk_string_equal(conflict, source->conflicts[j])) {
                return ON1X_SDK_DUPLICATE;
            }
        }
        destination.conflicts.emplace_back(
            conflict.data, conflict.size
        );
    }
    return ON1X_SDK_OK;
}

struct NativeCatalogue {
    std::vector<std::vector<mulib_cdd_cap_t>> capabilities;
    std::vector<std::vector<mulib_cdd_req_t>> requirements;
    std::vector<std::vector<const char *>> conflicts;
    std::vector<mulib_cdd_entry_t> entries;
    mulib_cdd_catalogue_t view{};
};

NativeCatalogue adapt(const std::vector<Entry> &source) {
    NativeCatalogue native;
    native.capabilities.resize(source.size());
    native.requirements.resize(source.size());
    native.conflicts.resize(source.size());
    native.entries.resize(source.size());
    for (std::size_t i = 0; i < source.size(); ++i) {
        const auto &entry = source[i];
        auto &capabilities = native.capabilities[i];
        auto &requirements = native.requirements[i];
        auto &conflicts = native.conflicts[i];
        capabilities.reserve(entry.provides.size());
        requirements.reserve(entry.requirements.size());
        conflicts.reserve(entry.conflicts.size());
        for (const auto &capability : entry.provides) {
            capabilities.push_back({
                capability.name.c_str(), capability.revision
            });
        }
        for (const auto &requirement : entry.requirements) {
            requirements.push_back({
                requirement.name.c_str(), requirement.minimum,
                requirement.maximum
            });
        }
        for (const auto &conflict : entry.conflicts) {
            conflicts.push_back(conflict.c_str());
        }
        native.entries[i] = {
            entry.id.c_str(), entry.category.c_str(),
            entry.implementation.c_str(), entry.revision, entry.cost,
            capabilities.data(), capabilities.size(),
            requirements.data(), requirements.size(),
            conflicts.data(), conflicts.size(),
            nullptr, 0
        };
    }
    native.view = {native.entries.data(), native.entries.size()};
    return native;
}

bool owner_thread(const On1xSDK_Catalogue *catalogue) noexcept;

} // namespace

struct On1xSDK_Catalogue {
    std::vector<Entry> entries;
    std::uint64_t revision = 1;
    std::thread::id owner = std::this_thread::get_id();
};

struct On1xSDK_CatalogueEntryView {
    const On1xSDK_Catalogue *catalogue = nullptr;
    std::uint64_t revision = 0;
    std::size_t index = 0;
};

struct On1xSDK_CataloguePlan {
    On1xSDK_CataloguePlanInfo info{};
    std::vector<std::pair<std::size_t, std::string>> order;
};

namespace {

bool owner_thread(const On1xSDK_Catalogue *catalogue) noexcept {
    return catalogue && catalogue->owner == std::this_thread::get_id();
}

On1xSDK_Status view_entry(
    const On1xSDK_CatalogueEntryView *view, const Entry **out
) noexcept {
    if (out) {
        *out = nullptr;
    }
    if (!view || !view->catalogue || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!owner_thread(view->catalogue)) {
        return ON1X_SDK_BUSY;
    }
    if (view->revision != view->catalogue->revision) {
        return ON1X_SDK_STALE;
    }
    if (view->index >= view->catalogue->entries.size()) {
        return ON1X_SDK_NOT_FOUND;
    }
    *out = &view->catalogue->entries[view->index];
    return ON1X_SDK_OK;
}

} // namespace

extern "C" On1xSDK_Status on1x_sdk_catalogue_create(
    On1xSDK_Catalogue **out
) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = nullptr;
    try {
        *out = new On1xSDK_Catalogue;
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_catalogue_destroy(
    On1xSDK_Catalogue *catalogue
) {
    if (owner_thread(catalogue)) {
        delete catalogue;
    }
}

extern "C" On1xSDK_Status on1x_sdk_catalogue_add(
    On1xSDK_Catalogue *catalogue,
    const On1xSDK_CatalogueEntry *entry
) {
    if (!catalogue || !entry) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!owner_thread(catalogue)) {
        return ON1X_SDK_BUSY;
    }
    if (catalogue->entries.size() >= ON1X_SDK_CATALOGUE_MAX_ENTRIES) {
        return ON1X_SDK_LIMIT;
    }
    try {
        Entry copied;
        auto status = copy_entry(entry, copied);
        if (status != ON1X_SDK_OK) {
            return status;
        }
        if (std::any_of(
                catalogue->entries.begin(), catalogue->entries.end(),
                [&](const Entry &existing) {
                    return existing.id == copied.id;
                })) {
            return ON1X_SDK_DUPLICATE;
        }
        auto staged = catalogue->entries;
        staged.push_back(std::move(copied));
        const auto native = adapt(staged);
        status = map_status(mulib_cdd_validate(&native.view, nullptr));
        if (status != ON1X_SDK_OK) {
            return status;
        }
        catalogue->entries.swap(staged);
        ++catalogue->revision;
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" std::uint64_t on1x_sdk_catalogue_revision(
    const On1xSDK_Catalogue *catalogue
) {
    return owner_thread(catalogue) ? catalogue->revision : 0;
}

extern "C" std::size_t on1x_sdk_catalogue_count(
    const On1xSDK_Catalogue *catalogue
) {
    return owner_thread(catalogue) ? catalogue->entries.size() : 0;
}

extern "C" On1xSDK_Status on1x_sdk_catalogue_entry(
    const On1xSDK_Catalogue *catalogue, std::size_t index,
    On1xSDK_CatalogueEntryView **out_view
) {
    if (out_view) {
        *out_view = nullptr;
    }
    if (!catalogue || !out_view) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!owner_thread(catalogue)) {
        return ON1X_SDK_BUSY;
    }
    if (index >= catalogue->entries.size()) {
        return ON1X_SDK_NOT_FOUND;
    }
    try {
        *out_view = new On1xSDK_CatalogueEntryView{
            catalogue, catalogue->revision, index
        };
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_catalogue_entry_view_destroy(
    On1xSDK_CatalogueEntryView *view
) {
    delete view;
}

extern "C" On1xSDK_Status on1x_sdk_catalogue_entry_identity(
    const On1xSDK_CatalogueEntryView *view,
    On1xSDK_StringView *id, On1xSDK_StringView *category,
    std::uint32_t *revision, std::uint32_t *cost
) {
    if (!id || !category || !revision || !cost) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *id = {};
    *category = {};
    *revision = 0;
    *cost = 0;
    const Entry *entry = nullptr;
    const auto status = view_entry(view, &entry);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    *id = {entry->id.data(), entry->id.size()};
    *category = {entry->category.data(), entry->category.size()};
    *revision = entry->revision;
    *cost = entry->cost;
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_catalogue_entry_provide_count(
    const On1xSDK_CatalogueEntryView *view, std::size_t *count
) {
    if (!count) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *count = 0;
    const Entry *entry = nullptr;
    const auto status = view_entry(view, &entry);
    if (status == ON1X_SDK_OK) {
        *count = entry->provides.size();
    }
    return status;
}

extern "C" On1xSDK_Status on1x_sdk_catalogue_entry_provide_at(
    const On1xSDK_CatalogueEntryView *view, std::size_t index,
    On1xSDK_CatalogueCapability *out
) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = {};
    const Entry *entry = nullptr;
    auto status = view_entry(view, &entry);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (index >= entry->provides.size()) {
        return ON1X_SDK_NOT_FOUND;
    }
    const auto &capability = entry->provides[index];
    *out = {
        {capability.name.data(), capability.name.size()},
        capability.revision, 0
    };
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_catalogue_entry_requirement_count(
    const On1xSDK_CatalogueEntryView *view, std::size_t *count
) {
    if (!count) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *count = 0;
    const Entry *entry = nullptr;
    const auto status = view_entry(view, &entry);
    if (status == ON1X_SDK_OK) {
        *count = entry->requirements.size();
    }
    return status;
}

extern "C" On1xSDK_Status on1x_sdk_catalogue_entry_requirement_at(
    const On1xSDK_CatalogueEntryView *view, std::size_t index,
    On1xSDK_CatalogueRequirement *out
) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = {};
    const Entry *entry = nullptr;
    auto status = view_entry(view, &entry);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (index >= entry->requirements.size()) {
        return ON1X_SDK_NOT_FOUND;
    }
    const auto &requirement = entry->requirements[index];
    *out = {
        {requirement.name.data(), requirement.name.size()},
        requirement.minimum, requirement.maximum
    };
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_catalogue_resolve(
    const On1xSDK_Catalogue *catalogue,
    const On1xSDK_CatalogueProblem *problem,
    On1xSDK_CataloguePlan **out
) {
    if (out) {
        *out = nullptr;
    }
    if (!catalogue || !problem || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!owner_thread(catalogue)) {
        return ON1X_SDK_BUSY;
    }
    auto status = on1x_sdk_abi_header_validate(
        problem, sizeof(On1xSDK_CatalogueProblem),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if ((!problem->goals && problem->goal_count) ||
        (!problem->available && problem->available_count) ||
        (!problem->allowed_entry_ids &&
         problem->allowed_entry_count)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (problem->goal_count > ON1X_SDK_CATALOGUE_MAX_ITEMS_PER_ENTRY ||
        problem->available_count >
            ON1X_SDK_CATALOGUE_MAX_ITEMS_PER_ENTRY ||
        problem->allowed_entry_count >
            ON1X_SDK_CATALOGUE_MAX_ENTRIES ||
        problem->max_search_nodes > maximum_search_nodes) {
        return ON1X_SDK_LIMIT;
    }
    if (!problem->max_search_nodes) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }

    try {
        const auto native_catalogue = adapt(catalogue->entries);
        std::vector<std::string> goal_names;
        std::vector<std::string> available_names;
        std::vector<mulib_cdd_req_t> goals;
        std::vector<mulib_cdd_cap_t> available;
        goal_names.reserve(problem->goal_count);
        available_names.reserve(problem->available_count);
        goals.reserve(problem->goal_count);
        available.reserve(problem->available_count);
        for (std::size_t i = 0; i < problem->goal_count; ++i) {
            const auto &goal = problem->goals[i];
            status = validate_requirement(goal);
            if (status != ON1X_SDK_OK) {
                return status;
            }
            goal_names.emplace_back(goal.name.data, goal.name.size);
        }
        for (std::size_t i = 0; i < problem->goal_count; ++i) {
            const auto &goal = problem->goals[i];
            goals.push_back({
                goal_names[i].c_str(),
                goal.min_revision, goal.max_revision
            });
        }
        for (std::size_t i = 0; i < problem->available_count; ++i) {
            const auto &capability = problem->available[i];
            status = validate_capability(capability);
            if (status != ON1X_SDK_OK) {
                return status;
            }
            available_names.emplace_back(
                capability.name.data, capability.name.size
            );
        }
        for (std::size_t i = 0; i < problem->available_count; ++i) {
            const auto &capability = problem->available[i];
            available.push_back({
                available_names[i].c_str(), capability.revision
            });
        }

        std::uint64_t allowed = UINT64_MAX;
        if (problem->allowed_entry_count) {
            allowed = 0;
            for (std::size_t i = 0;
                 i < problem->allowed_entry_count; ++i) {
                const auto id = problem->allowed_entry_ids[i];
                if (on1x_sdk_identifier_validate(id) != ON1X_SDK_OK) {
                    return ON1X_SDK_INVALID_ARGUMENT;
                }
                for (std::size_t j = 0; j < i; ++j) {
                    if (on1x_sdk_string_equal(
                            id, problem->allowed_entry_ids[j])) {
                        return ON1X_SDK_DUPLICATE;
                    }
                }
                const auto found = std::find_if(
                    catalogue->entries.begin(), catalogue->entries.end(),
                    [&](const Entry &entry) {
                        return entry.id.size() == id.size &&
                            std::memcmp(
                                entry.id.data(), id.data, id.size) == 0;
                    }
                );
                if (found == catalogue->entries.end()) {
                    return ON1X_SDK_NOT_FOUND;
                }
                const auto index = static_cast<std::size_t>(
                    found - catalogue->entries.begin()
                );
                allowed |= UINT64_C(1) << index;
            }
        } else if (catalogue->entries.size() < 64) {
            allowed = catalogue->entries.empty()
                ? 0
                : (UINT64_C(1) << catalogue->entries.size()) - 1;
        }

        const mulib_cdd_problem_t native_problem{
            goals.data(), goals.size(),
            available.data(), available.size(),
            allowed, problem->max_cost
        };
        mulib_cdd_plan_t native_plan{};
        mulib_cdd_resolve_stats_t statistics{};
        const auto native_status = mulib_cdd_resolve(
            &native_catalogue.view, &native_problem,
            problem->max_search_nodes, &native_plan, &statistics
        );
        status = map_status(native_status);
        if (native_status != MULIB_CDD_OK &&
            native_status != MULIB_CDD_LIMIT) {
            return status;
        }
        if (native_plan.found &&
            mulib_cdd_plan_validate(
                &native_catalogue.view, &native_problem,
                &native_plan, nullptr) != MULIB_CDD_OK) {
            return ON1X_SDK_ERROR;
        }

        auto plan = std::make_unique<On1xSDK_CataloguePlan>();
        plan->info = {
            catalogue->revision, native_plan.cost, native_plan.count,
            statistics.expanded,
            static_cast<std::uint32_t>(native_plan.found != 0),
            static_cast<std::uint32_t>(statistics.optimal != 0)
        };
        plan->order.reserve(native_plan.count);
        for (std::size_t i = 0; i < native_plan.count; ++i) {
            const auto index = native_plan.order[i];
            plan->order.emplace_back(
                index, catalogue->entries[index].id
            );
        }
        *out = plan.release();
        return status;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_catalogue_plan_destroy(
    On1xSDK_CataloguePlan *plan
) {
    delete plan;
}

extern "C" On1xSDK_Status on1x_sdk_catalogue_plan_info(
    const On1xSDK_CataloguePlan *plan,
    On1xSDK_CataloguePlanInfo *out
) {
    if (!plan || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = plan->info;
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_catalogue_plan_entry(
    const On1xSDK_CataloguePlan *plan, std::size_t order_index,
    std::size_t *catalogue_index, On1xSDK_StringView *entry_id
) {
    if (!plan || !catalogue_index || !entry_id) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *catalogue_index = 0;
    *entry_id = {};
    if (order_index >= plan->order.size()) {
        return ON1X_SDK_NOT_FOUND;
    }
    const auto &entry = plan->order[order_index];
    *catalogue_index = entry.first;
    *entry_id = {entry.second.data(), entry.second.size()};
    return ON1X_SDK_OK;
}
