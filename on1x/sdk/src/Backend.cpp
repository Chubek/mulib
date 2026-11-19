#include "On1xSDK-Backend.h"
#include "FacetInternal.hpp"
#include "backend/ArtifactInternal.hpp"
#include "backend/BackendProviderInternal.hpp"
#include "backend/TargetInternal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using on1x::sdk::Kind;

enum class Flavor { Legacy, Provider };

struct StoredCapability {
    On1xSDK_Phase phase{};
    std::uint32_t schema_min{};
    std::uint32_t schema_max{};
    On1xSDK_ArtifactKind artifact_kind{};
    std::uint32_t flags{};
    std::vector<std::string> targets;
    std::vector<std::string> required_features;
    std::vector<std::string> forbidden_features;
};

struct StoredBackend {
    Flavor flavor{Flavor::Legacy};
    std::string name;
    On1xSDK_Phase legacy_phase{};
    On1xSDK_BackendFn legacy_run{};
    On1xSDK_BackendProviderFn provider_run{};
    void *userdata{};
    std::int32_t priority{};
    bool trusted{};
    std::vector<StoredCapability> capabilities;
};

struct Selection {
    std::string name;
    On1xSDK_BackendProviderFn run{};
    void *userdata{};
    std::int32_t priority{};
    bool trusted{};
    std::size_t capability_index{};
    StoredCapability capability;
};

struct ActiveInvocation {
    const On1xSDK_Registry *registry;
    std::string_view provider;
};

thread_local std::vector<ActiveInvocation> active_invocations;

constexpr std::uint32_t provider_flags =
    ON1X_SDK_BACKEND_TARGET_INDEPENDENT | ON1X_SDK_BACKEND_DETERMINISTIC;
constexpr std::uint32_t request_flags =
    ON1X_SDK_BACKEND_REQUIRE_DETERMINISTIC | ON1X_SDK_BACKEND_CHECK_REPRODUCIBILITY;
/* ABI 2 reserves consecutive values 8..16 for the nine appended statuses
 * specified by ABI.md. Core supplies their public symbolic names. */
constexpr std::uint32_t max_abi2_status = 16u;

bool native_artifact(On1xSDK_ArtifactKind kind) noexcept {
    return kind == ON1X_SDK_ARTIFACT_ASSEMBLY || kind == ON1X_SDK_ARTIFACT_OBJECT ||
           kind == ON1X_SDK_ARTIFACT_EXECUTABLE;
}

bool valid_artifact(On1xSDK_ArtifactKind kind) noexcept {
    return kind >= ON1X_SDK_ARTIFACT_C11_SOURCE &&
           kind <= ON1X_SDK_ARTIFACT_EXECUTABLE;
}

On1xSDK_Status normalize_status(On1xSDK_Status status) noexcept {
    const auto raw = on1x::sdk::enum_value(status);
    if (raw <= max_abi2_status) return static_cast<On1xSDK_Status>(raw);
    return ON1X_SDK_ERROR;
}

On1xSDK_Status normalize_v1_status(On1xSDK_Status status) noexcept {
    const auto raw = on1x::sdk::enum_value(status);
    if (raw <= ON1X_SDK_BUSY) return static_cast<On1xSDK_Status>(raw);
    return ON1X_SDK_ERROR;
}

