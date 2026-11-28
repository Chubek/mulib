#include "On1xSDK-Extension.h"
#include "On1xSDK-PluginDependency.h"
#include "On1xSDK-Registry.h"
#include "../src/RegistryInternal.hpp"

#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        std::abort(); \
    } \
} while (false)

namespace {

On1xSDK_StringView text(const char *value) {
    return {value, std::char_traits<char>::length(value)};
}

struct State {
    std::string name;
    std::string entry;
    std::vector<std::string> *events = nullptr;
    On1xSDK_Status registration_status = ON1X_SDK_OK;
    On1xSDK_Status initialization_status = ON1X_SDK_OK;
    On1xSDK_Status shutdown_status = ON1X_SDK_OK;
    bool capability_seen = false;
    bool mutate_lifecycle = false;
    On1xSDK_Status initialize_mutation_status = ON1X_SDK_OK;
    On1xSDK_Status shutdown_mutation_status = ON1X_SDK_OK;
    On1xSDK_Registry *active_registry = nullptr;
};

extern "C" On1xSDK_Status register_extension(
    const On1xSDK_Host *host, On1xSDK_Registry *registry, void *opaque
) {
    auto &state = *static_cast<State *>(opaque);
    state.active_registry = registry;
    state.events->push_back("register:" + state.name);
    CHECK(host && host->abi_version == ON1X_SDK_ABI_VERSION_V2);
    CHECK(host->struct_size >= sizeof(On1xSDK_Host));
    state.capability_seen = host->capability_granted(
        host->context_token, text("cap.build"), 1
    );
    if (state.registration_status != ON1X_SDK_OK)
        return state.registration_status;
    return on1x::sdk::add(
        registry, on1x::sdk::Kind::Analyzer, state.entry,
        std::make_shared<const int>(7)
    );
}

extern "C" On1xSDK_Status initialize_extension(
    const On1xSDK_Host *, void *opaque
) {
    auto &state = *static_cast<State *>(opaque);
    state.events->push_back("initialize:" + state.name);
    if (state.mutate_lifecycle) {
        state.initialize_mutation_status = on1x::sdk::add(
            state.active_registry, on1x::sdk::Kind::Analyzer,
            "forbidden.initialize", std::make_shared<const int>(8)
        );
    }
    return state.initialization_status;
}

extern "C" On1xSDK_Status shutdown_extension(
    const On1xSDK_Host *, void *opaque
) {
    auto &state = *static_cast<State *>(opaque);
    state.events->push_back("shutdown:" + state.name);
    if (state.mutate_lifecycle) {
        state.shutdown_mutation_status = on1x::sdk::add(
            state.active_registry, on1x::sdk::Kind::Analyzer,
            "forbidden.shutdown", std::make_shared<const int>(9)
        );
    }
    return state.shutdown_status;
}

bool v1_host_was_inert = false;

extern "C" On1xSDK_Status register_v1(
    const On1xSDK_Host *host, On1xSDK_Registry *
) {
    CHECK(host);
    v1_host_was_inert =
        host->abi_version == 0 && host->struct_size == 0 &&
        host->feature_bits == 0 && host->context_token == nullptr &&
        host->allocator == nullptr && host->submit_candidate == nullptr;
    return ON1X_SDK_OK;
}

On1xSDK_ExtensionV2 extension(
    State &state, const char *version, const char *key,
    const On1xSDK_PluginDependency *dependencies = nullptr,
    std::size_t dependency_count = 0,
    const On1xSDK_CapabilityRequest *capabilities = nullptr,
    std::size_t capability_count = 0
) {
    return {
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_ExtensionV2),
        {state.name.data(), state.name.size()}, text(version), text(key),
        dependencies, dependency_count,
        capabilities, capability_count,
        register_extension, initialize_extension, shutdown_extension,
        &state
    };
}

On1xSDK_Registry *make_registry() {
    const On1xSDK_CapabilityGrant grants[]{
        {text("cap.build"), 1, 0}
    };
    const On1xSDK_RegistryConfig config{
        ON1X_SDK_ABI_VERSION_V2, sizeof(On1xSDK_RegistryConfig),
        nullptr, grants, 1, 16, 0
    };
    On1xSDK_Registry *registry = nullptr;
    CHECK(on1x_sdk_registry_create_v2(&config, &registry) == ON1X_SDK_OK);
    return registry;
}

} // namespace

