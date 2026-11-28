#include "ProjectInternal.hpp"

#include <deque>

namespace on1x::sdk::project {
namespace {

std::string text(On1xSDK_ProjectString value, const char* field,
                 std::size_t max_bytes) {
    if (!valid_view(value))
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-DESCRIPTOR",
             std::string("invalid length-aware ") + field);
    if (value.size > max_bytes)
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-DESCRIPTOR",
             std::string("oversized length-aware ") + field);
    return std::string(as_view(value));
}

bool commit_id(std::string_view value) noexcept {
    return (value.size() == 40 || value.size() == 64) &&
           std::all_of(value.begin(), value.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

std::string project_kind(std::uint32_t kind) {
    switch (kind) {
        case ON1X_SDK_PROJECT_EXECUTABLE: return "project.executable";
        case ON1X_SDK_PROJECT_COMPLEX: return "project.complex";
        case ON1X_SDK_PROJECT_LIBRARY: return "project.library";
        default:
            fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-MANIFEST",
                 "manifest has an unknown project kind");
    }
}

struct Candidate {
    const On1xSDK_ProjectManifest* manifest;
    std::string location;
    std::string revision;
    std::string digest;
};

struct Lock {
    std::uint32_t kind;
    std::string source;
    std::uint32_t selector_kind;
    std::string selector;
    std::string commit;
};

bool same_requirement(const On1xSDK_ManifestDependency& left,
                      const On1xSDK_ManifestDependency& right) noexcept {
    return left.kind == right.kind &&
           as_view(left.source) == as_view(right.source) &&
           left.selector_kind == right.selector_kind &&
           as_view(left.selector) == as_view(right.selector);
}

void validate_lock(const On1xSDK_ManifestDependency& dependency,
                   const Lock& lock) {
    if (lock.kind != dependency.kind ||
        lock.source != as_view(dependency.source) ||
        lock.selector_kind != dependency.selector_kind ||
        lock.selector != as_view(dependency.selector) ||
        !commit_id(lock.commit))
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-STALE-LOCK",
             "stale lock entry for dependency '" +
                 std::string(as_view(dependency.name)) + "'");
    if (dependency.selector_kind == ON1X_SDK_SELECTOR_COMMIT &&
        lock.commit != as_view(dependency.selector))
        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-STALE-LOCK",
             "locked commit differs from the pinned manifest commit for '" +
                 std::string(as_view(dependency.name)) + "'");
}

} // namespace
} // namespace on1x::sdk::project