template <class Function>
On1xSDK_Status guarded(Function &&function) noexcept {
    try {
        return function();
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

bool bytes_view(const char *data, std::size_t size, std::size_t limit,
                bool optional, std::string_view &out) noexcept {
    out = {};
    if (size > limit || (size && !data) || (!size && !optional)) return false;
    if (!size) return true;
    out = {data, size};
    return std::none_of(out.begin(), out.end(), [](char c) { return c == '\0'; });
}

On1xSDK_Status descriptor_header(const void *descriptor, std::uint32_t expected,
                                  std::size_t required, std::uint32_t &size) noexcept {
    if (!descriptor) return ON1X_SDK_INVALID_ARGUMENT;
    std::uint32_t abi{};
    const auto *bytes = static_cast<const unsigned char *>(descriptor);
    std::memcpy(&abi, bytes, sizeof abi);
    std::memcpy(&size, bytes + sizeof abi, sizeof size);
    return abi == expected && size >= required ? ON1X_SDK_OK : ON1X_SDK_ABI_MISMATCH;
}

bool field_available(std::uint32_t size, std::size_t offset,
                     std::size_t width) noexcept {
    return size >= offset && static_cast<std::size_t>(size) - offset >= width;
}

bool invocation_active(const On1xSDK_Registry *registry,
                       std::string_view name) noexcept {
    for (const auto &active : active_invocations)
        if (active.registry == registry && active.provider == name) return true;
    return false;
}

class ActiveScope {
public:
    ActiveScope(const On1xSDK_Registry *registry, std::string_view name)
        : active_(false) {
        active_invocations.push_back({registry, name});
        active_ = true;
    }

    ActiveScope(const ActiveScope &) = delete;
    ActiveScope &operator=(const ActiveScope &) = delete;

    ~ActiveScope() {
        if (active_) active_invocations.pop_back();
    }

private:
    bool active_;
};

On1xSDK_Status validate_feature_list(const On1xSDK_TargetFeature *features,
                                     std::size_t count,
                                     std::vector<std::string> &out) {
    if (count > ON1X_SDK_MAX_BACKEND_FEATURES) return ON1X_SDK_LIMIT;
    if (count && !features) return ON1X_SDK_INVALID_ARGUMENT;
    out.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        std::string_view value;
        if (!bytes_view(features[index].data, features[index].size,
                        ON1X_SDK_MAX_TARGET_FEATURE_BYTES, false, value) ||
            !on1x::sdk::target::valid_feature(value))
            return ON1X_SDK_INVALID_ARGUMENT;
        out.emplace_back(value);
    }
    std::sort(out.begin(), out.end());
    if (std::adjacent_find(out.begin(), out.end()) != out.end())
        return ON1X_SDK_INVALID_ARGUMENT;
    return ON1X_SDK_OK;
}

On1xSDK_Status copy_capability(const On1xSDK_BackendCapability &input, bool trusted,
                               StoredCapability &out) {
    std::uint32_t size{};
    if (const auto status =
            descriptor_header(&input, ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
                              offsetof(On1xSDK_BackendCapability, targets), size);
        status != ON1X_SDK_OK)
        return status;
    On1xSDK_BackendCapability capability{};
    std::memcpy(&capability, &input,
                std::min<std::size_t>(size, sizeof capability));
    if (!field_available(size, offsetof(On1xSDK_BackendCapability, target_count),
                         sizeof(capability.target_count))) {
        capability.targets = nullptr;
        capability.target_count = 0;
    }
    if (!field_available(
            size, offsetof(On1xSDK_BackendCapability, required_feature_count),
            sizeof(capability.required_feature_count))) {
        capability.required_features = nullptr;
        capability.required_feature_count = 0;
    }
    if (!field_available(
            size, offsetof(On1xSDK_BackendCapability, forbidden_feature_count),
            sizeof(capability.forbidden_feature_count))) {
        capability.forbidden_features = nullptr;
        capability.forbidden_feature_count = 0;
    }
    const auto phase = on1x::sdk::enum_value(capability.input_phase);
    if (phase != ON1X_SDK_VERIFIED_MIR &&
        !(trusted && phase == ON1X_SDK_CHECKED_HIR))
        return ON1X_SDK_UNSUPPORTED;
    if (!valid_artifact(capability.artifact_kind))
        return ON1X_SDK_INVALID_ARGUMENT;
    if (native_artifact(capability.artifact_kind))
        return ON1X_SDK_UNSUPPORTED;
    if (capability.flags & ~provider_flags)
        return ON1X_SDK_INVALID_ARGUMENT;
    if (phase == ON1X_SDK_VERIFIED_MIR) {
        const auto minimum = trusted ? 0u : 1u;
        if (capability.input_schema_min < minimum ||
            capability.input_schema_max < capability.input_schema_min ||
            capability.input_schema_max > 65535u)
            return ON1X_SDK_INVALID_ARGUMENT;
    } else if (capability.input_schema_min || capability.input_schema_max) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }

    const bool independent =
        (capability.flags & ON1X_SDK_BACKEND_TARGET_INDEPENDENT) != 0;
    if (independent) {
        if (capability.target_count || capability.targets ||
            capability.required_feature_count || capability.required_features ||
            capability.forbidden_feature_count || capability.forbidden_features)
            return ON1X_SDK_INVALID_ARGUMENT;
    } else {
        if (!capability.target_count || !capability.targets)
            return ON1X_SDK_INVALID_ARGUMENT;
        if (capability.target_count > ON1X_SDK_MAX_BACKEND_TARGETS)
            return ON1X_SDK_LIMIT;
        out.targets.reserve(capability.target_count);
        for (std::size_t index = 0; index < capability.target_count; ++index) {
            std::string_view target;
            if (!bytes_view(capability.targets[index].triple,
                            capability.targets[index].triple_size,
                            ON1X_SDK_MAX_TARGET_BYTES, false, target) ||
                !on1x::sdk::target::valid_triple(target))
                return ON1X_SDK_INVALID_ARGUMENT;
            out.targets.emplace_back(target);
        }
        std::sort(out.targets.begin(), out.targets.end());
        if (std::adjacent_find(out.targets.begin(), out.targets.end()) != out.targets.end())
            return ON1X_SDK_INVALID_ARGUMENT;
        if (const auto status =
                validate_feature_list(capability.required_features,
                                      capability.required_feature_count,
                                      out.required_features);
            status != ON1X_SDK_OK)
            return status;
        if (const auto status =
                validate_feature_list(capability.forbidden_features,
                                      capability.forbidden_feature_count,
                                      out.forbidden_features);
            status != ON1X_SDK_OK)
            return status;
        for (const auto &feature : out.required_features)
            if (std::binary_search(out.forbidden_features.begin(),
                                   out.forbidden_features.end(), feature))
                return ON1X_SDK_INVALID_ARGUMENT;
    }

    out.phase = static_cast<On1xSDK_Phase>(phase);
    out.schema_min = capability.input_schema_min;
    out.schema_max = capability.input_schema_max;
    out.artifact_kind = capability.artifact_kind;
    out.flags = capability.flags;
    return ON1X_SDK_OK;
}

