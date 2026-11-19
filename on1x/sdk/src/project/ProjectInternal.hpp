#pragma once

#include "On1xSDK-BuildGraph.h"
#include "On1xSDK-CodeAction.h"
#include "On1xSDK-Command.h"
#include "On1xSDK-DiagnosticCatalog.h"
#include "On1xSDK-Formatter.h"
#include "On1xSDK-Manifest.h"
#include "On1xSDK-PackageResolver.h"
#include "On1xSDK-Process.h"
#include "On1xSDK-Reproducibility.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace on1x::sdk::project {

inline constexpr std::uint64_t buffer_magic = UINT64_C(0x4f315850524a4246);
inline constexpr std::uint64_t manifest_magic = UINT64_C(0x4f315850524a4d46);
inline constexpr std::uint64_t graph_magic = UINT64_C(0x4f315850524a4746);
inline constexpr std::size_t object_alignment = alignof(std::max_align_t);

struct Failure {
    On1xSDK_Status status;
    const char* code;
    std::string message;
    std::size_t begin = 0;
    std::size_t end = 0;
};

[[noreturn]] inline void fail(On1xSDK_Status status, const char* code,
                              std::string message, std::size_t begin = 0,
                              std::size_t end = 0) {
    throw Failure{status, code, std::move(message), begin, end};
}

inline On1xSDK_ProjectString view(std::string_view value) noexcept {
    return {value.empty() ? nullptr : value.data(), value.size()};
}

inline bool valid_view(On1xSDK_ProjectString value) noexcept {
    return value.data != nullptr || value.size == 0;
}

inline bool valid_view(On1xSDK_ProjectBytes value) noexcept {
    return value.data != nullptr || value.size == 0;
}

inline std::string_view as_view(On1xSDK_ProjectString value) noexcept {
    return value.size ? std::string_view(value.data, value.size) : std::string_view{};
}

inline std::span<const unsigned char> as_span(On1xSDK_ProjectBytes value) noexcept {
    return value.size ? std::span<const unsigned char>(value.data, value.size)
                      : std::span<const unsigned char>{};
}

template <class T>
inline bool descriptor(const T* value, std::size_t minimum) noexcept {
    return value && value->abi_version == ON1X_SDK_PROJECT_ABI_VERSION &&
           value->struct_size >= minimum;
}

inline bool checked_add(std::size_t& total, std::size_t value) noexcept {
    if (value > std::numeric_limits<std::size_t>::max() - total) return false;
    total += value;
    return true;
}

inline bool checked_mul(std::size_t count, std::size_t size,
                        std::size_t& output) noexcept {
    if (count && size > std::numeric_limits<std::size_t>::max() / count) return false;
    output = count * size;
    return true;
}