int main() {
    On1xSDK_RegistryConfig partial_config{};
    partial_config.abi_version = ON1X_SDK_ABI_VERSION_V2;
    partial_config.struct_size =
        offsetof(On1xSDK_RegistryConfig, context) + 1;
    reinterpret_cast<unsigned char *>(&partial_config)
        [offsetof(On1xSDK_RegistryConfig, context)] = 1;
    On1xSDK_Registry *partial_registry = nullptr;
    CHECK(on1x_sdk_registry_create_v2(
        &partial_config, &partial_registry) == ON1X_SDK_OK);
    on1x_sdk_registry_destroy(partial_registry);

    On1xSDK_Registry *raw_v1_registry = nullptr;
    CHECK(on1x_sdk_registry_create(&raw_v1_registry) == ON1X_SDK_OK);
    const On1xSDK_Extension v1_extension{
        ON1X_SDK_ABI_VERSION_V1, sizeof(On1xSDK_Extension),
        "v1.inert", "1.0", register_v1
    };
    CHECK(on1x_sdk_registry_register(
        raw_v1_registry, &v1_extension) == ON1X_SDK_OK);
    CHECK(v1_host_was_inert);
    on1x_sdk_registry_destroy(raw_v1_registry);

    const On1xSDK_CapabilityRequest required_capability{
        text("cap.build"), 1, 1, ON1X_SDK_CAPABILITY_REQUIRED, 0
    };
    std::vector<std::string> events;
    State base{"base", "entry.base", &events};
    State consumer{"consumer", "entry.consumer", &events};
    const On1xSDK_PluginDependency base_dependency{
        text("base"), text("1.0.0"), text("1.0.0")
    };
    auto base_extension = extension(
        base, "1.0.0", "z-base", nullptr, 0,
        &required_capability, 1
    );
    auto consumer_extension = extension(
        consumer, "2.0.0", "a-consumer", &base_dependency, 1,
        &required_capability, 1
    );
    auto partial_extension = base_extension;
    std::memset(
        reinterpret_cast<unsigned char *>(&partial_extension) +
            offsetof(On1xSDK_ExtensionV2, initialize),
        0xa5,
        sizeof partial_extension -
            offsetof(On1xSDK_ExtensionV2, initialize)
    );
    partial_extension.struct_size =
        offsetof(On1xSDK_ExtensionV2, initialize) + 1;
    CHECK(on1x_sdk_extension_v2_validate(&partial_extension) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_extension_v2_validate(&base_extension) == ON1X_SDK_OK);
    CHECK(on1x_sdk_plugin_dependency_matches(
        &base_dependency, text("base"), text("1.0.0")));
    CHECK(!on1x_sdk_plugin_dependency_matches(
        &base_dependency, text("base"), text("1.0.1")));

    std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registry(make_registry(), on1x_sdk_registry_destroy);
    const On1xSDK_ExtensionV2 *batch[]{
        &consumer_extension, &base_extension
    };
    On1xSDK_RegistryTransactionReport report{};
    report.abi_version = ON1X_SDK_ABI_VERSION_V2;
    report.struct_size = sizeof(On1xSDK_RegistryTransactionReport);
    const auto revision_before = on1x_sdk_registry_revision(registry.get());
    CHECK(on1x_sdk_registry_register_batch_v2(
        registry.get(), batch, 2, &report) == ON1X_SDK_OK);
    CHECK(report.primary_status == ON1X_SDK_OK &&
          report.validated == 2 && report.initialized == 2 &&
          report.compensated == 0);
    CHECK(on1x_sdk_registry_revision(registry.get()) ==
          revision_before + 1);
    CHECK((events == std::vector<std::string>{
        "register:base", "register:consumer",
        "initialize:base", "initialize:consumer"
    }));
    CHECK(base.capability_seen && consumer.capability_seen);
    CHECK(on1x::sdk::count(
        registry.get(), on1x::sdk::Kind::Analyzer) == 2);

    std::size_t other_thread_count = 1;
    std::uint64_t other_thread_revision = 1;
    On1xSDK_Status other_thread_snapshot_status = ON1X_SDK_OK;
    On1xSDK_RegistrySnapshot *other_thread_snapshot =
        reinterpret_cast<On1xSDK_RegistrySnapshot *>(1);
    std::thread other_thread([&] {
        other_thread_count = on1x::sdk::count(
            registry.get(), on1x::sdk::Kind::Analyzer
        );
        other_thread_revision =
            on1x_sdk_registry_revision(registry.get());
        other_thread_snapshot_status = on1x_sdk_registry_snapshot(
            registry.get(), &other_thread_snapshot
        );
    });
    other_thread.join();
    CHECK(other_thread_count == 0 && other_thread_revision == 0);
    CHECK(other_thread_snapshot_status == ON1X_SDK_BUSY);
    CHECK(other_thread_snapshot == nullptr);

    On1xSDK_RegistrySnapshot *raw_snapshot = nullptr;
    CHECK(on1x_sdk_registry_snapshot(
        registry.get(), &raw_snapshot) == ON1X_SDK_OK);
    std::unique_ptr<
        On1xSDK_RegistrySnapshot,
        decltype(&on1x_sdk_registry_snapshot_destroy)
    > snapshot(raw_snapshot, on1x_sdk_registry_snapshot_destroy);
    std::size_t snapshot_count = 0;
    CHECK(on1x_sdk_registry_snapshot_count(
        snapshot.get(), &snapshot_count) == ON1X_SDK_OK);
    CHECK(snapshot_count == 2);
    On1xSDK_RegistryExtensionView view{};
    CHECK(on1x_sdk_registry_snapshot_at(
        snapshot.get(), 0, &view) == ON1X_SDK_OK);
    CHECK(on1x_sdk_string_equal(view.name, text("base")));
    CHECK(view.abi_version == ON1X_SDK_ABI_VERSION_V2);
    CHECK(view.lifecycle_state == ON1X_SDK_LIFECYCLE_ACTIVE);
    CHECK(view.granted_capability_count == 1);

    CHECK(on1x_sdk_registry_close(registry.get()) == ON1X_SDK_OK);
    CHECK((events == std::vector<std::string>{
        "register:base", "register:consumer",
        "initialize:base", "initialize:consumer",
        "shutdown:consumer", "shutdown:base"
    }));
    CHECK(on1x_sdk_registry_register_v2(
        registry.get(), &base_extension, nullptr) ==
          ON1X_SDK_INVALID_STATE);
    CHECK(on1x_sdk_registry_close(registry.get()) == ON1X_SDK_OK);

    std::vector<std::string> rollback_events;
    State first{"first", "entry.first", &rollback_events};
    State second{"second", "entry.second", &rollback_events};
    second.initialization_status = ON1X_SDK_ERROR;
    const On1xSDK_PluginDependency first_dependency{
        text("first"), text("1.0.0"), {nullptr, 0}
    };
    auto first_extension = extension(first, "1.0.0", "first-key");
    auto second_extension = extension(
        second, "1.0.0", "second-key", &first_dependency, 1
    );
    std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        rollback_registry(make_registry(), on1x_sdk_registry_destroy);
    const On1xSDK_ExtensionV2 *rollback_batch[]{
        &second_extension, &first_extension
    };
    report = {};
    report.abi_version = ON1X_SDK_ABI_VERSION_V2;
    report.struct_size = sizeof(On1xSDK_RegistryTransactionReport);
    CHECK(on1x_sdk_registry_register_batch_v2(
        rollback_registry.get(), rollback_batch, 2, &report) ==
          ON1X_SDK_ERROR);
    CHECK(report.primary_status == ON1X_SDK_ERROR);
    CHECK(report.initialized == 1 && report.compensated == 2);
    CHECK(report.compensation_status == ON1X_SDK_OK);
    CHECK((rollback_events == std::vector<std::string>{
        "register:first", "register:second",
        "initialize:first", "initialize:second",
        "shutdown:second", "shutdown:first"
    }));
    CHECK(on1x::sdk::count(
        rollback_registry.get(), on1x::sdk::Kind::Analyzer) == 0);

    std::vector<std::string> registration_failure_events;
    State registered{
        "registered", "entry.registered", &registration_failure_events
    };
    State registration_failure{
        "registration.failure", "entry.registration.failure",
        &registration_failure_events
    };
    registration_failure.registration_status = ON1X_SDK_ERROR;
    auto registered_extension =
        extension(registered, "1.0.0", "registered-key");
    const On1xSDK_PluginDependency registered_dependency{
        text("registered"), text("1.0.0"), {nullptr, 0}
    };
    auto registration_failure_extension = extension(
        registration_failure, "1.0.0", "registration-failure-key",
        &registered_dependency, 1
    );
    std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        registration_failure_registry(
            make_registry(), on1x_sdk_registry_destroy
        );
    const On1xSDK_ExtensionV2 *registration_failure_batch[]{
        &registration_failure_extension, &registered_extension
    };
    report = {};
    report.abi_version = ON1X_SDK_ABI_VERSION_V2;
    report.struct_size = sizeof(On1xSDK_RegistryTransactionReport);
    CHECK(on1x_sdk_registry_register_batch_v2(
        registration_failure_registry.get(),
        registration_failure_batch, 2, &report) == ON1X_SDK_ERROR);
    CHECK(report.initialized == 0 && report.compensated == 2);
    CHECK((registration_failure_events == std::vector<std::string>{
        "register:registered", "register:registration.failure",
        "shutdown:registration.failure", "shutdown:registered"
    }));
    CHECK(on1x::sdk::count(
        registration_failure_registry.get(),
        on1x::sdk::Kind::Analyzer) == 0);

    std::vector<std::string> mutation_events;
    State guarded{"guarded", "entry.guarded", &mutation_events};
    guarded.mutate_lifecycle = true;
    auto guarded_extension =
        extension(guarded, "1.0.0", "guarded-key");
    std::unique_ptr<On1xSDK_Registry, decltype(&on1x_sdk_registry_destroy)>
        guarded_registry(make_registry(), on1x_sdk_registry_destroy);
    CHECK(on1x_sdk_registry_register_v2(
        guarded_registry.get(), &guarded_extension, nullptr) ==
          ON1X_SDK_OK);
    CHECK(guarded.initialize_mutation_status == ON1X_SDK_BUSY);
    CHECK(on1x::sdk::count(
        guarded_registry.get(), on1x::sdk::Kind::Analyzer) == 1);
    CHECK(on1x_sdk_registry_close(guarded_registry.get()) == ON1X_SDK_OK);
    CHECK(guarded.shutdown_mutation_status == ON1X_SDK_BUSY);

    On1xSDK_RegistryTransactionReport partial_report;
    std::memset(&partial_report, 0xa5, sizeof partial_report);
    partial_report.abi_version = ON1X_SDK_ABI_VERSION_V2;
    partial_report.struct_size =
        offsetof(On1xSDK_RegistryTransactionReport, primary_status) + 1;
    CHECK(on1x_sdk_registry_register_batch_v2(
        rollback_registry.get(), nullptr, 0, &partial_report) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(reinterpret_cast<const unsigned char *>(&partial_report)
              [offsetof(
                  On1xSDK_RegistryTransactionReport,
                  primary_status)] == 0xa5);

    std::vector<std::string> denied_events;
    State denied{"denied", "entry.denied", &denied_events};
    const On1xSDK_CapabilityRequest unavailable{
        text("cap.unavailable"), 1, 0,
        ON1X_SDK_CAPABILITY_REQUIRED, 0
    };
    auto denied_extension = extension(
        denied, "1.0.0", "denied-key", nullptr, 0, &unavailable, 1
    );
    CHECK(on1x_sdk_registry_register_v2(
        rollback_registry.get(), &denied_extension, nullptr) ==
          ON1X_SDK_CAPABILITY_DENIED);
    CHECK(denied_events.empty());

    State cycle_a{"cycle.a", "entry.cycle.a", &denied_events};
    State cycle_b{"cycle.b", "entry.cycle.b", &denied_events};
    const On1xSDK_PluginDependency depends_a{
        text("cycle.a"), text("1.0.0"), {nullptr, 0}
    };
    const On1xSDK_PluginDependency depends_b{
        text("cycle.b"), text("1.0.0"), {nullptr, 0}
    };
    auto cycle_a_extension = extension(
        cycle_a, "1.0.0", "cycle-a", &depends_b, 1
    );
    auto cycle_b_extension = extension(
        cycle_b, "1.0.0", "cycle-b", &depends_a, 1
    );
    const On1xSDK_ExtensionV2 *cycle[]{
        &cycle_a_extension, &cycle_b_extension
    };
    CHECK(on1x_sdk_registry_register_batch_v2(
        rollback_registry.get(), cycle, 2, nullptr) ==
          ON1X_SDK_CONFLICT);
    CHECK(denied_events.empty());
    return 0;
}