On1xSDK_Status register_provider(On1xSDK_Registry *registry,
                                const On1xSDK_BackendProvider *provider,
                                bool trusted) noexcept {
    if (!registry || !provider) return ON1X_SDK_INVALID_ARGUMENT;
    std::uint32_t size{};
    if (const auto status =
            descriptor_header(provider, ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
                              offsetof(On1xSDK_BackendProvider, userdata), size);
        status != ON1X_SDK_OK)
        return status;
    On1xSDK_BackendProvider input{};
    std::memcpy(&input, provider,
                std::min<std::size_t>(size, sizeof input));
    if (!field_available(size, offsetof(On1xSDK_BackendProvider, userdata),
                         sizeof(input.userdata)))
        input.userdata = nullptr;
    std::string_view name;
    if (!bytes_view(input.name, input.name_size, ON1X_SDK_NAME_MAX, false, name) ||
        !on1x::sdk::valid_name(name) || input.reserved || !input.run)
        return ON1X_SDK_INVALID_ARGUMENT;
    if (!input.capability_count || !input.capabilities)
        return ON1X_SDK_INVALID_ARGUMENT;
    if (input.capability_count > ON1X_SDK_MAX_BACKEND_CAPABILITIES)
        return ON1X_SDK_LIMIT;

    return guarded([&] {
        auto stored = std::make_shared<StoredBackend>();
        stored->flavor = Flavor::Provider;
        stored->name.assign(name);
        stored->provider_run = input.run;
        stored->userdata = input.userdata;
        stored->priority = input.priority;
        stored->trusted = trusted;
        stored->capabilities.resize(input.capability_count);
        for (std::size_t index = 0; index < input.capability_count; ++index)
            if (const auto status =
                    copy_capability(input.capabilities[index], trusted,
                                    stored->capabilities[index]);
                status != ON1X_SDK_OK)
                return status;
        return on1x::sdk::add(registry, Kind::Backend, name, std::move(stored));
    });
}

