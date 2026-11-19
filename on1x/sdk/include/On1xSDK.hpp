#ifndef ON1X_SDK_CXX_HPP
#define ON1X_SDK_CXX_HPP

#include "On1xSDK-BuildGraph.h"
#include "On1xSDK-Formatter.h"
#include "On1xSDK-Manifest.h"

#include <cstddef>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace on1x::sdk {

class Error : public std::runtime_error {
public:
    Error(On1xSDK_Status status, const char* operation)
        : std::runtime_error(std::string(operation) + " failed with On1x SDK status " +
                             std::to_string(static_cast<unsigned>(status))),
          status_(status) {}
    On1xSDK_Status status() const noexcept { return status_; }
private:
    On1xSDK_Status status_;
};

inline On1xSDK_ProjectString string_view(std::string_view value) noexcept {
    return {value.empty() ? nullptr : value.data(), value.size()};
}

inline On1xSDK_ProjectBytes bytes_view(std::string_view value) noexcept {
    return {value.empty() ? nullptr :
                reinterpret_cast<const unsigned char*>(value.data()), value.size()};
}

class HostAllocator {
    static void* allocate(void*, std::size_t size, std::size_t alignment) noexcept {
        return ::operator new(size, static_cast<std::align_val_t>(alignment),
                              std::nothrow);
    }
    static void* reallocate(void*, void*, std::size_t, std::size_t,
                            std::size_t) noexcept {
        return nullptr; // Project services never request reallocation.
    }
    static void release(void*, void* pointer, std::size_t,
                        std::size_t alignment) noexcept {
        ::operator delete(pointer, static_cast<std::align_val_t>(alignment));
    }
public:
    static On1xSDK_ProjectAllocator value() noexcept {
        return {ON1X_SDK_PROJECT_ABI_VERSION, sizeof(On1xSDK_ProjectAllocator),
                nullptr, allocate, reallocate, release};
    }
};

class Buffer {
public:
    Buffer() noexcept = default;
    explicit Buffer(On1xSDK_ProjectBuffer* handle) noexcept : handle_(handle) {}
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }
    ~Buffer() { reset(); }
    void reset() noexcept {
        on1x_sdk_project_buffer_destroy(std::exchange(handle_, nullptr));
    }
    std::string_view view() const noexcept {
        const auto value = on1x_sdk_project_buffer_view(handle_);
        return value.size
                   ? std::string_view(reinterpret_cast<const char*>(value.data),
                                      value.size)
                   : std::string_view{};
    }
private:
    On1xSDK_ProjectBuffer* handle_ = nullptr;
};

class Manifest {
public:
    Manifest() noexcept = default;
    explicit Manifest(On1xSDK_ProjectManifest* handle) noexcept : handle_(handle) {}
    Manifest(const Manifest&) = delete;
    Manifest& operator=(const Manifest&) = delete;
    Manifest(Manifest&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}
    Manifest& operator=(Manifest&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }
    ~Manifest() { reset(); }

    static Manifest parse(std::string_view input) {
        auto allocator = HostAllocator::value();
        On1xSDK_ProjectManifest* handle = nullptr;
        const auto status = on1x_sdk_manifest_parse(
            bytes_view(input), nullptr, &allocator, nullptr, &handle);
        if (status != ON1X_SDK_OK) throw Error(status, "manifest parse");
        return Manifest(handle);
    }

    void reset() noexcept {
        on1x_sdk_manifest_destroy(std::exchange(handle_, nullptr));
    }
    const On1xSDK_ManifestInfo& info() const {
        const auto* result = on1x_sdk_manifest_info(handle_);
        if (!result) throw std::logic_error("empty On1x SDK manifest");
        return *result;
    }
    std::string_view name() const {
        const auto value = info().name;
        return {value.data, value.size};
    }
    std::string_view directory() const {
        const auto value = info().directory;
        return {value.data, value.size};
    }
    std::size_t target_count() const noexcept {
        return on1x_sdk_manifest_target_count(handle_);
    }
    const On1xSDK_ManifestTarget& target(std::size_t index) const {
        const auto* result = on1x_sdk_manifest_target_at(handle_, index);
        if (!result) throw std::out_of_range("On1x SDK manifest target");
        return *result;
    }
    std::size_t dependency_count() const noexcept {
        return on1x_sdk_manifest_dependency_count(handle_);
    }
    const On1xSDK_ManifestDependency& dependency(std::size_t index) const {
        const auto* result = on1x_sdk_manifest_dependency_at(handle_, index);
        if (!result) throw std::out_of_range("On1x SDK manifest dependency");
        return *result;
    }
    const On1xSDK_ProjectManifest* get() const noexcept { return handle_; }
private:
    On1xSDK_ProjectManifest* handle_ = nullptr;
};

struct BuildNode {
    std::string id;
    std::string kind;
    std::string input_sha256;
    std::vector<std::string> dependencies;
};

class BuildGraph {
public:
    BuildGraph() noexcept = default;
    explicit BuildGraph(On1xSDK_BuildGraph* handle) noexcept : handle_(handle) {}
    BuildGraph(const BuildGraph&) = delete;
    BuildGraph& operator=(const BuildGraph&) = delete;
    BuildGraph(BuildGraph&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}
    BuildGraph& operator=(BuildGraph&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }
    ~BuildGraph() { reset(); }

    static BuildGraph create(const std::vector<BuildNode>& nodes) {
        std::vector<std::vector<On1xSDK_ProjectString>> dependencies(nodes.size());
        std::vector<On1xSDK_BuildNodeDescriptor> descriptors(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            for (const auto& dependency : nodes[i].dependencies)
                dependencies[i].push_back(string_view(dependency));
            descriptors[i] = {
                ON1X_SDK_PROJECT_ABI_VERSION,
                sizeof(On1xSDK_BuildNodeDescriptor),
                string_view(nodes[i].id),
                string_view(nodes[i].kind),
                string_view(nodes[i].input_sha256),
                dependencies[i].size(),
                dependencies[i].empty() ? nullptr : dependencies[i].data()
            };
        }
        auto allocator = HostAllocator::value();
        On1xSDK_BuildGraph* handle = nullptr;
        const auto status = on1x_sdk_build_graph_create(
            descriptors.empty() ? nullptr : descriptors.data(), descriptors.size(),
            nullptr, &allocator, nullptr, &handle);
        if (status != ON1X_SDK_OK) throw Error(status, "build graph create");
        return BuildGraph(handle);
    }

    void reset() noexcept {
        on1x_sdk_build_graph_destroy(std::exchange(handle_, nullptr));
    }
    std::size_t size() const noexcept {
        return on1x_sdk_build_graph_node_count(handle_);
    }
    const On1xSDK_BuildNode& at(std::size_t index) const {
        const auto* result = on1x_sdk_build_graph_node_at(handle_, index);
        if (!result) throw std::out_of_range("On1x SDK build node");
        return *result;
    }
    std::string_view sha256() const noexcept {
        const auto value = on1x_sdk_build_graph_sha256(handle_);
        return value.size ? std::string_view(value.data, value.size)
                          : std::string_view{};
    }
private:
    On1xSDK_BuildGraph* handle_ = nullptr;
};

inline Buffer format_manifest(std::string_view input) {
    auto allocator = HostAllocator::value();
    On1xSDK_ProjectBuffer* handle = nullptr;
    const auto status = on1x_sdk_manifest_format(
        bytes_view(input), &allocator, nullptr, &handle);
    if (status != ON1X_SDK_OK) throw Error(status, "manifest format");
    return Buffer(handle);
}

} // namespace on1x::sdk

#endif