inline constexpr bool ascii_alpha(unsigned char value) noexcept {
    return (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z');
}

inline constexpr bool ascii_digit(unsigned char value) noexcept {
    return value >= '0' && value <= '9';
}

inline constexpr bool ascii_alnum(unsigned char value) noexcept {
    return ascii_alpha(value) || ascii_digit(value);
}

inline std::size_t aligned(std::size_t value, std::size_t alignment) {
    const auto mask = alignment - 1;
    if ((alignment & mask) != 0 || value > std::numeric_limits<std::size_t>::max() - mask)
        fail(ON1X_SDK_LIMIT, "ON1X-SDK-PROJECT-SIZE", "allocation size overflow");
    return (value + mask) & ~mask;
}

bool allocator_valid(const On1xSDK_ProjectAllocator* allocator) noexcept;
On1xSDK_Status diagnostic_sink_status(
    const On1xSDK_ProjectDiagnosticSink* sink) noexcept;
void* allocate(const On1xSDK_ProjectAllocator& allocator, std::size_t size);
void release(const On1xSDK_ProjectAllocator& allocator, void* pointer,
             std::size_t size) noexcept;

class Blob {
public:
    Blob(void* memory, std::size_t size) noexcept
        : base_(static_cast<unsigned char*>(memory)), size_(size) {}

    template <class T>
    T* array(std::size_t count = 1) {
        cursor_ = aligned(cursor_, alignof(T));
        std::size_t bytes = 0;
        if (!checked_mul(count, sizeof(T), bytes) || cursor_ > size_ ||
            bytes > size_ - cursor_)
            fail(ON1X_SDK_LIMIT, "ON1X-SDK-PROJECT-SIZE", "allocation layout overflow");
        auto* result = reinterpret_cast<T*>(base_ + cursor_);
        std::memset(result, 0, bytes);
        cursor_ += bytes;
        return result;
    }

    On1xSDK_ProjectString string(std::string_view value) {
        if (cursor_ > size_ ||
            value.size() == std::numeric_limits<std::size_t>::max() ||
            value.size() + 1 > size_ - cursor_)
            fail(ON1X_SDK_LIMIT, "ON1X-SDK-PROJECT-SIZE", "allocation layout overflow");
        char* result = reinterpret_cast<char*>(base_ + cursor_);
        if (!value.empty()) std::memcpy(result, value.data(), value.size());
        result[value.size()] = '\0';
        cursor_ += value.size() + 1;
        return {value.empty() ? nullptr : result, value.size()};
    }

    std::size_t used() const noexcept { return cursor_; }

private:
    unsigned char* base_;
    std::size_t size_;
    std::size_t cursor_ = 0;
};

struct Digest {
    std::array<unsigned char, ON1X_SDK_PROJECT_SHA256_BYTES> bytes{};
    std::array<char, ON1X_SDK_PROJECT_SHA256_TEXT_BYTES + 1> hex{};
};

Digest sha256(std::span<const unsigned char> input) noexcept;
bool sha256_text(std::string_view input) noexcept;
bool utf8(std::string_view input) noexcept;
bool normalized_path(std::string_view input, bool allow_root_dot) noexcept;
bool absolute_path(std::string_view input) noexcept;
bool simple_name(std::string_view input) noexcept;
bool identifier_name(std::string_view input) noexcept;
bool command_component(std::string_view input) noexcept;

On1xSDK_Status callback_status(On1xSDK_Status status) noexcept;
On1xSDK_Status emit(const On1xSDK_ProjectDiagnosticSink* sink,
                    const Failure& failure, const Digest& revision,
                    std::size_t input_size) noexcept;

On1xSDK_ProjectBuffer* make_buffer(const On1xSDK_ProjectAllocator& allocator,
                                   std::string_view bytes);

struct TargetTemp {
    std::string name;
    std::string source;
};

struct DependencyTemp {
    std::string name;
    std::uint32_t kind = 0;
    std::string source;
    std::uint32_t selector_kind = 0;
    std::string selector;
};

struct ManifestTemp {
    std::uint32_t kind = 0;
    std::string name;
    std::string directory = ".";
    std::vector<TargetTemp> targets;
    std::vector<DependencyTemp> dependencies;
};

struct Token {
    std::string value;
    bool quoted = false;
    std::size_t begin = 0;
    std::size_t end = 0;
    std::string spelling;
};

struct Row {
    std::size_t line_begin = 0;
    std::size_t line_end = 0;
    std::vector<Token> tokens;
    std::string comment;
};

std::vector<Row> lex_manifest(std::string_view input);
ManifestTemp parse_manifest_temp(std::string_view input, std::size_t max_targets,
                                 std::size_t max_dependencies,
                                 std::vector<Row>* rows = nullptr);

struct GraphNodeTemp {
    std::string id;
    std::string kind;
    std::string digest;
    std::vector<std::string> dependencies;
};

On1xSDK_Status create_graph(const std::vector<GraphNodeTemp>& nodes,
                            std::size_t max_nodes, std::size_t max_edges,
                            std::size_t max_bytes,
                            const On1xSDK_ProjectAllocator* allocator,
                            const On1xSDK_ProjectDiagnosticSink* sink,
                            On1xSDK_BuildGraph** output) noexcept;

} // namespace on1x::sdk::project

struct On1xSDK_ProjectBuffer {
    std::uint64_t magic;
    On1xSDK_ProjectAllocator allocator;
    std::size_t allocation_size;
    std::size_t size;
    const unsigned char* data;
};

struct On1xSDK_ProjectManifest {
    std::uint64_t magic;
    On1xSDK_ProjectAllocator allocator;
    std::size_t allocation_size;
    On1xSDK_ManifestInfo info;
    std::size_t target_count;
    On1xSDK_ManifestTarget* targets;
    std::size_t dependency_count;
    On1xSDK_ManifestDependency* dependencies;
};

struct On1xSDK_BuildGraph {
    std::uint64_t magic;
    On1xSDK_ProjectAllocator allocator;
    std::size_t allocation_size;
    std::size_t node_count;
    On1xSDK_BuildNode* nodes;
    char hash[ON1X_SDK_PROJECT_SHA256_TEXT_BYTES + 1];
};