extern "C" On1xSDK_Status on1x_sdk_package_resolve_offline(
    const On1xSDK_ProjectManifest* root,
    On1xSDK_ProjectString root_digest,
    const On1xSDK_PackageCandidate* candidate_descriptors, size_t candidate_count,
    const On1xSDK_PackageLockEntry* lock_descriptors, size_t lock_count,
    const On1xSDK_PackageResolveOptions* options,
    const On1xSDK_ProjectAllocator* allocator,
    const On1xSDK_ProjectDiagnosticSink* sink,
    On1xSDK_BuildGraph** output) {
    using namespace on1x::sdk::project;
    if (!output) return ON1X_SDK_INVALID_ARGUMENT;
    *output = nullptr;
    if (!root || root->magic != manifest_magic || !valid_view(root_digest) ||
        !sha256_text(as_view(root_digest)) ||
        (candidate_count && !candidate_descriptors) ||
        (lock_count && !lock_descriptors))
        return ON1X_SDK_INVALID_ARGUMENT;
    if (!allocator || !allocator_valid(allocator)) {
        if (allocator &&
            allocator->abi_version != ON1X_SDK_PROJECT_ABI_VERSION)
            return ON1X_SDK_ABI_MISMATCH;
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (const auto status = diagnostic_sink_status(sink);
        status != ON1X_SDK_OK)
        return status;
    bool offline = true;
    bool require_lock = false;
    std::size_t max_packages = ON1X_SDK_MANIFEST_MAX_DEPENDENCIES;
    if (options) {
        if (options->abi_version != ON1X_SDK_PROJECT_ABI_VERSION)
            return ON1X_SDK_ABI_MISMATCH;
        if (options->struct_size < sizeof(std::uint32_t) * 2)
            return ON1X_SDK_INVALID_ARGUMENT;
        if (options->struct_size >=
            offsetof(On1xSDK_PackageResolveOptions, offline) +
                sizeof(options->offline))
            offline = options->offline != 0;
        if (options->struct_size >=
            offsetof(On1xSDK_PackageResolveOptions, require_lock) +
                sizeof(options->require_lock))
            require_lock = options->require_lock != 0;
        if (options->struct_size >=
                offsetof(On1xSDK_PackageResolveOptions, max_packages) +
                    sizeof(options->max_packages) &&
            options->max_packages)
            max_packages = options->max_packages;
    }

    Digest revision;
    std::copy(std::begin(root->info.source_revision),
              std::end(root->info.source_revision), revision.bytes.begin());
    static constexpr char alphabet[] = "0123456789abcdef";
    for (std::size_t i = 0; i < revision.bytes.size(); ++i) {
        revision.hex[i * 2] = alphabet[revision.bytes[i] >> 4];
        revision.hex[i * 2 + 1] = alphabet[revision.bytes[i] & 15];
    }
    revision.hex.back() = '\0';

    try {
        if (!offline)
            fail(ON1X_SDK_UNSUPPORTED, "ON1X-SDK-PACKAGE-NETWORK-UNSUPPORTED",
                 "package resolver is offline-only and performs no network access");
        if (!max_packages || max_packages >= ON1X_SDK_BUILD_GRAPH_MAX_NODES)
            fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-LIMIT",
                 "invalid package count limit");
        if (candidate_count > max_packages || lock_count > max_packages)
            fail(ON1X_SDK_LIMIT, "ON1X-SDK-PACKAGE-LIMIT",
                 "package candidate or lock count limit exceeded");

        constexpr auto candidate_required =
            offsetof(On1xSDK_PackageCandidate, content_sha256) +
            sizeof(On1xSDK_PackageCandidate::content_sha256);
        std::map<std::string, Candidate, std::less<>> candidates;
        for (std::size_t i = 0; i < candidate_count; ++i) {
            const auto& descriptor = candidate_descriptors[i];
            if (!on1x::sdk::project::descriptor(&descriptor, candidate_required))
                fail(descriptor.abi_version == ON1X_SDK_PROJECT_ABI_VERSION
                         ? ON1X_SDK_INVALID_ARGUMENT : ON1X_SDK_ABI_MISMATCH,
                     "ON1X-SDK-PACKAGE-DESCRIPTOR",
                     "invalid package candidate descriptor prefix");
            if (!descriptor.manifest || descriptor.manifest->magic != manifest_magic)
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-DESCRIPTOR",
                     "candidate manifest handle is invalid");
            const auto* info = &descriptor.manifest->info;
            if (info->kind != ON1X_SDK_PROJECT_LIBRARY)
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-MANIFEST",
                     "dependency candidate must be a library manifest");
            Candidate candidate{descriptor.manifest,
                                 text(descriptor.location, "candidate location",
                                      ON1X_SDK_PROJECT_MAX_PATH_BYTES),
                                 text(descriptor.revision, "candidate revision",
                                      64),
                                 text(descriptor.content_sha256,
                                      "candidate digest", 64)};
            if (!normalized_path(candidate.location, false) ||
                !sha256_text(candidate.digest) ||
                (!candidate.revision.empty() && !commit_id(candidate.revision)))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-DESCRIPTOR",
                     "invalid candidate path, revision or digest");
            const std::string name(as_view(info->name));
            if (name == as_view(root->info.name))
                fail(ON1X_SDK_INVALID_ARGUMENT,
                     "ON1X-SDK-PACKAGE-CANDIDATE-DUPLICATE",
                     "a dependency candidate cannot duplicate the root package");
            auto [found, inserted] = candidates.emplace(name, std::move(candidate));
            if (!inserted)
                fail(ON1X_SDK_DUPLICATE, "ON1X-SDK-PACKAGE-CANDIDATE-DUPLICATE",
                     "multiple candidates provide package '" + name + "'");
        }

        constexpr auto lock_required =
            offsetof(On1xSDK_PackageLockEntry, commit) +
            sizeof(On1xSDK_PackageLockEntry::commit);
        std::map<std::string, Lock, std::less<>> locks;
        for (std::size_t i = 0; i < lock_count; ++i) {
            const auto& descriptor = lock_descriptors[i];
            if (!on1x::sdk::project::descriptor(&descriptor, lock_required))
                fail(descriptor.abi_version == ON1X_SDK_PROJECT_ABI_VERSION
                         ? ON1X_SDK_INVALID_ARGUMENT : ON1X_SDK_ABI_MISMATCH,
                     "ON1X-SDK-PACKAGE-LOCK",
                     "invalid package lock descriptor prefix");
            const auto name = text(descriptor.name, "lock name", 64);
            if (!identifier_name(name) ||
                (descriptor.kind != ON1X_SDK_DEPENDENCY_GIT &&
                 descriptor.kind != ON1X_SDK_DEPENDENCY_GIT_REMOTE))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-LOCK",
                     "invalid lock name or dependency kind");
            Lock lock{descriptor.kind,
                      text(descriptor.source, "lock source", 2048),
                      descriptor.selector_kind,
                      text(descriptor.selector, "lock selector", 240),
                      text(descriptor.commit, "lock commit", 64)};
            if (!commit_id(lock.commit))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-LOCK",
                     "lock commit must be a full lowercase object ID");
            if (!locks.emplace(name, std::move(lock)).second)
                fail(ON1X_SDK_DUPLICATE, "ON1X-SDK-PACKAGE-LOCK-DUPLICATE",
                     "duplicate lock entry for package '" + name + "'");
        }

        std::vector<GraphNodeTemp> nodes;
        std::map<std::string, const On1xSDK_ManifestDependency*, std::less<>>
            requirements;
        std::set<std::string, std::less<>> selected;
        std::deque<const On1xSDK_ProjectManifest*> pending;
        pending.push_back(root);
        selected.insert(std::string(as_view(root->info.name)));
        while (!pending.empty()) {
            const auto* manifest = pending.front();
            pending.pop_front();
            GraphNodeTemp node;
            node.id = std::string(as_view(manifest->info.name));
            node.kind = manifest == root ? project_kind(manifest->info.kind) : "package";
            node.digest = manifest == root
                              ? std::string(as_view(root_digest))
                              : candidates.at(node.id).digest;
            for (std::size_t i = 0; i < manifest->dependency_count; ++i) {
                const auto& dependency = manifest->dependencies[i];
                const std::string name(as_view(dependency.name));
                auto [requirement, inserted] = requirements.emplace(name, &dependency);
                if (!inserted && !same_requirement(*requirement->second, dependency))
                    fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-CONFLICT",
                         "conflicting dependency requirements for package '" + name + "'");
                const auto candidate = candidates.find(name);
                if (candidate == candidates.end())
                    fail(ON1X_SDK_UNSUPPORTED, "ON1X-SDK-PACKAGE-OFFLINE-MISS",
                         "offline package candidate is missing for '" + name + "'");
                const auto& selected_candidate = candidate->second;
                if (dependency.kind == ON1X_SDK_DEPENDENCY_LOCAL) {
                    if (selected_candidate.location != as_view(dependency.source) ||
                        !selected_candidate.revision.empty())
                        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-CONFLICT",
                             "local package candidate does not match source for '" +
                                 name + "'");
                    if (locks.find(name) != locks.end())
                        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-STALE-LOCK",
                             "local dependency has an inapplicable lock entry: '" +
                                 name + "'");
                } else {
                    if (dependency.kind == ON1X_SDK_DEPENDENCY_GIT &&
                        selected_candidate.location != as_view(dependency.source))
                        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-CONFLICT",
                             "Git package candidate does not match source for '" + name + "'");
                    const auto lock = locks.find(name);
                    const bool lock_needed =
                        require_lock ||
                        dependency.selector_kind != ON1X_SDK_SELECTOR_COMMIT;
                    if (lock_needed && lock == locks.end())
                        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-LOCK-MISSING",
                             "matching lock entry is required for package '" + name + "'");
                    std::string expected;
                    if (lock != locks.end()) {
                        validate_lock(dependency, lock->second);
                        expected = lock->second.commit;
                    } else {
                        expected = std::string(as_view(dependency.selector));
                    }
                    if (selected_candidate.revision != expected)
                        fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-STALE-LOCK",
                             "candidate revision differs from the locked commit for '" +
                                 name + "'");
                }
                node.dependencies.push_back(name);
                if (selected.insert(name).second) {
                    if (selected.size() > max_packages + 1)
                        fail(ON1X_SDK_LIMIT, "ON1X-SDK-PACKAGE-LIMIT",
                             "resolved package count limit exceeded");
                    pending.push_back(selected_candidate.manifest);
                }
            }
            nodes.push_back(std::move(node));
        }
        for (const auto& [name, lock] : locks) {
            (void)lock;
            if (!requirements.contains(name))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-PACKAGE-STALE-LOCK",
                     "lockfile contains stale entry for undeclared package '" +
                         name + "'");
        }
        return create_graph(nodes, max_packages + 1,
                            ON1X_SDK_BUILD_GRAPH_MAX_EDGES,
                            4u * 1024u * 1024u, allocator, sink, output);
    } catch (const Failure& failure) {
        return emit(sink, failure, revision, 0);
    } catch (const std::bad_alloc&) {
        return emit(sink, {ON1X_SDK_LIMIT, "ON1X-SDK-PROJECT-ALLOCATION",
                           "temporary package resolution allocation failed"},
                    revision, 0);
    } catch (...) {
        return emit(sink, {ON1X_SDK_ERROR, "ON1X-SDK-PROJECT-INTERNAL",
                           "unexpected package resolver failure"},
                    revision, 0);
    }
}