On1xSDK_Status register_legacy(On1xSDK_Registry *registry,
                              const On1xSDK_Backend *backend,
                              bool checked_hir) noexcept {
    if (!registry || !backend) return ON1X_SDK_INVALID_ARGUMENT;
    std::uint32_t size{};
    if (const auto status =
            descriptor_header(backend, ON1X_SDK_ABI_VERSION,
                              sizeof(On1xSDK_Backend), size);
        status != ON1X_SDK_OK)
        return status;
    On1xSDK_Backend input{};
    std::memcpy(&input, backend, sizeof input);
    std::string_view name;
    if (const auto status = on1x::sdk::read_name(input.name, name);
        status != ON1X_SDK_OK)
        return status;
    if (!input.run) return ON1X_SDK_INVALID_ARGUMENT;
    const auto phase = on1x::sdk::enum_value(input.phase);
    if (phase != (checked_hir ? ON1X_SDK_CHECKED_HIR : ON1X_SDK_VERIFIED_MIR))
        return ON1X_SDK_UNSUPPORTED;
    return guarded([&] {
        auto stored = std::make_shared<StoredBackend>();
        stored->flavor = Flavor::Legacy;
        stored->name.assign(name);
        stored->legacy_phase = static_cast<On1xSDK_Phase>(phase);
        stored->legacy_run = input.run;
        stored->userdata = input.userdata;
        stored->trusted = checked_hir;
        return on1x::sdk::add(registry, Kind::Backend, name, std::move(stored));
    });
}

const StoredBackend *lookup(const On1xSDK_Registry *registry,
                            std::string_view name) noexcept {
    return static_cast<const StoredBackend *>(
        on1x::sdk::find(registry, Kind::Backend, name));
}

On1xSDK_Status derive_schema(const On1xSDK_Program *program,
                            std::uint32_t requested,
                            std::uint32_t &schema) noexcept {
    schema = 0;
    if (!program) return ON1X_SDK_INVALID_ARGUMENT;
    const auto phase = on1x::sdk::enum_value(program->phase);
    if (phase == ON1X_SDK_CHECKED_HIR)
        return requested ? ON1X_SDK_INVALID_ARGUMENT : ON1X_SDK_OK;
    if (phase != ON1X_SDK_VERIFIED_MIR) return ON1X_SDK_UNSUPPORTED;
    if (program->verified_module) {
        schema = requested;
        return ON1X_SDK_OK;
    }
    constexpr std::string_view prefix = "(on1x.mir ";
    if (program->bytes.size() <= prefix.size() ||
        std::string_view(reinterpret_cast<const char *>(program->bytes.data()),
                         prefix.size()) != prefix)
        return ON1X_SDK_INVALID_ARGUMENT;
    std::size_t cursor = prefix.size();
    if (program->bytes[cursor] < '1' || program->bytes[cursor] > '9')
        return ON1X_SDK_INVALID_ARGUMENT;
    std::uint32_t parsed = 0;
    while (cursor < program->bytes.size() &&
           program->bytes[cursor] >= '0' && program->bytes[cursor] <= '9') {
        const auto digit = static_cast<std::uint32_t>(program->bytes[cursor] - '0');
        if (parsed > (65535u - digit) / 10u) return ON1X_SDK_INVALID_ARGUMENT;
        parsed = parsed * 10u + digit;
        ++cursor;
    }
    if (cursor >= program->bytes.size() || program->bytes[cursor] != ' ')
        return ON1X_SDK_INVALID_ARGUMENT;
    if (requested && requested != parsed) return ON1X_SDK_INVALID_ARGUMENT;
    schema = parsed;
    return ON1X_SDK_OK;
}

