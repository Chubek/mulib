#include "On1xSDK-BuildGraph.h"
#include "On1xSDK-CodeAction.h"
#include "On1xSDK-Command.h"
#include "On1xSDK-DiagnosticCatalog.h"
#include "On1xSDK-Formatter.h"
#include "On1xSDK-Manifest.h"
#include "On1xSDK-PackageResolver.h"
#include "On1xSDK-Process.h"
#include "On1xSDK-Project.h"
#include "On1xSDK-Reproducibility.h"
#include "On1xSDK.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(std::is_same_v<On1xSDK_ProjectString, On1xSDK_StringView>);
static_assert(std::is_same_v<On1xSDK_ProjectBytes, On1xSDK_ByteView>);
static_assert(std::is_same_v<On1xSDK_ProjectAllocator, On1xSDK_Allocator>);
static_assert(std::is_same_v<On1xSDK_ProjectSpan, On1xSDK_SourceSpan>);
static_assert(std::is_same_v<
              On1xSDK_ProjectDiagnostic, On1xSDK_DiagnosticV2>);
static_assert(std::is_same_v<
              On1xSDK_ProjectDiagnosticSink, On1xSDK_DiagnosticSinkV2>);
static_assert(std::is_same_v<
              On1xSDK_ProcessCancellation, On1xSDK_Cancellation>);

namespace {

#define CHECK(expression) do {                                                \
    if (!(expression)) {                                                      \
        std::fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__,        \
                     #expression);                                            \
        std::abort();                                                         \
    }                                                                         \
} while (false)

using namespace std::string_view_literals;

On1xSDK_ProjectString text(std::string_view value) {
    return {value.empty() ? nullptr : value.data(), value.size()};
}

On1xSDK_ProjectBytes bytes(std::string_view value) {
    return {value.empty()
                ? nullptr
                : reinterpret_cast<const unsigned char*>(value.data()),
            value.size()};
}

std::string value(On1xSDK_ProjectString input) {
    return input.size ? std::string(input.data, input.size) : std::string{};
}

struct AllocationState {
    std::size_t allocations = 0;
    std::size_t releases = 0;
    std::size_t live = 0;
    bool reject = false;
};

void* allocate(void* context, std::size_t size, std::size_t alignment) {
    auto& state = *static_cast<AllocationState*>(context);
    ++state.allocations;
    if (state.reject) return nullptr;
    void* result =
        ::operator new(size, static_cast<std::align_val_t>(alignment),
                       std::nothrow);
    if (result) ++state.live;
    return result;
}

void* reallocate(void*, void*, std::size_t, std::size_t, std::size_t) {
    std::abort(); // The API documents that the current services never call it.
}

void release(void* context, void* pointer, std::size_t,
             std::size_t alignment) {
    auto& state = *static_cast<AllocationState*>(context);
    CHECK(pointer != nullptr);
    CHECK(state.live != 0);
    --state.live;
    ++state.releases;
    ::operator delete(pointer, static_cast<std::align_val_t>(alignment));
}

On1xSDK_ProjectAllocator allocator(AllocationState& state) {
    return {ON1X_SDK_PROJECT_ABI_VERSION,
            sizeof(On1xSDK_ProjectAllocator), &state, allocate, reallocate,
            release};
}

struct Diagnostics {
    std::vector<std::string> codes;
    std::vector<std::string> messages;
    std::vector<std::pair<std::size_t, std::size_t>> spans;
    On1xSDK_Status response = ON1X_SDK_OK;
};

On1xSDK_Status diagnostic(void* context,
                          const On1xSDK_ProjectDiagnostic* item) {
    auto& output = *static_cast<Diagnostics*>(context);
    CHECK(item != nullptr);
    CHECK(item->abi_version == ON1X_SDK_PROJECT_ABI_VERSION);
    CHECK(item->struct_size >= sizeof(*item));
    CHECK(item->severity == ON1X_SDK_PROJECT_ERROR);
    CHECK(item->primary.source_id != 0 && item->primary.revision != 0);
    CHECK(item->primary.line_begin == UINT32_MAX &&
          item->primary.column_begin == UINT32_MAX);
    output.codes.push_back(value(item->code));
    output.messages.push_back(value(item->message));
    output.spans.emplace_back(
        item->primary.byte_begin, item->primary.byte_end);
    return output.response;
}

On1xSDK_ProjectDiagnosticSink sink(Diagnostics& diagnostics) {
    return {ON1X_SDK_PROJECT_ABI_VERSION,
            sizeof(On1xSDK_ProjectDiagnosticSink), diagnostic, &diagnostics};
}

