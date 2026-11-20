#include "On1xSDK-Analyzer.h"
#include "On1xSDK-Extension.h"
#include "On1xSDK-PluginDependency.h"
#include "On1xSDK-Registry.h"
#include "On1xSDK-Status.h"
#include "RegistryInternal.hpp"
#include "core/CoreInternal.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t v1_max_extensions = 1024;
constexpr std::size_t max_entries_per_kind = 1024;
constexpr std::size_t num_kinds =
    static_cast<std::size_t>(on1x::sdk::Kind::AnalyzerV2) + 1;
const On1xSDK_Host v1_host{};

struct Entry {
    std::string name;
    std::shared_ptr<const void> descriptor;
};

using CapabilityPtr = std::unique_ptr<
    On1xSDK_CapabilitySet, decltype(&on1x_sdk_capability_set_destroy)>;

struct Dependency {
    std::string name;
    std::string minimum;
    std::string maximum;
};

struct RequestedCapability {
    std::string name;
    std::uint32_t minimum = 0;
    std::uint32_t maximum = 0;
    std::uint32_t flags = 0;
};

struct ExtensionEntry {
    std::string name;
    std::string version;
    std::string registration_key;
    std::uint32_t abi_version = ON1X_SDK_ABI_VERSION_V1;
    On1xSDK_RegisterFn register_v1 = nullptr;
    On1xSDK_RegisterFnV2 register_v2 = nullptr;
    On1xSDK_InitializeFnV2 initialize = nullptr;
    On1xSDK_ShutdownFnV2 shutdown = nullptr;
    void *userdata = nullptr;
    CapabilityPtr capabilities{nullptr, on1x_sdk_capability_set_destroy};
    On1xSDK_LifecycleState state = ON1X_SDK_LIFECYCLE_NEW;
};

struct HostGrant {
    std::string name;
    std::uint32_t revision = 0;
};

struct SnapshotEntry {
    std::string name;
    std::string version;
    std::uint32_t abi_version = 0;
    std::uint32_t lifecycle_state = 0;
    std::size_t granted_capability_count = 0;
};

struct Candidate {
    std::string name;
    std::string version;
    std::string key;
    std::vector<Dependency> dependencies;
    std::vector<RequestedCapability> requests;
    On1xSDK_RegisterFnV2 register_all = nullptr;
    On1xSDK_InitializeFnV2 initialize = nullptr;
    On1xSDK_ShutdownFnV2 shutdown = nullptr;
    void *userdata = nullptr;
    CapabilityPtr capabilities{nullptr, on1x_sdk_capability_set_destroy};
    On1xSDK_LifecycleState state = ON1X_SDK_LIFECYCLE_NEW;
};

struct Version {
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;
};

On1xSDK_Status bounded_string(
    const char *text, std::string_view &out
) noexcept {
    if (!text) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    for (std::size_t n = 0; n <= ON1X_SDK_NAME_MAX; ++n) {
        if (text[n] == '\0') {
            out = {text, n};
            return ON1X_SDK_OK;
        }
    }
    return ON1X_SDK_LIMIT;
}

bool valid_v1_version(std::string_view version) noexcept {
    return !version.empty() &&
        std::all_of(version.begin(), version.end(), [](unsigned char value) {
            return value >= 0x20 && value <= 0x7e;
        });
}