bool capability_matches(const StoredCapability &capability,
                        On1xSDK_Phase phase, std::uint32_t schema,
                        On1xSDK_ArtifactKind artifact_kind,
                        const On1xSDK_Target *target,
                        std::uint32_t flags) noexcept {
    if (capability.phase != phase || capability.artifact_kind != artifact_kind)
        return false;
    if (phase == ON1X_SDK_VERIFIED_MIR &&
        (schema < capability.schema_min || schema > capability.schema_max))
        return false;
    if ((flags & ON1X_SDK_BACKEND_REQUIRE_DETERMINISTIC) &&
        !(capability.flags & ON1X_SDK_BACKEND_DETERMINISTIC))
        return false;
    if (capability.flags & ON1X_SDK_BACKEND_TARGET_INDEPENDENT) return true;
    if (!target ||
        !std::binary_search(capability.targets.begin(), capability.targets.end(),
                            target->triple))
        return false;
    for (const auto &feature : capability.required_features)
        if (!on1x::sdk::target::has_feature(*target, feature)) return false;
    for (const auto &feature : capability.forbidden_features)
        if (on1x::sdk::target::has_feature(*target, feature)) return false;
    return true;
}

On1xSDK_Status select_provider(const On1xSDK_Registry *registry,
                               std::string_view requested_name,
                               On1xSDK_Phase phase, std::uint32_t schema,
                               On1xSDK_ArtifactKind artifact_kind,
                               const On1xSDK_Target *target,
                               std::uint32_t flags,
                               Selection &selection) {
    bool found = false;
    bool ambiguous = false;
    int best_specificity = -1;
    std::int32_t best_priority = std::numeric_limits<std::int32_t>::min();
    const auto entries = on1x::sdk::count(registry, Kind::Backend);
    for (std::size_t entry = 0; entry < entries; ++entry) {
        const auto *backend = static_cast<const StoredBackend *>(
            on1x::sdk::at(registry, Kind::Backend, entry));
        if (!backend || backend->flavor != Flavor::Provider ||
            (!requested_name.empty() && backend->name != requested_name))
            continue;
        for (std::size_t index = 0; index < backend->capabilities.size(); ++index) {
            const auto &capability = backend->capabilities[index];
            if (!capability_matches(capability, phase, schema, artifact_kind, target, flags))
                continue;
            const int specificity =
                (capability.flags & ON1X_SDK_BACKEND_TARGET_INDEPENDENT) ? 0 : 1;
            const bool better = !found || backend->priority > best_priority ||
                                (backend->priority == best_priority &&
                                 specificity > best_specificity);
            const bool tied = found && backend->priority == best_priority &&
                              specificity == best_specificity;
            if (better) {
                selection = {backend->name, backend->provider_run, backend->userdata,
                             backend->priority, backend->trusted, index, capability};
                best_priority = backend->priority;
                best_specificity = specificity;
                found = true;
                ambiguous = false;
            } else if (tied) {
                ambiguous = true;
            }
        }
    }
    if (!found) return ON1X_SDK_UNSUPPORTED;
    return ambiguous ? ON1X_SDK_DUPLICATE : ON1X_SDK_OK;
}

On1xSDK_Status poll(const On1xSDK_BackendInvocation &invocation) noexcept {
    if (!invocation.poll) return ON1X_SDK_OK;
    return guarded([&] {
        return normalize_status(invocation.poll(invocation.poll_userdata));
    });
}

