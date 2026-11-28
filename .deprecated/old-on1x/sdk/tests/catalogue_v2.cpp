#include "On1xSDK-Catalogue.h"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        std::abort(); \
    } \
} while (false)

namespace {

On1xSDK_StringView text(const char *value) {
    return {value, std::strlen(value)};
}

On1xSDK_CatalogueEntry entry(
    const char *id, std::uint32_t cost,
    const On1xSDK_CatalogueCapability *provides,
    std::size_t provide_count,
    const On1xSDK_CatalogueRequirement *requirements = nullptr,
    std::size_t requirement_count = 0
) {
    return {
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_CatalogueEntry),
        text(id), text("backend"), text(id), 1, cost,
        provides, provide_count,
        requirements, requirement_count,
        nullptr, 0
    };
}

} // namespace

int main() {
    On1xSDK_Catalogue *raw_catalogue = nullptr;
    CHECK(on1x_sdk_catalogue_create(&raw_catalogue) == ON1X_SDK_OK);
    std::unique_ptr<
        On1xSDK_Catalogue, decltype(&on1x_sdk_catalogue_destroy)
    > catalogue(raw_catalogue, on1x_sdk_catalogue_destroy);

    const On1xSDK_CatalogueCapability provides_mid[]{
        {text("cap.mid"), 1, 0}
    };
    const On1xSDK_CatalogueCapability provides_goal[]{
        {text("cap.goal"), 1, 0}
    };
    const On1xSDK_CatalogueRequirement requires_mid[]{
        {text("cap.mid"), 1, 1}
    };
    auto base = entry("base", 1, provides_mid, 1);
    auto via = entry(
        "via", 10, provides_goal, 1, requires_mid, 1
    );
    auto direct = entry("direct", 5, provides_goal, 1);
    CHECK(on1x_sdk_catalogue_add(catalogue.get(), &base) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_catalogue_add(catalogue.get(), &via) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_catalogue_count(catalogue.get()) == 2);
    std::size_t other_thread_count = 1;
    std::uint64_t other_thread_revision = 1;
    On1xSDK_Status other_thread_entry_status = ON1X_SDK_OK;
    On1xSDK_CatalogueEntryView *other_thread_view =
        reinterpret_cast<On1xSDK_CatalogueEntryView *>(1);
    std::thread other_thread([&] {
        other_thread_count = on1x_sdk_catalogue_count(catalogue.get());
        other_thread_revision =
            on1x_sdk_catalogue_revision(catalogue.get());
        other_thread_entry_status = on1x_sdk_catalogue_entry(
            catalogue.get(), 0, &other_thread_view
        );
    });
    other_thread.join();
    CHECK(other_thread_count == 0 && other_thread_revision == 0);
    CHECK(other_thread_entry_status == ON1X_SDK_BUSY);
    CHECK(other_thread_view == nullptr);

    On1xSDK_CatalogueEntryView *raw_view = nullptr;
    CHECK(on1x_sdk_catalogue_entry(
        catalogue.get(), 0, &raw_view) == ON1X_SDK_OK);
    std::unique_ptr<
        On1xSDK_CatalogueEntryView,
        decltype(&on1x_sdk_catalogue_entry_view_destroy)
    > stale_view(raw_view, on1x_sdk_catalogue_entry_view_destroy);
    On1xSDK_StringView id{}, category{};
    std::uint32_t revision = 0, cost = 0;
    CHECK(on1x_sdk_catalogue_entry_identity(
        stale_view.get(), &id, &category, &revision, &cost) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_string_equal(id, text("base")) && cost == 1);

    CHECK(on1x_sdk_catalogue_add(catalogue.get(), &direct) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_catalogue_entry_identity(
        stale_view.get(), &id, &category, &revision, &cost) ==
          ON1X_SDK_STALE);
    CHECK(on1x_sdk_catalogue_add(catalogue.get(), &direct) ==
          ON1X_SDK_DUPLICATE);

    const On1xSDK_CatalogueRequirement goal[]{
        {text("cap.goal"), 1, 1}
    };
    On1xSDK_CatalogueProblem problem{
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_CatalogueProblem),
        goal, 1, nullptr, 0, nullptr, 0, 100, 100
    };
    On1xSDK_CataloguePlan *raw_plan = nullptr;
    CHECK(on1x_sdk_catalogue_resolve(
        catalogue.get(), &problem, &raw_plan) == ON1X_SDK_OK);
    std::unique_ptr<
        On1xSDK_CataloguePlan,
        decltype(&on1x_sdk_catalogue_plan_destroy)
    > plan(raw_plan, on1x_sdk_catalogue_plan_destroy);
    On1xSDK_CataloguePlanInfo info{};
    CHECK(on1x_sdk_catalogue_plan_info(plan.get(), &info) ==
          ON1X_SDK_OK);
    CHECK(info.found == 1 && info.optimal == 1 &&
          info.cost == 5 && info.entry_count == 1);
    std::size_t selected_index = 0;
    On1xSDK_StringView selected_id{};
    CHECK(on1x_sdk_catalogue_plan_entry(
        plan.get(), 0, &selected_index, &selected_id) == ON1X_SDK_OK);
    CHECK(selected_index == 2 &&
          on1x_sdk_string_equal(selected_id, text("direct")));

    problem.max_search_nodes = 3;
    raw_plan = nullptr;
    CHECK(on1x_sdk_catalogue_resolve(
        catalogue.get(), &problem, &raw_plan) == ON1X_SDK_LIMIT);
    plan.reset(raw_plan);
    CHECK(on1x_sdk_catalogue_plan_info(plan.get(), &info) ==
          ON1X_SDK_OK);
    CHECK(info.found == 1 && info.optimal == 0 &&
          info.cost == 6 && info.entry_count == 2);

    const On1xSDK_StringView allowed[]{
        text("base"), text("via")
    };
    problem.allowed_entry_ids = allowed;
    problem.allowed_entry_count = 2;
    problem.max_search_nodes = 100;
    raw_plan = nullptr;
    CHECK(on1x_sdk_catalogue_resolve(
        catalogue.get(), &problem, &raw_plan) == ON1X_SDK_OK);
    plan.reset(raw_plan);
    CHECK(on1x_sdk_catalogue_plan_info(plan.get(), &info) ==
          ON1X_SDK_OK);
    CHECK(info.found == 1 && info.optimal == 1 &&
          info.cost == 11 && info.entry_count == 2);

    problem.allowed_entry_ids = nullptr;
    problem.allowed_entry_count = 0;
    problem.max_search_nodes = 0;
    raw_plan = reinterpret_cast<On1xSDK_CataloguePlan *>(1);
    CHECK(on1x_sdk_catalogue_resolve(
        catalogue.get(), &problem, &raw_plan) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(raw_plan == nullptr);
    return 0;
}
