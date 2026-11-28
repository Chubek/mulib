#include "ProjectInternal.hpp"

namespace on1x::sdk::project {
namespace {

bool covered(std::uint32_t size, std::size_t offset, std::size_t field_size) noexcept {
    return size >= offset && field_size <= size - offset;
}

template <class T>
void reserve_array(std::size_t count, std::size_t& total) {
    total = aligned(total, alignof(T));
    std::size_t bytes = 0;
    if (!checked_mul(count, sizeof(T), bytes) || !checked_add(total, bytes))
        fail(ON1X_SDK_LIMIT, "ON1X-SDK-PROJECT-SIZE", "manifest allocation size overflow");
}

void reserve_string(std::string_view value, std::size_t& total) {
    if (value.size() == std::numeric_limits<std::size_t>::max() ||
        !checked_add(total, value.size() + 1))
        fail(ON1X_SDK_LIMIT, "ON1X-SDK-PROJECT-SIZE", "manifest allocation size overflow");
}

On1xSDK_ProjectManifest* copy_manifest(
    const ManifestTemp& parsed, const Digest& digest,
    const On1xSDK_ProjectAllocator& allocator) {
    std::size_t size = 0;
    reserve_array<On1xSDK_ProjectManifest>(1, size);
    reserve_array<On1xSDK_ManifestTarget>(parsed.targets.size(), size);
    reserve_array<On1xSDK_ManifestDependency>(parsed.dependencies.size(), size);
    reserve_string(parsed.name, size);
    reserve_string(parsed.directory, size);
    for (const auto& target : parsed.targets) {
        reserve_string(target.name, size);
        reserve_string(target.source, size);
    }
    for (const auto& dependency : parsed.dependencies) {
        reserve_string(dependency.name, size);
        reserve_string(dependency.source, size);
        reserve_string(dependency.selector, size);
    }

    void* memory = allocate(allocator, size);
    try {
        Blob blob(memory, size);
        auto* manifest = blob.array<On1xSDK_ProjectManifest>();
        auto* targets = blob.array<On1xSDK_ManifestTarget>(parsed.targets.size());
        auto* dependencies =
            blob.array<On1xSDK_ManifestDependency>(parsed.dependencies.size());

        manifest->magic = manifest_magic;
        manifest->allocator = allocator;
        manifest->allocation_size = size;
        manifest->info.abi_version = ON1X_SDK_PROJECT_ABI_VERSION;
        manifest->info.struct_size = sizeof(manifest->info);
        manifest->info.kind = parsed.kind;
        manifest->info.name = blob.string(parsed.name);
        manifest->info.directory = blob.string(parsed.directory);
        std::copy(digest.bytes.begin(), digest.bytes.end(),
                  manifest->info.source_revision);
        manifest->target_count = parsed.targets.size();
        manifest->targets = parsed.targets.empty() ? nullptr : targets;
        manifest->dependency_count = parsed.dependencies.size();
        manifest->dependencies = parsed.dependencies.empty() ? nullptr : dependencies;

        for (std::size_t i = 0; i < parsed.targets.size(); ++i) {
            targets[i].abi_version = ON1X_SDK_PROJECT_ABI_VERSION;
            targets[i].struct_size = sizeof(targets[i]);
            targets[i].name = blob.string(parsed.targets[i].name);
            targets[i].source = blob.string(parsed.targets[i].source);
        }
        for (std::size_t i = 0; i < parsed.dependencies.size(); ++i) {
            dependencies[i].abi_version = ON1X_SDK_PROJECT_ABI_VERSION;
            dependencies[i].struct_size = sizeof(dependencies[i]);
            dependencies[i].name = blob.string(parsed.dependencies[i].name);
            dependencies[i].kind = parsed.dependencies[i].kind;
            dependencies[i].source = blob.string(parsed.dependencies[i].source);
            dependencies[i].selector_kind = parsed.dependencies[i].selector_kind;
            dependencies[i].selector = blob.string(parsed.dependencies[i].selector);
        }
        if (blob.used() != size)
            fail(ON1X_SDK_ERROR, "ON1X-SDK-PROJECT-INTERNAL",
                 "manifest allocation accounting mismatch");
        return manifest;
    } catch (...) {
        release(allocator, memory, size);
        throw;
    }
}

} // namespace
} // namespace on1x::sdk::project