bool letter(unsigned char value) noexcept {
    return (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z');
}

bool parse_component(
    std::string_view text, std::size_t &offset, std::uint32_t &out
) noexcept {
    if (offset >= text.size() || text[offset] < '0' || text[offset] > '9') {
        return false;
    }
    const auto begin = offset;
    std::uint32_t value = 0;
    do {
        const auto digit = static_cast<std::uint32_t>(text[offset] - '0');
        if (value > (UINT32_MAX - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
        ++offset;
    } while (offset < text.size() &&
             text[offset] >= '0' && text[offset] <= '9');
    if (offset - begin > 1 && text[begin] == '0') {
        return false;
    }
    out = value;
    return true;
}

bool parse_version(std::string_view text, Version &out) noexcept {
    if (text.empty() || text.size() > ON1X_SDK_NAME_MAX) {
        return false;
    }
    std::size_t offset = 0;
    Version value{};
    if (!parse_component(text, offset, value.major) ||
        offset >= text.size() || text[offset++] != '.' ||
        !parse_component(text, offset, value.minor) ||
        offset >= text.size() || text[offset++] != '.' ||
        !parse_component(text, offset, value.patch) ||
        offset != text.size()) {
        return false;
    }
    out = value;
    return true;
}

int compare_version(const Version &left, const Version &right) noexcept {
    if (left.major != right.major) {
        return left.major < right.major ? -1 : 1;
    }
    if (left.minor != right.minor) {
        return left.minor < right.minor ? -1 : 1;
    }
    if (left.patch != right.patch) {
        return left.patch < right.patch ? -1 : 1;
    }
    return 0;
}

bool version_matches(
    std::string_view provider, const Dependency &dependency
) noexcept {
    Version value{}, minimum{}, maximum{};
    if (!parse_version(provider, value) ||
        !parse_version(dependency.minimum, minimum) ||
        compare_version(value, minimum) < 0) {
        return false;
    }
    return dependency.maximum.empty() ||
        (parse_version(dependency.maximum, maximum) &&
         compare_version(value, maximum) <= 0);
}

On1xSDK_Status validate_dependency_view(
    const On1xSDK_PluginDependency &dependency
) noexcept {
    if (on1x_sdk_identifier_validate(dependency.name) != ON1X_SDK_OK ||
        !dependency.minimum_version.data ||
        !dependency.minimum_version.size ||
        dependency.minimum_version.size > ON1X_SDK_NAME_MAX ||
        (!dependency.maximum_version.data &&
         dependency.maximum_version.size)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    Version minimum{}, maximum{};
    const std::string_view min{
        dependency.minimum_version.data, dependency.minimum_version.size
    };
    if (!parse_version(min, minimum)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (dependency.maximum_version.size) {
        const std::string_view max{
            dependency.maximum_version.data, dependency.maximum_version.size
        };
        if (!parse_version(max, maximum) ||
            compare_version(minimum, maximum) > 0) {
            return ON1X_SDK_INVALID_ARGUMENT;
        }
    }
    return ON1X_SDK_OK;
}

On1xSDK_Status copy_candidate(
    const On1xSDK_ExtensionV2 *extension, Candidate &out
) {
    On1xSDK_ABIHeader header{};
    auto status = on1x_sdk_abi_header_validate(
        extension, ON1X_SDK_EXTENSION_V2_REQUIRED_SIZE,
        ON1X_SDK_ABI_VERSION_V2, &header
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    On1xSDK_ExtensionV2 value{};
    std::memcpy(&value, extension, ON1X_SDK_EXTENSION_V2_REQUIRED_SIZE);
    if (ON1X_SDK_STRUCT_HAS(
            extension, On1xSDK_ExtensionV2, initialize)) {
        value.initialize = extension->initialize;
    }
    if (ON1X_SDK_STRUCT_HAS(
            extension, On1xSDK_ExtensionV2, shutdown)) {
        value.shutdown = extension->shutdown;
    }
    if (ON1X_SDK_STRUCT_HAS(
            extension, On1xSDK_ExtensionV2, userdata)) {
        value.userdata = extension->userdata;
    }
    if (on1x_sdk_identifier_validate(value.name) != ON1X_SDK_OK ||
        on1x_sdk_identifier_validate(value.registration_key) != ON1X_SDK_OK ||
        !value.version.data || !value.version.size ||
        value.version.size > ON1X_SDK_NAME_MAX ||
        !value.register_all ||
        (value.initialize && !value.shutdown) ||
        (!value.dependencies && value.dependency_count) ||
        (!value.requested_capabilities &&
         value.requested_capability_count)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (value.dependency_count > ON1X_SDK_MAX_EXTENSION_DEPENDENCIES ||
        value.requested_capability_count > ON1X_SDK_MAX_CAPABILITIES) {
        return ON1X_SDK_LIMIT;
    }
    Version parsed{};
    const std::string_view version{value.version.data, value.version.size};
    if (!parse_version(version, parsed)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }

    out.name.assign(value.name.data, value.name.size);
    out.version.assign(value.version.data, value.version.size);
    out.key.assign(
        value.registration_key.data, value.registration_key.size
    );
    out.register_all = value.register_all;
    out.initialize = value.initialize;
    out.shutdown = value.shutdown;
    out.userdata = value.userdata;
    out.dependencies.reserve(value.dependency_count);
    out.requests.reserve(value.requested_capability_count);

    for (std::size_t i = 0; i < value.dependency_count; ++i) {
        const auto &dependency = value.dependencies[i];
        status = validate_dependency_view(dependency);
        if (status != ON1X_SDK_OK) {
            return status;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (on1x_sdk_string_equal(
                    dependency.name, value.dependencies[j].name)) {
                return ON1X_SDK_DUPLICATE;
            }
        }
        out.dependencies.push_back({
            std::string(dependency.name.data, dependency.name.size),
            std::string(
                dependency.minimum_version.data,
                dependency.minimum_version.size
            ),
            dependency.maximum_version.size
                ? std::string(
                      dependency.maximum_version.data,
                      dependency.maximum_version.size)
                : std::string{}
        });
    }
    for (std::size_t i = 0; i < value.requested_capability_count; ++i) {
        const auto &request = value.requested_capabilities[i];
        status = on1x_sdk_capability_request_validate(&request);
        if (status != ON1X_SDK_OK) {
            return status;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (on1x_sdk_string_equal(
                    request.name, value.requested_capabilities[j].name)) {
                return ON1X_SDK_DUPLICATE;
            }
        }
        out.requests.push_back({
            std::string(request.name.data, request.name.size),
            request.min_revision, request.max_revision, request.flags
        });
    }
    out.state = ON1X_SDK_LIFECYCLE_VALIDATED;
    return ON1X_SDK_OK;
}

template<class Callback, class... Arguments>
On1xSDK_Status invoke_v2(
    Callback callback, Arguments... arguments
) noexcept {
    if (!callback) {
        return ON1X_SDK_OK;
    }
    try {
        return on1x::sdk::v2::normalize_status(callback(arguments...));
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_CALLBACK_FAILED;
    }
}

void copy_report(
    On1xSDK_RegistryTransactionReport *destination,
    const On1xSDK_RegistryTransactionReport &source,
    std::uint32_t destination_size
) noexcept {
    if (!destination) {
        return;
    }
    destination->abi_version = source.abi_version;
    destination->struct_size = source.struct_size;
#define ON1X_SDK_COPY_REPORT_FIELD(field_) \
    do { \
        if (destination_size >= \
            offsetof(On1xSDK_RegistryTransactionReport, field_) + \
            sizeof(destination->field_)) { \
            destination->field_ = source.field_; \
        } \
    } while (false)
    ON1X_SDK_COPY_REPORT_FIELD(primary_status);
    ON1X_SDK_COPY_REPORT_FIELD(compensation_status);
    ON1X_SDK_COPY_REPORT_FIELD(validated);
    ON1X_SDK_COPY_REPORT_FIELD(initialized);
    ON1X_SDK_COPY_REPORT_FIELD(compensated);
#undef ON1X_SDK_COPY_REPORT_FIELD
}

} // namespace

struct On1xSDK_Registry {
    std::array<std::vector<Entry>, num_kinds> entries;
    std::vector<ExtensionEntry> extensions;
    std::vector<HostGrant> host_grants;
    On1xSDK_Context *context = nullptr;
    std::size_t extension_limit = v1_max_extensions;
    std::uint64_t revision = 1;
    std::thread::id owner_thread = std::this_thread::get_id();
    bool registering = false;
    bool accepting_entries = false;
    bool closed = false;
    const On1xSDK_CapabilitySet *registering_capabilities = nullptr;
};

struct On1xSDK_RegistrySnapshot {
    std::uint64_t revision = 0;
    std::vector<SnapshotEntry> extensions;
};

namespace {

bool on_owner_thread(const On1xSDK_Registry *registry) noexcept {
    return registry && registry->owner_thread == std::this_thread::get_id();
}

class Registration {
public:
    explicit Registration(On1xSDK_Registry &registry) noexcept
        : registry_(registry), extension_size_(registry.extensions.size()) {
        for (std::size_t i = 0; i < num_kinds; ++i) {
            old_sizes_[i] = registry_.entries[i].size();
        }
        registry_.accepting_entries = false;
        registry_.registering = true;
    }

    Registration(const Registration &) = delete;
    Registration &operator=(const Registration &) = delete;

    ~Registration() {
        if (!committed_) {
            for (std::size_t i = 0; i < num_kinds; ++i) {
                auto &entries = registry_.entries[i];
                while (entries.size() > old_sizes_[i]) {
                    entries.pop_back();
                }
            }
            while (registry_.extensions.size() > extension_size_) {
                registry_.extensions.pop_back();
            }
        }
        registry_.accepting_entries = false;
        registry_.registering = false;
    }

    void commit() noexcept {
        committed_ = true;
        ++registry_.revision;
    }

private:
    On1xSDK_Registry &registry_;
    std::array<std::size_t, num_kinds> old_sizes_{};
    std::size_t extension_size_ = 0;
    bool committed_ = false;
};

class EntryRegistration {
public:
    explicit EntryRegistration(On1xSDK_Registry &registry,
                               const On1xSDK_CapabilitySet *capabilities = nullptr) noexcept
        : registry_(registry), previous_(registry.accepting_entries),
          previous_capabilities_(registry.registering_capabilities) {
        registry_.accepting_entries = true;
        registry_.registering_capabilities = capabilities;
    }

    EntryRegistration(const EntryRegistration &) = delete;
    EntryRegistration &operator=(const EntryRegistration &) = delete;

    ~EntryRegistration() {
        registry_.accepting_entries = previous_;
        registry_.registering_capabilities = previous_capabilities_;
    }

private:
    On1xSDK_Registry &registry_;
    bool previous_ = false;
    const On1xSDK_CapabilitySet *previous_capabilities_ = nullptr;
};

std::vector<On1xSDK_CapabilityGrant> grant_views(
    const On1xSDK_Registry &registry
) {
    std::vector<On1xSDK_CapabilityGrant> views;
    views.reserve(registry.host_grants.size());
    for (const auto &grant : registry.host_grants) {
        views.push_back({
            {grant.name.data(), grant.name.size()}, grant.revision, 0
        });
    }
    return views;
}

std::vector<On1xSDK_CapabilityRequest> request_views(
    const Candidate &candidate
) {
    std::vector<On1xSDK_CapabilityRequest> views;
    views.reserve(candidate.requests.size());
    for (const auto &request : candidate.requests) {
        views.push_back({
            {request.name.data(), request.name.size()},
            request.minimum, request.maximum, request.flags, 0
        });
    }
    return views;
}

void compensate_candidates(
    On1xSDK_Registry &registry,
    std::vector<Candidate> &candidates,
    const std::vector<std::size_t> &attempts,
    On1xSDK_RegistryTransactionReport &result
) noexcept {
    for (auto iterator = attempts.rbegin();
         iterator != attempts.rend(); ++iterator) {
        auto &candidate = candidates[*iterator];
        candidate.state = ON1X_SDK_LIFECYCLE_STOPPING;
        On1xSDK_Context::HostToken token{};
        On1xSDK_Host host{};
        on1x::sdk::v2::make_host(
            registry.context, candidate.capabilities.get(), token, host
        );
        const auto status = invoke_v2(
            candidate.shutdown, &host, candidate.userdata
        );
        ++result.compensated;
        if (result.compensation_status == ON1X_SDK_OK &&
            status != ON1X_SDK_OK) {
            result.compensation_status = status;
        }
        candidate.state = status == ON1X_SDK_OK
            ? ON1X_SDK_LIFECYCLE_CLOSED
            : ON1X_SDK_LIFECYCLE_FAILED;
    }
}

On1xSDK_Status prepare_report(
    On1xSDK_RegistryTransactionReport *report,
    std::uint32_t &caller_size
) noexcept {
    caller_size = 0;
    if (!report) {
        return ON1X_SDK_OK;
    }
    On1xSDK_ABIHeader header{};
    const auto status = on1x_sdk_abi_header_validate(
        report, sizeof(On1xSDK_ABIHeader),
        ON1X_SDK_ABI_VERSION_V2, &header
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    caller_size = header.struct_size;
    return ON1X_SDK_OK;
}

const ExtensionEntry *find_extension(
    const On1xSDK_Registry &registry, std::string_view name
) noexcept {
    const auto found = std::find_if(
        registry.extensions.begin(), registry.extensions.end(),
        [&](const ExtensionEntry &entry) { return entry.name == name; }
    );
    return found == registry.extensions.end() ? nullptr : &*found;
}

On1xSDK_Status order_candidates(
    const On1xSDK_Registry &registry,
    const std::vector<Candidate> &candidates,
    std::vector<std::size_t> &order
) {
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto &candidate = candidates[i];
        if (find_extension(registry, candidate.name)) {
            return ON1X_SDK_DUPLICATE;
        }
        if (std::any_of(
                registry.extensions.begin(), registry.extensions.end(),
                [&](const ExtensionEntry &entry) {
                    return entry.registration_key == candidate.key;
                })) {
            return ON1X_SDK_DUPLICATE;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (candidate.name == candidates[j].name) {
                return ON1X_SDK_DUPLICATE;
            }
            if (candidate.key == candidates[j].key) {
                return ON1X_SDK_DUPLICATE;
            }
        }
        for (const auto &dependency : candidate.dependencies) {
            const Candidate *batch_provider = nullptr;
            for (const auto &provider : candidates) {
                if (provider.name == dependency.name) {
                    batch_provider = &provider;
                    break;
                }
            }
            if (batch_provider) {
                if (!version_matches(batch_provider->version, dependency)) {
                    return ON1X_SDK_UNSATISFIABLE;
                }
                continue;
            }
            const auto *provider =
                find_extension(registry, dependency.name);
            if (!provider ||
                !version_matches(provider->version, dependency)) {
                return ON1X_SDK_UNSATISFIABLE;
            }
        }
    }

    order.reserve(candidates.size());
    std::vector<bool> selected(candidates.size(), false);
    while (order.size() != candidates.size()) {
        std::size_t best = candidates.size();
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            if (selected[i]) {
                continue;
            }
            bool ready = true;
            for (const auto &dependency : candidates[i].dependencies) {
                for (std::size_t j = 0; j < candidates.size(); ++j) {
                    if (dependency.name == candidates[j].name &&
                        !selected[j]) {
                        ready = false;
                    }
                }
            }
            if (!ready) {
                continue;
            }
            if (best == candidates.size() ||
                candidates[i].key < candidates[best].key ||
                (candidates[i].key == candidates[best].key &&
                 candidates[i].name < candidates[best].name)) {
                best = i;
            }
        }
        if (best == candidates.size()) {
            return ON1X_SDK_CONFLICT;
        }
        selected[best] = true;
        order.push_back(best);
    }
    return ON1X_SDK_OK;
}

On1xSDK_Status make_context(On1xSDK_Context **out) {
    return on1x_sdk_context_create(nullptr, out);
}

On1xSDK_Status create_registry(
    On1xSDK_Context *context,
    std::size_t extension_limit,
    const On1xSDK_CapabilityGrant *grants,
    std::size_t grant_count,
    On1xSDK_Registry **out
) {
    if (!out || !context || (!grants && grant_count)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = nullptr;
    try {
        auto registry = std::make_unique<On1xSDK_Registry>();
        registry->extension_limit = extension_limit;
        registry->host_grants.reserve(grant_count);
        for (std::size_t i = 0; i < grant_count; ++i) {
            const auto status =
                on1x_sdk_capability_grant_validate(grants + i);
            if (status != ON1X_SDK_OK) {
                return status;
            }
            for (std::size_t j = 0; j < i; ++j) {
                if (on1x_sdk_string_equal(
                        grants[i].name, grants[j].name)) {
                    return ON1X_SDK_DUPLICATE;
                }
            }
            registry->host_grants.push_back({
                std::string(grants[i].name.data, grants[i].name.size),
                grants[i].revision
            });
        }
        registry->context = context;
        on1x_sdk_context_retain(context);
        *out = registry.release();
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

} // namespace

namespace on1x::sdk {

bool valid_name(std::string_view name) noexcept {
    if (name.empty() || name.size() > ON1X_SDK_NAME_MAX) {
        return false;
    }
    if (!letter(static_cast<unsigned char>(name.front())) &&
        name.front() != '_') {
        return false;
    }
    return std::all_of(
        name.begin() + 1, name.end(), [](unsigned char value) {
            return letter(value) || (value >= '0' && value <= '9') ||
                   value == '_' || value == '-' || value == '.';
        }
    );
}

On1xSDK_Status add(
    On1xSDK_Registry *registry, Kind kind, std::string_view name,
    std::shared_ptr<const void> descriptor
) {
    const auto index = static_cast<std::size_t>(kind);
    if (!registry || index >= num_kinds || !valid_name(name) ||
        !descriptor || !descriptor.use_count()) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!on_owner_thread(registry)) {
        return ON1X_SDK_BUSY;
    }
    if (registry->closed) {
        return ON1X_SDK_INVALID_STATE;
    }
    if (registry->registering && !registry->accepting_entries) {
        return ON1X_SDK_BUSY;
    }
    if (kind == Kind::AnalyzerV2 &&
        (!registry->registering_capabilities ||
         !on1x_sdk_capability_set_contains(
             registry->registering_capabilities,
             {ON1X_SDK_MIR_ANALYZE_CAPABILITY, sizeof(ON1X_SDK_MIR_ANALYZE_CAPABILITY) - 1},
             ON1X_SDK_MIR_ANALYZE_CAPABILITY_REVISION))) {
        return ON1X_SDK_CAPABILITY_DENIED;
    }
    if (find(registry, kind, name)) {
        return ON1X_SDK_DUPLICATE;
    }
    auto &entries = registry->entries[index];
    if (entries.size() >= max_entries_per_kind) {
        return ON1X_SDK_LIMIT;
    }
    entries.push_back({std::string{name}, std::move(descriptor)});
    if (!registry->registering) {
        ++registry->revision;
    }
    return ON1X_SDK_OK;
}

On1xSDK_Status check_active_registry_context(const On1xSDK_Registry *registry,
                                              const On1xSDK_Context *context) noexcept {
    if (!registry || !context) return ON1X_SDK_INVALID_ARGUMENT;
    if (!on_owner_thread(registry) || registry->registering) return ON1X_SDK_BUSY;
    if (registry->closed) return ON1X_SDK_INVALID_STATE;
    if (registry->context != context) return ON1X_SDK_STALE;
    return ON1X_SDK_OK;
}

const void *find(
    const On1xSDK_Registry *registry, Kind kind,
    std::string_view name
) noexcept {
    const auto index = static_cast<std::size_t>(kind);
    if (!registry || index >= num_kinds || !valid_name(name)) {
        return nullptr;
    }
    if (!on_owner_thread(registry)) {
        return nullptr;
    }
    const auto &entries = registry->entries[index];
    const auto found = std::find_if(
        entries.begin(), entries.end(),
        [name](const Entry &entry) { return entry.name == name; }
    );
    return found == entries.end() ? nullptr : found->descriptor.get();
}

const void *at(
    const On1xSDK_Registry *registry, Kind kind, std::size_t index
) noexcept {
    const auto kind_index = static_cast<std::size_t>(kind);
    if (!registry || kind_index >= num_kinds ||
        !on_owner_thread(registry) ||
        index >= registry->entries[kind_index].size()) {
        return nullptr;
    }
    return registry->entries[kind_index][index].descriptor.get();
}

std::size_t count(
    const On1xSDK_Registry *registry, Kind kind
) noexcept {
    const auto index = static_cast<std::size_t>(kind);
    return registry && index < num_kinds && on_owner_thread(registry)
        ? registry->entries[index].size() : 0;
}

} // namespace on1x::sdk

extern "C" On1xSDK_Status on1x_sdk_plugin_dependency_validate(
    const On1xSDK_PluginDependency *dependency
) {
    return dependency
        ? validate_dependency_view(*dependency)
        : ON1X_SDK_INVALID_ARGUMENT;
}

extern "C" int on1x_sdk_plugin_dependency_matches(
    const On1xSDK_PluginDependency *dependency,
    On1xSDK_StringView provider_name,
    On1xSDK_StringView provider_version
) {
    if (!dependency ||
        validate_dependency_view(*dependency) != ON1X_SDK_OK ||
        !on1x_sdk_string_equal(dependency->name, provider_name) ||
        (!provider_version.data && provider_version.size)) {
        return 0;
    }
    Version provider{}, minimum{}, maximum{};
    const std::string_view provider_text{
        provider_version.data, provider_version.size
    };
    const std::string_view minimum_text{
        dependency->minimum_version.data,
        dependency->minimum_version.size
    };
    if (!parse_version(provider_text, provider) ||
        !parse_version(minimum_text, minimum) ||
        compare_version(provider, minimum) < 0) {
        return 0;
    }
    if (dependency->maximum_version.size) {
        const std::string_view maximum_text{
            dependency->maximum_version.data,
            dependency->maximum_version.size
        };
        if (!parse_version(maximum_text, maximum) ||
            compare_version(provider, maximum) > 0) {
            return 0;
        }
    }
    return 1;
}

extern "C" On1xSDK_Status on1x_sdk_extension_v2_validate(
    const On1xSDK_ExtensionV2 *extension
) {
    try {
        Candidate copied;
        return copy_candidate(extension, copied);
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" On1xSDK_Status on1x_sdk_registry_create(
    On1xSDK_Registry **out
) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = nullptr;
    On1xSDK_Context *context = nullptr;
    auto status = make_context(&context);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    status = create_registry(
        context, v1_max_extensions, nullptr, 0, out
    );
    on1x_sdk_context_release(context);
    return status;
}

extern "C" On1xSDK_Status on1x_sdk_registry_create_v2(
    const On1xSDK_RegistryConfig *config, On1xSDK_Registry **out
) {
    if (out) {
        *out = nullptr;
    }
    if (!config || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    On1xSDK_ABIHeader header{};
    auto status = on1x_sdk_abi_header_validate(
        config, sizeof(On1xSDK_ABIHeader),
        ON1X_SDK_ABI_VERSION_V2, &header
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    On1xSDK_RegistryConfig value{};
    value.abi_version = header.abi_version;
    value.struct_size = header.struct_size;
#define ON1X_SDK_COPY_REGISTRY_CONFIG(field_) \
    do { \
        if (ON1X_SDK_STRUCT_HAS( \
                config, On1xSDK_RegistryConfig, field_)) { \
            value.field_ = config->field_; \
        } \
    } while (false)
    ON1X_SDK_COPY_REGISTRY_CONFIG(context);
    ON1X_SDK_COPY_REGISTRY_CONFIG(host_grants);
    ON1X_SDK_COPY_REGISTRY_CONFIG(host_grant_count);
    ON1X_SDK_COPY_REGISTRY_CONFIG(max_extensions);
    ON1X_SDK_COPY_REGISTRY_CONFIG(reserved);
#undef ON1X_SDK_COPY_REGISTRY_CONFIG
    if ((!value.host_grants && value.host_grant_count) ||
        value.host_grant_count > ON1X_SDK_MAX_CAPABILITIES ||
        value.reserved) {
        return value.host_grant_count > ON1X_SDK_MAX_CAPABILITIES
            ? ON1X_SDK_LIMIT : ON1X_SDK_INVALID_ARGUMENT;
    }
    const auto limit = value.max_extensions
        ? value.max_extensions : ON1X_SDK_MAX_EXTENSIONS_V2;
    if (limit > ON1X_SDK_MAX_EXTENSIONS_V2) {
        return ON1X_SDK_LIMIT;
    }
    On1xSDK_Context *context = value.context;
    bool created_context = false;
    if (!context) {
        status = make_context(&context);
        if (status != ON1X_SDK_OK) {
            return status;
        }
        created_context = true;
    }
    status = create_registry(
        context, limit, value.host_grants, value.host_grant_count, out
    );
    if (created_context) {
        on1x_sdk_context_release(context);
    }
    return status;
}

extern "C" void on1x_sdk_registry_destroy(
    On1xSDK_Registry *registry
) {
    if (!registry || registry->registering ||
        !on_owner_thread(registry)) {
        return;
    }
    (void)on1x_sdk_registry_close(registry);
    on1x_sdk_context_release(registry->context);
    delete registry;
}

extern "C" On1xSDK_Status on1x_sdk_registry_close(
    On1xSDK_Registry *registry
) {
    if (!registry) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!on_owner_thread(registry) || registry->registering) {
        return ON1X_SDK_BUSY;
    }
    if (registry->closed) {
        return ON1X_SDK_OK;
    }
    registry->accepting_entries = false;
    registry->registering = true;
    On1xSDK_Status first_failure = ON1X_SDK_OK;
    for (auto iterator = registry->extensions.rbegin();
         iterator != registry->extensions.rend(); ++iterator) {
        auto &entry = *iterator;
        if (entry.state != ON1X_SDK_LIFECYCLE_ACTIVE) {
            continue;
        }
        if (entry.abi_version != ON1X_SDK_ABI_VERSION_V2) {
            entry.state = ON1X_SDK_LIFECYCLE_CLOSED;
            continue;
        }
        entry.state = ON1X_SDK_LIFECYCLE_STOPPING;
        On1xSDK_Context::HostToken token{};
        On1xSDK_Host host{};
        on1x::sdk::v2::make_host(
            registry->context, entry.capabilities.get(), token, host
        );
        const auto status =
            invoke_v2(entry.shutdown, &host, entry.userdata);
        entry.state = status == ON1X_SDK_OK
            ? ON1X_SDK_LIFECYCLE_CLOSED
            : ON1X_SDK_LIFECYCLE_FAILED;
        if (first_failure == ON1X_SDK_OK && status != ON1X_SDK_OK) {
            first_failure = status;
        }
    }
    registry->registering = false;
    registry->closed = true;
    ++registry->revision;
    return first_failure;
}

extern "C" On1xSDK_Status on1x_sdk_registry_register(
    On1xSDK_Registry *registry, const On1xSDK_Extension *extension
) {
    return on1x_sdk_registry_register_v1(registry, extension);
}

extern "C" On1xSDK_Status on1x_sdk_registry_register_v1(
    On1xSDK_Registry *registry, const On1xSDK_Extension *extension
) {
    if (!registry || !extension) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!on_owner_thread(registry) || registry->registering) {
        return ON1X_SDK_BUSY;
    }
    if (registry->closed) {
        return ON1X_SDK_INVALID_STATE;
    }

    const auto *bytes =
        reinterpret_cast<const unsigned char *>(extension);
    std::uint32_t abi_version = 0, struct_size = 0;
    std::memcpy(
        &abi_version,
        bytes + offsetof(On1xSDK_Extension, abi_version),
        sizeof abi_version
    );
    std::memcpy(
        &struct_size,
        bytes + offsetof(On1xSDK_Extension, struct_size),
        sizeof struct_size
    );
    if (abi_version != ON1X_SDK_ABI_VERSION_V1 ||
        struct_size < sizeof(On1xSDK_Extension)) {
        return ON1X_SDK_ABI_MISMATCH;
    }
    On1xSDK_Extension descriptor{};
    std::memcpy(&descriptor, bytes, sizeof descriptor);
    if (!descriptor.register_all) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    std::string_view name, version;
    auto status = bounded_string(descriptor.name, name);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (!on1x::sdk::valid_name(name)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    status = bounded_string(descriptor.version, version);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (!valid_v1_version(version)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }

    try {
        if (find_extension(*registry, name)) {
            return ON1X_SDK_DUPLICATE;
        }
        if (std::any_of(
                registry->extensions.begin(), registry->extensions.end(),
                [&](const ExtensionEntry &entry) {
                    return entry.registration_key == name;
                })) {
            return ON1X_SDK_DUPLICATE;
        }
        if (registry->extensions.size() >= registry->extension_limit) {
            return ON1X_SDK_LIMIT;
        }
        registry->extensions.reserve(registry->extensions.size() + 1);
        ExtensionEntry copied;
        copied.name.assign(name);
        copied.version.assign(version);
        copied.registration_key = copied.name;
        copied.register_v1 = descriptor.register_all;
        copied.state = ON1X_SDK_LIFECYCLE_INITIALIZING;

        Registration transaction(*registry);
        try {
            EntryRegistration registration(*registry);
            status = descriptor.register_all(&v1_host, registry);
            if (!on1x_sdk_status_is_v1(status)) {
                status = ON1X_SDK_ERROR;
            }
        } catch (const std::bad_alloc &) {
            status = ON1X_SDK_LIMIT;
        } catch (...) {
            status = ON1X_SDK_ERROR;
        }
        if (status != ON1X_SDK_OK) {
            return status;
        }
        copied.state = ON1X_SDK_LIFECYCLE_ACTIVE;
        registry->extensions.push_back(std::move(copied));
        transaction.commit();
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" On1xSDK_Status on1x_sdk_registry_register_v2(
    On1xSDK_Registry *registry, const On1xSDK_ExtensionV2 *extension,
    On1xSDK_RegistryTransactionReport *report
) {
    const On1xSDK_ExtensionV2 *extensions[] = {extension};
    return on1x_sdk_registry_register_batch_v2(
        registry, extension ? extensions : nullptr,
        extension ? 1 : 0, report
    );
}

extern "C" On1xSDK_Status on1x_sdk_registry_register_batch_v2(
    On1xSDK_Registry *registry,
    const On1xSDK_ExtensionV2 *const *extensions,
    std::size_t extension_count,
    On1xSDK_RegistryTransactionReport *report
) {
    std::uint32_t report_size = 0;
    auto status = prepare_report(report, report_size);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    On1xSDK_RegistryTransactionReport result{
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_RegistryTransactionReport),
        ON1X_SDK_OK,
        ON1X_SDK_OK,
        0, 0, 0
    };
    const auto finish = [&](On1xSDK_Status value) {
        result.primary_status = value;
        copy_report(report, result, report_size);
        return value;
    };
    if (!registry || (!extensions && extension_count) ||
        !extension_count) {
        return finish(ON1X_SDK_INVALID_ARGUMENT);
    }
    if (!on_owner_thread(registry) || registry->registering) {
        return finish(ON1X_SDK_BUSY);
    }
    if (registry->closed) {
        return finish(ON1X_SDK_INVALID_STATE);
    }
    if (extension_count >
            registry->extension_limit - std::min(
                registry->extension_limit, registry->extensions.size())) {
        return finish(ON1X_SDK_LIMIT);
    }

    try {
        std::vector<Candidate> candidates(extension_count);
        for (std::size_t i = 0; i < extension_count; ++i) {
            if (!extensions[i]) {
                return finish(ON1X_SDK_INVALID_ARGUMENT);
            }
            status = copy_candidate(extensions[i], candidates[i]);
            if (status != ON1X_SDK_OK) {
                return finish(status);
            }
            ++result.validated;
        }
        std::vector<std::size_t> order;
        status = order_candidates(*registry, candidates, order);
        if (status != ON1X_SDK_OK) {
            return finish(status);
        }

        const auto grants = grant_views(*registry);
        for (auto &candidate : candidates) {
            const auto requests = request_views(candidate);
            On1xSDK_CapabilitySet *set = nullptr;
            status = on1x_sdk_capabilities_negotiate(
                requests.data(), requests.size(),
                grants.data(), grants.size(), &set
            );
            if (status != ON1X_SDK_OK) {
                return finish(status);
            }
            candidate.capabilities.reset(set);
        }

        registry->extensions.reserve(
            registry->extensions.size() + extension_count
        );
        std::vector<std::size_t> registration_attempts;
        registration_attempts.reserve(extension_count);
        Registration transaction(*registry);
        for (const auto index : order) {
            auto &candidate = candidates[index];
            registration_attempts.push_back(index);
            candidate.state = ON1X_SDK_LIFECYCLE_INITIALIZING;
            On1xSDK_Context::HostToken token{};
            On1xSDK_Host host{};
            on1x::sdk::v2::make_host(
                registry->context, candidate.capabilities.get(),
                token, host
            );
            {
                EntryRegistration registration(*registry, candidate.capabilities.get());
                status = invoke_v2(
                    candidate.register_all, &host, registry,
                    candidate.userdata
                );
            }
            if (status != ON1X_SDK_OK) {
                compensate_candidates(
                    *registry, candidates, registration_attempts, result
                );
                return finish(status);
            }
        }

        for (const auto index : order) {
            auto &candidate = candidates[index];
            On1xSDK_Context::HostToken token{};
            On1xSDK_Host host{};
            on1x::sdk::v2::make_host(
                registry->context, candidate.capabilities.get(),
                token, host
            );
            status = invoke_v2(
                candidate.initialize, &host, candidate.userdata
            );
            if (status != ON1X_SDK_OK) {
                compensate_candidates(
                    *registry, candidates, registration_attempts, result
                );
                return finish(status);
            }
            candidate.state = ON1X_SDK_LIFECYCLE_ACTIVE;
            ++result.initialized;
        }

        for (const auto index : order) {
            auto &candidate = candidates[index];
            ExtensionEntry entry;
            entry.name = std::move(candidate.name);
            entry.version = std::move(candidate.version);
            entry.registration_key = std::move(candidate.key);
            entry.abi_version = ON1X_SDK_ABI_VERSION_V2;
            entry.register_v2 = candidate.register_all;
            entry.initialize = candidate.initialize;
            entry.shutdown = candidate.shutdown;
            entry.userdata = candidate.userdata;
            entry.capabilities = std::move(candidate.capabilities);
            entry.state = candidate.state;
            registry->extensions.push_back(std::move(entry));
        }
        transaction.commit();
        return finish(ON1X_SDK_OK);
    } catch (const std::bad_alloc &) {
        return finish(ON1X_SDK_LIMIT);
    } catch (...) {
        return finish(ON1X_SDK_ERROR);
    }
}

extern "C" std::uint64_t on1x_sdk_registry_revision(
    const On1xSDK_Registry *registry
) {
    return registry && on_owner_thread(registry)
        ? registry->revision : 0;
}

extern "C" On1xSDK_Status on1x_sdk_registry_snapshot(
    const On1xSDK_Registry *registry,
    On1xSDK_RegistrySnapshot **out
) {
    if (out) {
        *out = nullptr;
    }
    if (!registry || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!on_owner_thread(registry) || registry->registering) {
        return ON1X_SDK_BUSY;
    }
    try {
        auto snapshot = std::make_unique<On1xSDK_RegistrySnapshot>();
        snapshot->revision = registry->revision;
        snapshot->extensions.reserve(registry->extensions.size());
        for (const auto &entry : registry->extensions) {
            snapshot->extensions.push_back({
                entry.name, entry.version, entry.abi_version,
                static_cast<std::uint32_t>(entry.state),
                on1x_sdk_capability_set_count(entry.capabilities.get())
            });
        }
        *out = snapshot.release();
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_registry_snapshot_destroy(
    On1xSDK_RegistrySnapshot *snapshot
) {
    delete snapshot;
}

extern "C" std::uint64_t on1x_sdk_registry_snapshot_revision(
    const On1xSDK_RegistrySnapshot *snapshot
) {
    return snapshot ? snapshot->revision : 0;
}

extern "C" On1xSDK_Status on1x_sdk_registry_snapshot_count(
    const On1xSDK_RegistrySnapshot *snapshot, std::size_t *out
) {
    if (!snapshot || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = snapshot->extensions.size();
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_registry_snapshot_at(
    const On1xSDK_RegistrySnapshot *snapshot, std::size_t index,
    On1xSDK_RegistryExtensionView *out
) {
    if (out) {
        *out = {};
    }
    if (!snapshot || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (index >= snapshot->extensions.size()) {
        return ON1X_SDK_NOT_FOUND;
    }
    const auto &entry = snapshot->extensions[index];
    *out = {
        {entry.name.data(), entry.name.size()},
        {entry.version.data(), entry.version.size()},
        entry.abi_version, entry.lifecycle_state,
        entry.granted_capability_count
    };
    return ON1X_SDK_OK;
}