std::string sha256(std::string_view input) {
    std::array<unsigned char, ON1X_SDK_PROJECT_SHA256_BYTES> digest{};
    CHECK(on1x_sdk_project_sha256(bytes(input), digest.data()) ==
          ON1X_SDK_OK);
    static constexpr char alphabet[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (const auto byte : digest) {
        result.push_back(alphabet[byte >> 4]);
        result.push_back(alphabet[byte & 15]);
    }
    return result;
}

On1xSDK_Status parse_manifest(
    std::string_view input, On1xSDK_ProjectAllocator& memory,
    On1xSDK_ProjectManifest** output,
    const On1xSDK_ProjectDiagnosticSink* diagnostics = nullptr,
    const On1xSDK_ManifestParseOptions* options = nullptr) {
    return on1x_sdk_manifest_parse(bytes(input), options, &memory, diagnostics,
                                   output);
}

On1xSDK_ProjectManifest* require_manifest(
    std::string_view input, On1xSDK_ProjectAllocator& memory) {
    On1xSDK_ProjectManifest* result = nullptr;
    CHECK(parse_manifest(input, memory, &result) == ON1X_SDK_OK);
    CHECK(result != nullptr);
    return result;
}

void project_primitives() {
    std::array<unsigned char, 32> digest{};
    CHECK(on1x_sdk_project_sha256(bytes("abc"), digest.data()) == ON1X_SDK_OK);
    CHECK(sha256("abc") ==
          "ba7816bf8f01cfea414140de5dae2223"
          "b00361a396177a9cb410ff61f20015ad");
    const unsigned char empty_storage = 0;
    CHECK(on1x_sdk_project_sha256(
              {&empty_storage, 0}, digest.data()) == ON1X_SDK_OK);
    CHECK(std::equal(
        digest.begin(), digest.end(),
        std::array<unsigned char, 32>{
            0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
            0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
            0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
            0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55}.begin()));
    CHECK(on1x_sdk_project_sha256({nullptr, 1}, digest.data()) ==
          ON1X_SDK_INVALID_ARGUMENT);

    std::array<char, ON1X_SDK_PROJECT_MAX_PATH_BYTES> output{};
    std::size_t size = 0;
    CHECK(on1x_sdk_project_normalize_path(text("src/lib.on1x"), 0,
                                          output.data(), output.size(),
                                          &size) == ON1X_SDK_OK);
    CHECK(std::string_view(output.data(), size) == "src/lib.on1x");
    CHECK(on1x_sdk_project_normalize_path(text("src/../secret"), 0,
                                          output.data(), output.size(),
                                          &size) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_project_normalize_path(text("/absolute"), 0, output.data(),
                                          output.size(), &size) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_project_normalize_path(text("."), 1, nullptr, 0, &size) ==
          ON1X_SDK_LIMIT);
    CHECK(size == 1);
    CHECK(on1x_sdk_project_normalize_path(text("."), 0, output.data(),
                                          output.size(), &size) ==
          ON1X_SDK_INVALID_ARGUMENT);
    std::array<char, 32> overlap{};
    std::memcpy(overlap.data(), "src/main.on1x", 13);
    CHECK(on1x_sdk_project_normalize_path(
              {overlap.data(), 13}, 0, overlap.data() + 1,
              overlap.size() - 1, &size) == ON1X_SDK_OK);
    CHECK(std::string_view(overlap.data() + 1, size) == "src/main.on1x");
}

void manifests_and_formatting() {
    AllocationState allocations;
    auto memory = allocator(allocations);
    Diagnostics diagnostics;
    auto reports = sink(diagnostics);
    std::string source =
        "on1xfile 1\n"
        "kind executable\n"
        "name \"demo\"\n"
        "directory \"src\"\n"
        "target \"demo\" \"main.on1x\"\n"
        "dependency \"localdep\" local \"vendor/localdep\"\n"
        "dependency \"gitdep\" git \"repositories/gitdep.git\" branch \"main\"\n";
    const auto expected_revision = sha256(source);
    On1xSDK_ProjectManifest* manifest = nullptr;
    CHECK(parse_manifest(source, memory, &manifest, &reports) == ON1X_SDK_OK);
    CHECK(diagnostics.codes.empty());
    CHECK(manifest != nullptr);
    source.assign(source.size(), 'x'); // The manifest is a deep immutable copy.

    const auto* info = on1x_sdk_manifest_info(manifest);
    CHECK(info != nullptr);
    CHECK(info->kind == ON1X_SDK_PROJECT_EXECUTABLE);
    CHECK(value(info->name) == "demo");
    CHECK(value(info->directory) == "src");
    std::string revision;
    static constexpr char alphabet[] = "0123456789abcdef";
    for (const auto byte : info->source_revision) {
        revision.push_back(alphabet[byte >> 4]);
        revision.push_back(alphabet[byte & 15]);
    }
    CHECK(revision == expected_revision);
    CHECK(on1x_sdk_manifest_target_count(manifest) == 1);
    const auto* target = on1x_sdk_manifest_target_at(manifest, 0);
    CHECK(target && value(target->name) == "demo");
    CHECK(value(target->source) == "main.on1x");
    CHECK(on1x_sdk_manifest_target_at(manifest, 1) == nullptr);
    CHECK(on1x_sdk_manifest_dependency_count(manifest) == 2);
    const auto* local = on1x_sdk_manifest_dependency_at(manifest, 0);
    const auto* git = on1x_sdk_manifest_dependency_at(manifest, 1);
    CHECK(local && local->kind == ON1X_SDK_DEPENDENCY_LOCAL);
    CHECK(value(local->source) == "vendor/localdep");
    CHECK(git && git->kind == ON1X_SDK_DEPENDENCY_GIT);
    CHECK(git->selector_kind == ON1X_SDK_SELECTOR_BRANCH);
    CHECK(value(git->selector) == "main");
    on1x_sdk_manifest_destroy(manifest);

    const std::string untidy =
        "  on1xfile   1 \r\n"
        "\tkind   executable\n"
        "name \"demo\"   # identity  \n"
        "target \"demo\" \"main.on1x\"\n\n";
    On1xSDK_ProjectBuffer* formatted = nullptr;
    CHECK(on1x_sdk_manifest_format(bytes(untidy), &memory, &reports,
                                   &formatted) == ON1X_SDK_OK);
    auto formatted_view = on1x_sdk_project_buffer_view(formatted);
    const std::string clean(
        reinterpret_cast<const char*>(formatted_view.data),
        formatted_view.size);
    CHECK(clean ==
          "on1xfile 1\n"
          "kind executable\n"
          "name \"demo\" # identity\n"
          "target \"demo\" \"main.on1x\"\n");
    On1xSDK_ProjectBuffer* second = nullptr;
    CHECK(on1x_sdk_manifest_format(bytes(clean), &memory, nullptr, &second) ==
          ON1X_SDK_OK);
    const auto second_view = on1x_sdk_project_buffer_view(second);
    CHECK(std::string_view(
              reinterpret_cast<const char*>(second_view.data),
              second_view.size) == clean);
    on1x_sdk_project_buffer_destroy(second);
    on1x_sdk_project_buffer_destroy(formatted);

    const std::array invalid{
        "on1xfile 1\nkind executable\nname \"x\"\n"
        "target \"x\" \"../escape\"\n",
        "on1xfile 1\nkind executable\nkind library\nname \"x\"\n",
        "on1xfile 1\nkind executable\nname \"x\"\nprofile \"p\" \"p.lua\"\n",
        "on1xfile 9\nkind executable\nname \"x\"\n",
    };
    const std::array<On1xSDK_Status, invalid.size()> expected{
        ON1X_SDK_INVALID_ARGUMENT, ON1X_SDK_DUPLICATE,
        ON1X_SDK_UNSUPPORTED, ON1X_SDK_INVALID_ARGUMENT};
    for (std::size_t i = 0; i < invalid.size(); ++i) {
        manifest = reinterpret_cast<On1xSDK_ProjectManifest*>(1);
        const auto before = diagnostics.codes.size();
        CHECK(parse_manifest(invalid[i], memory, &manifest, &reports) ==
              expected[i]);
        CHECK(manifest == nullptr);
        CHECK(diagnostics.codes.size() == before + 1);
    }

    On1xSDK_ManifestParseOptions tiny{
        ON1X_SDK_PROJECT_ABI_VERSION,
        sizeof(On1xSDK_ManifestParseOptions), 32, 0, 0};
    manifest = nullptr;
    CHECK(parse_manifest(
              "on1xfile 1\nkind executable\nname \"x\"\n"
              "target \"x\" \"x.on1x\"\n",
              memory, &manifest, &reports, &tiny) == ON1X_SDK_LIMIT);
    CHECK(manifest == nullptr);

    diagnostics.response = ON1X_SDK_UNSATISFIABLE;
    manifest = nullptr;
    CHECK(parse_manifest(invalid[0], memory, &manifest, &reports) ==
          ON1X_SDK_UNSATISFIABLE);
    diagnostics.response = ON1X_SDK_OK;

    allocations.reject = true;
    manifest = nullptr;
    CHECK(parse_manifest(
              "on1xfile 1\nkind library\nname \"lib\"\n", memory, &manifest,
              &reports) == ON1X_SDK_LIMIT);
    CHECK(manifest == nullptr);
    allocations.reject = false;
    CHECK(allocations.live == 0);

    auto bad_sink = reports;
    bad_sink.abi_version = 99;
    manifest = nullptr;
    CHECK(parse_manifest(
              "on1xfile 1\nkind library\nname \"lib\"\n", memory, &manifest,
              &bad_sink) == ON1X_SDK_ABI_MISMATCH);

    const unsigned char one = 'x';
    const On1xSDK_ProjectBytes oversized{
        &one, ON1X_SDK_MANIFEST_MAX_BYTES + 1};
    CHECK(on1x_sdk_manifest_parse(
              oversized, nullptr, &memory, nullptr, &manifest) ==
          ON1X_SDK_LIMIT);
    formatted = reinterpret_cast<On1xSDK_ProjectBuffer*>(1);
    CHECK(on1x_sdk_manifest_format(
              oversized, &memory, nullptr, &formatted) == ON1X_SDK_LIMIT);
    CHECK(formatted == nullptr);
}

struct Node {
    std::string id;
    std::string kind = "compile";
    std::string digest = std::string(64, 'a');
    std::vector<std::string> dependencies;
};

On1xSDK_Status make_graph(
    const std::vector<Node>& input, On1xSDK_ProjectAllocator& memory,
    On1xSDK_BuildGraph** output,
    const On1xSDK_ProjectDiagnosticSink* diagnostics = nullptr,
    const On1xSDK_BuildGraphLimits* limits = nullptr) {
    std::vector<std::vector<On1xSDK_ProjectString>> dependency_views(
        input.size());
    std::vector<On1xSDK_BuildNodeDescriptor> descriptors(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        for (const auto& dependency : input[i].dependencies)
            dependency_views[i].push_back(text(dependency));
        descriptors[i] = {
            ON1X_SDK_PROJECT_ABI_VERSION,
            sizeof(On1xSDK_BuildNodeDescriptor),
            text(input[i].id),
            text(input[i].kind),
            text(input[i].digest),
            dependency_views[i].size(),
            dependency_views[i].empty() ? nullptr
                                        : dependency_views[i].data()};
    }
    return on1x_sdk_build_graph_create(
        descriptors.empty() ? nullptr : descriptors.data(), descriptors.size(),
        limits, &memory, diagnostics, output);
}

void build_graphs() {
    AllocationState allocations;
    auto memory = allocator(allocations);
    Diagnostics diagnostics;
    auto reports = sink(diagnostics);
    const std::vector<Node> nodes{
        {"link", "link", std::string(64, 'c'), {"compile-a", "compile-b"}},
        {"compile-b", "compile", std::string(64, 'b'), {}},
        {"compile-a", "compile", std::string(64, 'a'), {}}};
    On1xSDK_BuildGraph* graph = nullptr;
    CHECK(make_graph(nodes, memory, &graph, &reports) == ON1X_SDK_OK);
    CHECK(on1x_sdk_build_graph_node_count(graph) == 3);
    CHECK(value(on1x_sdk_build_graph_node_at(graph, 0)->id) == "compile-a");
    CHECK(value(on1x_sdk_build_graph_node_at(graph, 1)->id) == "compile-b");
    CHECK(value(on1x_sdk_build_graph_node_at(graph, 2)->id) == "link");
    const auto hash = value(on1x_sdk_build_graph_sha256(graph));
    CHECK(hash.size() == 64);
    struct ExtendedRecord {
        On1xSDK_ReproducibilityRecord record;
        std::uint32_t trailing;
    } reproducibility{{ON1X_SDK_PROJECT_ABI_VERSION,
                       sizeof(ExtendedRecord), {}, {}, 0},
                      0x12345678u};
    CHECK(on1x_sdk_reproducibility_describe(
              graph, &reproducibility.record) == ON1X_SDK_OK);
    CHECK(value(reproducibility.record.schema) == "on1x.build-graph/v1");
    CHECK(value(reproducibility.record.graph_sha256) == hash);
    CHECK(reproducibility.record.input_count == 3);
    CHECK(reproducibility.trailing == 0x12345678u);

    auto reordered = nodes;
    std::reverse(reordered.begin(), reordered.end());
    On1xSDK_BuildGraph* same = nullptr;
    CHECK(make_graph(reordered, memory, &same) == ON1X_SDK_OK);
    CHECK(value(on1x_sdk_build_graph_sha256(same)) == hash);
    on1x_sdk_build_graph_destroy(same);
    on1x_sdk_build_graph_destroy(graph);

    const std::array failures{
        std::vector<Node>{{"consumer", "compile", std::string(64, 'a'),
                           {"absent"}}},
        std::vector<Node>{{"a", "compile", std::string(64, 'a'), {"b"}},
                          {"b", "compile", std::string(64, 'b'), {"a"}}},
        std::vector<Node>{{"same", "compile", std::string(64, 'a'), {}},
                          {"same", "link", std::string(64, 'a'), {}}},
        std::vector<Node>{{"same", "compile", std::string(64, 'a'), {}},
                          {"same", "compile", std::string(64, 'a'), {}}},
    };
    const std::array expected{
        ON1X_SDK_UNSUPPORTED, ON1X_SDK_ERROR, ON1X_SDK_INVALID_ARGUMENT,
        ON1X_SDK_DUPLICATE};
    const std::array expected_code{
        "ON1X-SDK-BUILD-MISSING"sv, "ON1X-SDK-BUILD-CYCLE"sv,
        "ON1X-SDK-BUILD-CONFLICT"sv, "ON1X-SDK-BUILD-NODE-DUPLICATE"sv};
    for (std::size_t i = 0; i < failures.size(); ++i) {
        graph = reinterpret_cast<On1xSDK_BuildGraph*>(1);
        const auto before = diagnostics.codes.size();
        CHECK(make_graph(failures[i], memory, &graph, &reports) ==
              expected[i]);
        CHECK(graph == nullptr);
        CHECK(diagnostics.codes.size() == before + 1);
        CHECK(diagnostics.codes.back() == expected_code[i]);
    }

    On1xSDK_BuildGraphLimits limit{
        ON1X_SDK_PROJECT_ABI_VERSION, sizeof(On1xSDK_BuildGraphLimits), 2, 8,
        4096};
    graph = nullptr;
    CHECK(make_graph(nodes, memory, &graph, &reports, &limit) ==
          ON1X_SDK_LIMIT);
    CHECK(graph == nullptr);

    const char one = 'x';
    const std::string digest(64, 'a');
    On1xSDK_BuildNodeDescriptor oversized{
        ON1X_SDK_PROJECT_ABI_VERSION,
        sizeof(On1xSDK_BuildNodeDescriptor),
        {&one, 129},
        text("compile"),
        text(digest),
        0,
        nullptr};
    CHECK(on1x_sdk_build_graph_create(
              &oversized, 1, nullptr, &memory, nullptr, &graph) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(graph == nullptr);

    On1xSDK_BuildGraphLimits node_limit{
        ON1X_SDK_PROJECT_ABI_VERSION, sizeof(On1xSDK_BuildGraphLimits), 1, 8,
        4096};
    CHECK(on1x_sdk_build_graph_create(
              &oversized, 2, &node_limit, &memory, nullptr, &graph) ==
          ON1X_SDK_LIMIT);
    CHECK(graph == nullptr);

    const On1xSDK_ProjectString dependency = text("provider");
    const std::array edge_descriptors{
        On1xSDK_BuildNodeDescriptor{
            ON1X_SDK_PROJECT_ABI_VERSION,
            sizeof(On1xSDK_BuildNodeDescriptor),
            text("first"),
            text("compile"),
            text(digest),
            1,
            &dependency},
        On1xSDK_BuildNodeDescriptor{
            ON1X_SDK_PROJECT_ABI_VERSION,
            sizeof(On1xSDK_BuildNodeDescriptor),
            text("second"),
            text("compile"),
            text(digest),
            1,
            reinterpret_cast<const On1xSDK_ProjectString*>(
                std::uintptr_t{1})}};
    On1xSDK_BuildGraphLimits edge_limit{
        ON1X_SDK_PROJECT_ABI_VERSION, sizeof(On1xSDK_BuildGraphLimits), 2, 1,
        4096};
    CHECK(on1x_sdk_build_graph_create(
              edge_descriptors.data(), edge_descriptors.size(), &edge_limit,
              &memory, nullptr, &graph) == ON1X_SDK_LIMIT);
    CHECK(graph == nullptr);
    CHECK(allocations.live == 0);
}

void package_resolution() {
    AllocationState allocations;
    auto memory = allocator(allocations);
    Diagnostics diagnostics;
    auto reports = sink(diagnostics);
    const std::string root_source =
        "on1xfile 1\n"
        "kind executable\n"
        "name \"app\"\n"
        "target \"app\" \"main.on1x\"\n"
        "dependency \"localpkg\" local \"vendor/localpkg\"\n"
        "dependency \"gitpkg\" git \"repositories/gitpkg.git\" branch \"main\"\n";
    const std::string local_source =
        "on1xfile 1\nkind library\nname \"localpkg\"\n";
    const std::string git_source =
        "on1xfile 1\nkind library\nname \"gitpkg\"\n";
    auto* root = require_manifest(root_source, memory);
    auto* local = require_manifest(local_source, memory);
    auto* git = require_manifest(git_source, memory);
    const std::string commit(40, '1');
    const std::string root_hash = sha256(root_source);
    const std::string local_hash = sha256(local_source);
    const std::string git_hash = sha256(git_source);
    const std::array candidates{
        On1xSDK_PackageCandidate{
            ON1X_SDK_PROJECT_ABI_VERSION,
            sizeof(On1xSDK_PackageCandidate), local, text("vendor/localpkg"),
            {}, text(local_hash)},
        On1xSDK_PackageCandidate{
            ON1X_SDK_PROJECT_ABI_VERSION,
            sizeof(On1xSDK_PackageCandidate), git,
            text("repositories/gitpkg.git"), text(commit), text(git_hash)}};
    const std::array locks{
        On1xSDK_PackageLockEntry{
            ON1X_SDK_PROJECT_ABI_VERSION,
            sizeof(On1xSDK_PackageLockEntry), text("gitpkg"),
            ON1X_SDK_DEPENDENCY_GIT, text("repositories/gitpkg.git"),
            ON1X_SDK_SELECTOR_BRANCH, text("main"), text(commit)}};
    On1xSDK_PackageResolveOptions options{
        ON1X_SDK_PROJECT_ABI_VERSION,
        sizeof(On1xSDK_PackageResolveOptions), 1, 0, 32};
    On1xSDK_BuildGraph* graph = nullptr;
    CHECK(on1x_sdk_package_resolve_offline(
              root, text(root_hash), candidates.data(), candidates.size(),
              locks.data(), locks.size(), &options, &memory, &reports,
              &graph) == ON1X_SDK_OK);
    CHECK(graph != nullptr);
    CHECK(on1x_sdk_build_graph_node_count(graph) == 3);
    CHECK(value(on1x_sdk_build_graph_node_at(graph, 0)->id) == "gitpkg");
    CHECK(value(on1x_sdk_build_graph_node_at(graph, 1)->id) == "localpkg");
    CHECK(value(on1x_sdk_build_graph_node_at(graph, 2)->id) == "app");
    on1x_sdk_build_graph_destroy(graph);

    auto oversized_candidate = candidates;
    const char one = 'x';
    oversized_candidate[0].location = {
        &one, ON1X_SDK_PROJECT_MAX_PATH_BYTES + 1};
    CHECK(on1x_sdk_package_resolve_offline(
              root, text(root_hash), oversized_candidate.data(),
              oversized_candidate.size(), locks.data(), locks.size(), &options,
              &memory, &reports, &graph) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(graph == nullptr);

    graph = nullptr;
    CHECK(on1x_sdk_package_resolve_offline(
              root, text(root_hash), candidates.data(), candidates.size(),
              nullptr, 0, &options, &memory, &reports, &graph) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(diagnostics.codes.back() == "ON1X-SDK-PACKAGE-LOCK-MISSING");
    auto stale = locks;
    const std::string stale_commit(40, '2');
    stale[0].commit = text(stale_commit);
    CHECK(on1x_sdk_package_resolve_offline(
              root, text(root_hash), candidates.data(), candidates.size(),
              stale.data(), stale.size(), &options, &memory, &reports,
              &graph) == ON1X_SDK_INVALID_ARGUMENT);
    CHECK(diagnostics.codes.back() == "ON1X-SDK-PACKAGE-STALE-LOCK");
    options.offline = 0;
    CHECK(on1x_sdk_package_resolve_offline(
              root, text(root_hash), candidates.data(), candidates.size(),
              locks.data(), locks.size(), &options, &memory, &reports,
              &graph) == ON1X_SDK_UNSUPPORTED);
    CHECK(diagnostics.codes.back() ==
          "ON1X-SDK-PACKAGE-NETWORK-UNSUPPORTED");
    options.offline = 1;

    const std::string cycle_root_source =
        "on1xfile 1\nkind executable\nname \"cycleapp\"\n"
        "target \"cycleapp\" \"main.on1x\"\n"
        "dependency \"a\" local \"vendor/a\"\n";
    const std::string a_source =
        "on1xfile 1\nkind library\nname \"a\"\n"
        "dependency \"b\" local \"vendor/b\"\n";
    const std::string b_source =
        "on1xfile 1\nkind library\nname \"b\"\n"
        "dependency \"a\" local \"vendor/a\"\n";
    auto* cycle_root = require_manifest(cycle_root_source, memory);
    auto* a = require_manifest(a_source, memory);
    auto* b = require_manifest(b_source, memory);
    const std::string cycle_root_hash = sha256(cycle_root_source);
    const std::string a_hash = sha256(a_source);
    const std::string b_hash = sha256(b_source);
    const std::array cycle_candidates{
        On1xSDK_PackageCandidate{
            ON1X_SDK_PROJECT_ABI_VERSION,
            sizeof(On1xSDK_PackageCandidate), a, text("vendor/a"), {},
            text(a_hash)},
        On1xSDK_PackageCandidate{
            ON1X_SDK_PROJECT_ABI_VERSION,
            sizeof(On1xSDK_PackageCandidate), b, text("vendor/b"), {},
            text(b_hash)}};
    CHECK(on1x_sdk_package_resolve_offline(
              cycle_root, text(cycle_root_hash), cycle_candidates.data(),
              cycle_candidates.size(), nullptr, 0, &options, &memory, &reports,
              &graph) == ON1X_SDK_ERROR);
    CHECK(diagnostics.codes.back() == "ON1X-SDK-BUILD-CYCLE");

    on1x_sdk_manifest_destroy(b);
    on1x_sdk_manifest_destroy(a);
    on1x_sdk_manifest_destroy(cycle_root);
    on1x_sdk_manifest_destroy(git);
    on1x_sdk_manifest_destroy(local);
    on1x_sdk_manifest_destroy(root);
    CHECK(allocations.live == 0);
}

struct ProcessContext {
    On1xSDK_Status authorize_status = ON1X_SDK_OK;
    On1xSDK_Status execute_status = ON1X_SDK_OK;
    On1xSDK_Status finish_status = ON1X_SDK_OK;
    std::uint64_t token = 77;
    int authorize_calls = 0;
    int execute_calls = 0;
    int finish_calls = 0;
    int cancellation_polls = 0;
    std::vector<std::string> arguments;
    std::uint64_t finished_token = UINT64_MAX;
    On1xSDK_Status operation_status = ON1X_SDK_OK;
};

On1xSDK_Status authorize_process(void* context,
                                 const On1xSDK_ProcessRequest* request,
                                 std::uint64_t* authorization) {
    auto& state = *static_cast<ProcessContext*>(context);
    ++state.authorize_calls;
    state.arguments.clear();
    for (std::size_t i = 0; i < request->argument_count; ++i)
        state.arguments.push_back(value(request->arguments[i]));
    *authorization = state.token;
    return state.authorize_status;
}

On1xSDK_Status execute_process(
    void* context, std::uint64_t authorization,
    const On1xSDK_ProcessRequest*,
    const On1xSDK_ProcessCancellation* cancellation,
    On1xSDK_ProcessResult* result) {
    auto& state = *static_cast<ProcessContext*>(context);
    ++state.execute_calls;
    CHECK(authorization == state.token);
    if (cancellation)
        state.cancellation_polls +=
            on1x_sdk_cancellation_poll(cancellation) == ON1X_SDK_CANCELLED
                ? 1 : 0;
    result->abi_version = ON1X_SDK_PROJECT_ABI_VERSION;
    result->struct_size = sizeof(*result);
    result->exit_code = 0;
    result->signal_number = 0;
    result->output_bytes = 12;
    return state.execute_status;
}

On1xSDK_Status finish_process(void* context, std::uint64_t authorization,
                              On1xSDK_Status operation_status) {
    auto& state = *static_cast<ProcessContext*>(context);
    ++state.finish_calls;
    state.finished_token = authorization;
    state.operation_status = operation_status;
    return state.finish_status;
}

On1xSDK_ProcessHost process_host(ProcessContext& state) {
    return {ON1X_SDK_PROJECT_ABI_VERSION, sizeof(On1xSDK_ProcessHost), &state,
            authorize_process, execute_process, finish_process};
}

void processes_and_commands() {
    const std::array arguments{text("/usr/bin/tool"), text("literal; $(touch x)"),
                               text("two words")};
    const std::array environment{text("LC_ALL=C"), text("PATH=/usr/bin")};
    On1xSDK_ProcessRequest request{
        ON1X_SDK_PROJECT_ABI_VERSION,
        sizeof(On1xSDK_ProcessRequest),
        text("/usr/bin/tool"),
        text("/work"),
        arguments.size(),
        arguments.data(),
        environment.size(),
        environment.data(),
        500,
        1024,
        2000};
    CHECK(on1x_sdk_process_request_validate(&request) == ON1X_SDK_OK);
    ProcessContext state;
    auto host = process_host(state);
    On1xSDK_CancellationConfig cancellation_config{
        ON1X_SDK_ABI_VERSION_V2,
        sizeof(On1xSDK_CancellationConfig), 0};
    On1xSDK_ProcessCancellation* cancellation = nullptr;
    CHECK(on1x_sdk_cancellation_create(
              &cancellation_config, &cancellation) == ON1X_SDK_OK);
    CHECK(on1x_sdk_cancellation_cancel(cancellation) == ON1X_SDK_OK);
    On1xSDK_ProcessResult result{};
    CHECK(on1x_sdk_process_dispatch(&host, &request, cancellation, &result) ==
          ON1X_SDK_OK);
    CHECK(state.authorize_calls == 1 && state.execute_calls == 1 &&
          state.finish_calls == 1);
    CHECK(state.arguments[1] == "literal; $(touch x)");
    CHECK(state.arguments[2] == "two words");
    CHECK(state.cancellation_polls == 1);
    CHECK(result.exit_code == 0 && result.output_bytes == 12);
    state.execute_status = ON1X_SDK_BUSY;
    CHECK(on1x_sdk_process_dispatch(&host, &request, cancellation, &result) ==
          ON1X_SDK_BUSY);
    CHECK(state.finish_calls == 2);
    CHECK(state.operation_status == ON1X_SDK_BUSY);
    CHECK(result.struct_size == 0);

    state.execute_status = ON1X_SDK_OK;
    state.token = 0;
    CHECK(on1x_sdk_process_dispatch(&host, &request, nullptr, &result) ==
          ON1X_SDK_ERROR);
    CHECK(state.execute_calls == 2);
    CHECK(state.finish_calls == 3);
    CHECK(state.finished_token == 0);
    CHECK(state.operation_status == ON1X_SDK_ERROR);
    state.token = 77;
    on1x_sdk_cancellation_release(cancellation);

    auto duplicate_environment = environment;
    duplicate_environment[1] = text("LC_ALL=other");
    request.environment = duplicate_environment.data();
    CHECK(on1x_sdk_process_request_validate(&request) == ON1X_SDK_DUPLICATE);
    request.environment = environment.data();
    request.executable = text("tool");
    CHECK(on1x_sdk_process_request_validate(&request) ==
          ON1X_SDK_INVALID_ARGUMENT);
    request.executable = text("/usr/bin/tool");
    const char one = 'x';
    auto oversized_arguments = arguments;
    oversized_arguments[1] = {
        &one, ON1X_SDK_PROCESS_MAX_ARGUMENT_BYTES + 1};
    request.arguments = oversized_arguments.data();
    CHECK(on1x_sdk_process_request_validate(&request) == ON1X_SDK_LIMIT);
    request.arguments = arguments.data();

    state.arguments.clear();
    CHECK(on1x_sdk_formatter_dispatch_source(
              &host, text("/usr/bin/on1xc"), text("/work/input.on1x"),
              text("/work"), nullptr, &result) == ON1X_SDK_OK);
    CHECK((state.arguments ==
           std::vector<std::string>{"/usr/bin/on1xc", "fmt", "--target",
                                    "plain", "/work/input.on1x"}));
    CHECK(on1x_sdk_formatter_dispatch_source(
              &host, text("/usr/bin/on1xc"), text("../input.on1x"),
              text("/work"), nullptr, &result) ==
          ON1X_SDK_INVALID_ARGUMENT);

    CHECK(on1x_sdk_diagnostic_catalog_dispatch_check(
              &host, text("/usr/bin/on1xc"), text("/work/catalog.json"),
              text("/work/input.on1x"), text("/work"), nullptr, &result) ==
          ON1X_SDK_OK);
    CHECK((state.arguments ==
           std::vector<std::string>{
               "/usr/bin/on1xc", "--check", "--diagnostics", "json",
               "--color", "never", "--diagnostic-catalog",
               "/work/catalog.json", "/work/input.on1x"}));
    state.authorize_status = static_cast<On1xSDK_Status>(99);
    CHECK(on1x_sdk_process_dispatch(&host, &request, nullptr, &result) ==
          ON1X_SDK_ERROR);
    state.authorize_status = ON1X_SDK_OK;
    state.finish_status = ON1X_SDK_LIMIT;
    CHECK(on1x_sdk_process_dispatch(&host, &request, nullptr, &result) ==
          ON1X_SDK_LIMIT);
    CHECK(result.struct_size == 0);
    state.finish_status = ON1X_SDK_OK;

    CHECK(on1x_sdk_command_is_builtin(text("build")) == 1);
    CHECK(on1x_sdk_command_validate_extension_name(text("build")) ==
          ON1X_SDK_DUPLICATE);
    CHECK(on1x_sdk_command_validate_extension_name(text("vendor:inspect")) ==
          ON1X_SDK_OK);
    CHECK(on1x_sdk_command_validate_extension_name(text("inspect")) ==
          ON1X_SDK_DUPLICATE);
    CHECK(on1x_sdk_command_validate_extension_name(text("vendor::inspect")) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(on1x_sdk_command_validate_extension_name(text("Vendor:inspect")) ==
          ON1X_SDK_INVALID_ARGUMENT);
}

void code_actions_and_cpp() {
    AllocationState allocations;
    auto memory = allocator(allocations);
    Diagnostics diagnostics;
    auto reports = sink(diagnostics);
    const std::string source = "abc\n";
    const std::string revision = sha256(source);
    const std::array edits{
        On1xSDK_CodeActionEdit{
            ON1X_SDK_PROJECT_ABI_VERSION, sizeof(On1xSDK_CodeActionEdit), 0, 1,
            text("A")},
        On1xSDK_CodeActionEdit{
            ON1X_SDK_PROJECT_ABI_VERSION, sizeof(On1xSDK_CodeActionEdit), 2, 3,
            text("C")}};
    On1xSDK_ProjectBuffer* candidate = nullptr;
    CHECK(on1x_sdk_code_action_apply_candidate(
              bytes(source), text(revision), edits.data(), edits.size(),
              &memory, &reports, &candidate) == ON1X_SDK_OK);
    const auto candidate_view = on1x_sdk_project_buffer_view(candidate);
    CHECK(std::string_view(
              reinterpret_cast<const char*>(candidate_view.data),
              candidate_view.size) == "AbC\n");
    on1x_sdk_project_buffer_destroy(candidate);

    candidate = reinterpret_cast<On1xSDK_ProjectBuffer*>(1);
    CHECK(on1x_sdk_code_action_apply_candidate(
              bytes(source), text(std::string(64, '0')), edits.data(),
              edits.size(), &memory, &reports, &candidate) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(candidate == nullptr);
    CHECK(diagnostics.codes.back() == "ON1X-SDK-CODE-ACTION-STALE");
    auto overlapping = edits;
    overlapping[1].begin = 0;
    CHECK(on1x_sdk_code_action_apply_candidate(
              bytes(source), text(revision), overlapping.data(),
              overlapping.size(), &memory, &reports, &candidate) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(diagnostics.codes.back() == "ON1X-SDK-CODE-ACTION-SPAN");
    const std::string unicode_source = "\xcf\x80";
    const std::string unicode_revision = sha256(unicode_source);
    On1xSDK_CodeActionEdit split{
        ON1X_SDK_PROJECT_ABI_VERSION, sizeof(On1xSDK_CodeActionEdit), 1, 1,
        text("x")};
    CHECK(on1x_sdk_code_action_apply_candidate(
              bytes(unicode_source), text(unicode_revision), &split, 1,
              &memory, &reports, &candidate) ==
          ON1X_SDK_INVALID_ARGUMENT);
    CHECK(diagnostics.codes.back() == "ON1X-SDK-CODE-ACTION-SPAN");
    const std::string empty;
    const std::string empty_revision = sha256(empty);
    CHECK(on1x_sdk_code_action_apply_candidate(
              bytes(empty), text(empty_revision), nullptr, 0, &memory, &reports,
              &candidate) == ON1X_SDK_OK);
    CHECK(on1x_sdk_project_buffer_view(candidate).size == 0);
    on1x_sdk_project_buffer_destroy(candidate);
    const unsigned char one = 'x';
    candidate = reinterpret_cast<On1xSDK_ProjectBuffer*>(1);
    CHECK(on1x_sdk_code_action_apply_candidate(
              {&one, ON1X_SDK_CODE_ACTION_MAX_OUTPUT + 1},
              text(empty_revision), nullptr, 0, &memory, &reports,
              &candidate) == ON1X_SDK_LIMIT);
    CHECK(candidate == nullptr);
    On1xSDK_CodeActionEdit oversized_replacement{
        ON1X_SDK_PROJECT_ABI_VERSION, sizeof(On1xSDK_CodeActionEdit), 0, 0,
        {reinterpret_cast<const char*>(&one),
         ON1X_SDK_CODE_ACTION_MAX_OUTPUT + 1}};
    CHECK(on1x_sdk_code_action_apply_candidate(
              bytes(empty), text(empty_revision), &oversized_replacement, 1,
              &memory, &reports, &candidate) == ON1X_SDK_LIMIT);
    CHECK(candidate == nullptr);
    CHECK(allocations.live == 0);

    const std::string manifest_source =
        "on1xfile 1\nkind executable\nname \"cpp\"\n"
        "target \"cpp\" \"main.on1x\"\n";
    auto manifest = on1x::sdk::Manifest::parse(manifest_source);
    CHECK(manifest.name() == "cpp");
    CHECK(manifest.target_count() == 1);
    auto formatted = on1x::sdk::format_manifest(manifest_source);
    CHECK(formatted.view() == manifest_source);
    const std::vector<on1x::sdk::BuildNode> nodes{
        {"compile", "compile", std::string(64, 'a'), {}},
        {"link", "link", std::string(64, 'b'), {"compile"}}};
    auto graph = on1x::sdk::BuildGraph::create(nodes);
    CHECK(graph.size() == 2);
    CHECK(value(graph.at(0).id) == "compile");
    CHECK(graph.sha256().size() == 64);
}

} // namespace

int main() {
    project_primitives();
    manifests_and_formatting();
    build_graphs();
    package_resolution();
    processes_and_commands();
    code_actions_and_cpp();
}