On1xSDK_Status invoke_provider(const Selection &selection,
                               const On1xSDK_BackendInvocation &invocation,
                               std::vector<std::uint8_t> &bytes) noexcept {
    if (const auto status = poll(invocation); status != ON1X_SDK_OK) return status;
    On1xSDK_OutputSink sink{{}, invocation.byte_limit};
    const auto status = guarded([&] {
        return normalize_status(selection.run(selection.userdata, &invocation, &sink));
    });
    if (sink.status != ON1X_SDK_OK) return sink.status;
    if (status != ON1X_SDK_OK) return status;
    if (const auto after = poll(invocation); after != ON1X_SDK_OK) return after;
    bytes = std::move(sink.bytes);
    return ON1X_SDK_OK;
}

On1xSDK_Status invoke_legacy(const StoredBackend &backend,
                             const On1xSDK_Program *program,
                             std::size_t byte_limit,
                             std::vector<std::uint8_t> &bytes) noexcept {
    if (backend.legacy_phase != program->phase) return ON1X_SDK_UNSUPPORTED;
    On1xSDK_OutputSink sink{{}, byte_limit};
    const auto status = guarded([&] {
        return normalize_v1_status(
            backend.legacy_run(backend.userdata, program, &sink));
    });
    if (sink.status != ON1X_SDK_OK) return sink.status;
    if (status != ON1X_SDK_OK) return status;
    bytes = std::move(sink.bytes);
    return ON1X_SDK_OK;
}

On1xSDK_Status run_named(const On1xSDK_Registry *registry, const char *name,
                         const On1xSDK_Program *program, std::size_t byte_limit,
                         bool private_input, On1xSDK_Artifact **out) noexcept {
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = nullptr;
    if (!registry || !program) return ON1X_SDK_INVALID_ARGUMENT;
    std::string_view key;
    if (const auto status = on1x::sdk::read_name(name, key); status != ON1X_SDK_OK)
        return status;
    const auto *stored = lookup(registry, key);
    if (!stored) return ON1X_SDK_UNSUPPORTED;
    if (byte_limit > (private_input ? 256u * 1024u * 1024u
                                    : ON1X_SDK_MAX_ARTIFACT_BYTES))
        return ON1X_SDK_LIMIT;

    return guarded([&] {
        /* Copy callback-bearing state before invocation so callback-scoped
         * owner destruction cannot invalidate host stack data. */
        StoredBackend backend = *stored;
        std::vector<std::uint8_t> bytes;
        On1xSDK_ArtifactKind kind = ON1X_SDK_ARTIFACT_UNKNOWN;
        std::uint32_t schema = 0;
        std::uint32_t artifact_flags = 0;
        On1xSDK_Status status;
        if (invocation_active(registry, backend.name))
            return ON1X_SDK_BUSY;
        ActiveScope active(registry, backend.name);
        if (backend.flavor == Flavor::Legacy) {
            status = invoke_legacy(backend, program, byte_limit, bytes);
        } else {
            status = derive_schema(program, 0, schema);
            /*
             * The ABI-v1 compatibility entry point historically forwards
             * malformed VERIFIED_MIR bytes to the selected callback, which owns
             * its decoder. Keep that behavior while SDK 2 produce rejects a
             * malformed envelope before provider invocation.
             */
            if (status == ON1X_SDK_INVALID_ARGUMENT &&
                program->phase == ON1X_SDK_VERIFIED_MIR)
                schema = 0;
            else if (status != ON1X_SDK_OK)
                return status;
            const StoredCapability *capability = nullptr;
            std::size_t index = 0;
            for (; index < backend.capabilities.size(); ++index) {
                const auto &candidate = backend.capabilities[index];
                if (candidate.phase == program->phase &&
                    (candidate.flags & ON1X_SDK_BACKEND_TARGET_INDEPENDENT) &&
                    (program->phase != ON1X_SDK_VERIFIED_MIR ||
                     (schema >= candidate.schema_min &&
                      schema <= candidate.schema_max))) {
                    capability = &candidate;
                    break;
                }
            }
            if (!capability) return ON1X_SDK_UNSUPPORTED;
            kind = capability->artifact_kind;
            if (capability->flags & ON1X_SDK_BACKEND_DETERMINISTIC)
                artifact_flags |= ON1X_SDK_ARTIFACT_DECLARED_DETERMINISTIC;
            Selection selected{backend.name, backend.provider_run, backend.userdata,
                               backend.priority, backend.trusted, index, *capability};
            const On1xSDK_BackendInvocation invocation{
                ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
                sizeof(On1xSDK_BackendInvocation),
                program,
                nullptr,
                kind,
                schema,
                index,
                byte_limit,
                nullptr,
                nullptr
            };
            status = invoke_provider(selected, invocation, bytes);
        }
        if (status != ON1X_SDK_OK) return status;
        return on1x::sdk::backend::make_artifact(
            std::move(bytes), kind, schema, artifact_flags, backend.name, {}, out);
    });
}

} // namespace