extern "C" On1xSDK_Status on1x_sdk_manifest_parse(
    On1xSDK_ProjectBytes input, const On1xSDK_ManifestParseOptions* options,
    const On1xSDK_ProjectAllocator* allocator,
    const On1xSDK_ProjectDiagnosticSink* sink,
    On1xSDK_ProjectManifest** out_manifest) {
    using namespace on1x::sdk::project;
    if (!out_manifest) return ON1X_SDK_INVALID_ARGUMENT;
    *out_manifest = nullptr;
    if (!valid_view(input) || !allocator) return ON1X_SDK_INVALID_ARGUMENT;
    if (!allocator_valid(allocator)) {
        if (allocator->abi_version != ON1X_SDK_PROJECT_ABI_VERSION)
            return ON1X_SDK_ABI_MISMATCH;
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (const auto status = diagnostic_sink_status(sink);
        status != ON1X_SDK_OK)
        return status;
    std::size_t max_bytes = ON1X_SDK_MANIFEST_MAX_BYTES;
    std::size_t max_targets = ON1X_SDK_MANIFEST_MAX_TARGETS;
    std::size_t max_dependencies = ON1X_SDK_MANIFEST_MAX_DEPENDENCIES;
    if (options) {
        if (options->abi_version != ON1X_SDK_PROJECT_ABI_VERSION)
            return ON1X_SDK_ABI_MISMATCH;
        if (options->struct_size < sizeof(std::uint32_t) * 2)
            return ON1X_SDK_INVALID_ARGUMENT;
        if (covered(options->struct_size, offsetof(On1xSDK_ManifestParseOptions, max_bytes),
                    sizeof(options->max_bytes)) && options->max_bytes)
            max_bytes = options->max_bytes;
        if (covered(options->struct_size, offsetof(On1xSDK_ManifestParseOptions, max_targets),
                    sizeof(options->max_targets)) && options->max_targets)
            max_targets = options->max_targets;
        if (covered(options->struct_size,
                    offsetof(On1xSDK_ManifestParseOptions, max_dependencies),
                    sizeof(options->max_dependencies)) && options->max_dependencies)
            max_dependencies = options->max_dependencies;
    }
    if (!max_bytes || max_bytes > ON1X_SDK_MANIFEST_MAX_BYTES ||
        !max_targets || max_targets > ON1X_SDK_MANIFEST_MAX_TARGETS ||
        !max_dependencies || max_dependencies > ON1X_SDK_MANIFEST_MAX_DEPENDENCIES)
        return ON1X_SDK_INVALID_ARGUMENT;

    /*
     * The public hard limit is also a dereference budget.  Do not hash a
     * caller view whose declared size is outside that budget.
     */
    if (input.size > ON1X_SDK_MANIFEST_MAX_BYTES)
        return ON1X_SDK_LIMIT;
    const auto revision = sha256(as_span(input));
    try {
        if (input.size > max_bytes)
            fail(ON1X_SDK_LIMIT, "ON1X-SDK-MANIFEST-BYTE-LIMIT",
                 "manifest byte limit exceeded", 0, input.size);
        const auto text =
            input.size
                ? std::string_view(reinterpret_cast<const char*>(input.data),
                                   input.size)
                : std::string_view{};
        auto parsed = parse_manifest_temp(text, max_targets, max_dependencies);
        *out_manifest = copy_manifest(parsed, revision, *allocator);
        return ON1X_SDK_OK;
    } catch (const Failure& failure) {
        return emit(sink, failure, revision, input.size);
    } catch (const std::bad_alloc&) {
        return emit(sink, {ON1X_SDK_LIMIT, "ON1X-SDK-PROJECT-ALLOCATION",
                           "temporary manifest allocation failed", 0, input.size},
                    revision, input.size);
    } catch (...) {
        return emit(sink, {ON1X_SDK_ERROR, "ON1X-SDK-PROJECT-INTERNAL",
                           "unexpected manifest parser failure", 0, input.size},
                    revision, input.size);
    }
}

extern "C" const On1xSDK_ManifestInfo* on1x_sdk_manifest_info(
    const On1xSDK_ProjectManifest* manifest) {
    using namespace on1x::sdk::project;
    return manifest && manifest->magic == manifest_magic ? &manifest->info : nullptr;
}

extern "C" size_t on1x_sdk_manifest_target_count(
    const On1xSDK_ProjectManifest* manifest) {
    using namespace on1x::sdk::project;
    return manifest && manifest->magic == manifest_magic ? manifest->target_count : 0;
}

extern "C" const On1xSDK_ManifestTarget* on1x_sdk_manifest_target_at(
    const On1xSDK_ProjectManifest* manifest, size_t index) {
    using namespace on1x::sdk::project;
    return manifest && manifest->magic == manifest_magic &&
                   index < manifest->target_count
               ? &manifest->targets[index]
               : nullptr;
}

extern "C" size_t on1x_sdk_manifest_dependency_count(
    const On1xSDK_ProjectManifest* manifest) {
    using namespace on1x::sdk::project;
    return manifest && manifest->magic == manifest_magic
               ? manifest->dependency_count
               : 0;
}

extern "C" const On1xSDK_ManifestDependency* on1x_sdk_manifest_dependency_at(
    const On1xSDK_ProjectManifest* manifest, size_t index) {
    using namespace on1x::sdk::project;
    return manifest && manifest->magic == manifest_magic &&
                   index < manifest->dependency_count
               ? &manifest->dependencies[index]
               : nullptr;
}

extern "C" void on1x_sdk_manifest_destroy(On1xSDK_ProjectManifest* manifest) {
    using namespace on1x::sdk::project;
    if (!manifest || manifest->magic != manifest_magic) return;
    const auto allocator = manifest->allocator;
    const auto size = manifest->allocation_size;
    manifest->magic = 0;
    release(allocator, manifest, size);
}