namespace on1x::sdk {

On1xSDK_Status register_checked_hir_backend(On1xSDK_Registry *registry,
                                             const On1xSDK_Backend *backend) noexcept {
    return register_legacy(registry, backend, true);
}

On1xSDK_Status register_builtin_backend_provider(
    On1xSDK_Registry *registry, const On1xSDK_BackendProvider *provider) noexcept {
    return register_provider(registry, provider, true);
}

On1xSDK_Status run_builtin_backend(const On1xSDK_Registry *registry, const char *name,
                                    const On1xSDK_Program *program,
                                    On1xSDK_Artifact **out) noexcept {
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = nullptr;
    if (!program || (!program->checked_unit && !program->verified_module))
        return ON1X_SDK_INVALID_ARGUMENT;
    return run_named(registry, name, program, 256u * 1024u * 1024u, true, out);
}

} // namespace on1x::sdk

extern "C" {

On1xSDK_Status on1x_sdk_backend_register(On1xSDK_Registry *registry,
                                          const On1xSDK_Backend *backend) {
    return register_legacy(registry, backend, false);
}

On1xSDK_Status on1x_sdk_backend_run(const On1xSDK_Registry *registry, const char *name,
                                     const On1xSDK_Program *program,
                                     size_t byte_limit, On1xSDK_Artifact **out) {
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = nullptr;
    if (program && (program->checked_unit || program->verified_module))
        return ON1X_SDK_UNSUPPORTED;
    return normalize_v1_status(
        run_named(registry, name, program, byte_limit, false, out));
}

On1xSDK_Status on1x_sdk_backend_provider_register(
    On1xSDK_Registry *registry, const On1xSDK_BackendProvider *provider) {
    return register_provider(registry, provider, false);
}

On1xSDK_Status on1x_sdk_backend_poll(const On1xSDK_BackendInvocation *invocation) {
    if (!invocation) return ON1X_SDK_INVALID_ARGUMENT;
    std::uint32_t size{};
    if (const auto status =
            descriptor_header(invocation, ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
                              offsetof(On1xSDK_BackendInvocation, poll), size);
        status != ON1X_SDK_OK)
        return status;
    On1xSDK_BackendInvocation input{};
    std::memcpy(&input, invocation, std::min<std::size_t>(size, sizeof input));
    if (!field_available(size, offsetof(On1xSDK_BackendInvocation, poll),
                         sizeof(input.poll)))
        input.poll = nullptr;
    if (!field_available(size,
                         offsetof(On1xSDK_BackendInvocation, poll_userdata),
                         sizeof(input.poll_userdata)))
        input.poll_userdata = nullptr;
    return poll(input);
}

On1xSDK_Status on1x_sdk_backend_produce(const On1xSDK_Registry *registry,
                                         const On1xSDK_BackendRequest *request,
                                         On1xSDK_Artifact **out_artifact) {
    if (!out_artifact) return ON1X_SDK_INVALID_ARGUMENT;
    *out_artifact = nullptr;
    if (!registry || !request) return ON1X_SDK_INVALID_ARGUMENT;
    std::uint32_t size{};
    if (const auto status =
            descriptor_header(request, ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
                              offsetof(On1xSDK_BackendRequest, poll), size);
        status != ON1X_SDK_OK)
        return status;
    On1xSDK_BackendRequest input{};
    std::memcpy(&input, request, std::min<std::size_t>(size, sizeof input));
    if (!field_available(size, offsetof(On1xSDK_BackendRequest, poll),
                         sizeof(input.poll)))
        input.poll = nullptr;
    if (!field_available(size, offsetof(On1xSDK_BackendRequest, poll_userdata),
                         sizeof(input.poll_userdata)))
        input.poll_userdata = nullptr;
    if (!input.program || !valid_artifact(input.artifact_kind) ||
        native_artifact(input.artifact_kind))
        return native_artifact(input.artifact_kind) ? ON1X_SDK_UNSUPPORTED
                                                    : ON1X_SDK_INVALID_ARGUMENT;
    if (input.flags & ~request_flags || input.reserved)
        return ON1X_SDK_INVALID_ARGUMENT;
    if (input.byte_limit > ON1X_SDK_MAX_ARTIFACT_BYTES)
        return ON1X_SDK_LIMIT;
    std::string_view requested_name;
    if (!bytes_view(input.provider_name, input.provider_name_size,
                    ON1X_SDK_NAME_MAX, true, requested_name) ||
        (!requested_name.empty() && !on1x::sdk::valid_name(requested_name)))
        return ON1X_SDK_INVALID_ARGUMENT;
    std::uint32_t schema{};
    if (const auto status = derive_schema(input.program, input.input_schema, schema);
        status != ON1X_SDK_OK)
        return status;
    const auto phase = input.program->phase;
    Selection selection;
    if (const auto status =
            guarded([&] {
                return select_provider(registry, requested_name, phase, schema,
                                       input.artifact_kind, input.target,
                                       input.flags, selection);
            });
        status != ON1X_SDK_OK)
        return status;

    if (invocation_active(registry, selection.name))
        return ON1X_SDK_BUSY;

    return guarded([&] {
        /* Capture target metadata before entering provider code. */
        const std::string target =
            input.target ? input.target->triple : std::string{};
        const On1xSDK_BackendInvocation invocation{
            ON1X_SDK_BACKEND_PROVIDER_ABI_VERSION,
            sizeof(On1xSDK_BackendInvocation),
            input.program,
            input.target,
            input.artifact_kind,
            schema,
            selection.capability_index,
            input.byte_limit,
            input.poll,
            input.poll_userdata
        };
        ActiveScope active(registry, selection.name);
        std::vector<std::uint8_t> bytes;
        auto status = invoke_provider(selection, invocation, bytes);
        if (status == ON1X_SDK_OK &&
            (input.flags & ON1X_SDK_BACKEND_CHECK_REPRODUCIBILITY)) {
            std::vector<std::uint8_t> second;
            status = invoke_provider(selection, invocation, second);
            if (status == ON1X_SDK_OK && second != bytes)
                status = ON1X_SDK_ERROR;
        }
        if (status != ON1X_SDK_OK) return status;

        std::uint32_t artifact_flags = 0;
        if (selection.capability.flags & ON1X_SDK_BACKEND_DETERMINISTIC)
            artifact_flags |= ON1X_SDK_ARTIFACT_DECLARED_DETERMINISTIC;
        if (input.flags & ON1X_SDK_BACKEND_CHECK_REPRODUCIBILITY)
            artifact_flags |= ON1X_SDK_ARTIFACT_REPRODUCIBILITY_CHECKED;
        return on1x::sdk::backend::make_artifact(
            std::move(bytes), input.artifact_kind, schema, artifact_flags,
            selection.name, target, out_artifact);
    });
}

} // extern "C"
