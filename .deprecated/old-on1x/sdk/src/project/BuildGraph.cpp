#include "ProjectInternal.hpp"

namespace on1x::sdk::project {
namespace {

bool graph_id(std::string_view value) noexcept {
    if (value.empty() || value.size() > 128) return false;
    const auto first = static_cast<unsigned char>(value.front());
    if (!ascii_alpha(first) && first != '_') return false;
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char c) {
        return ascii_alnum(c) || c == '_' || c == '-' || c == '.' || c == ':';
    });
}

bool graph_kind(std::string_view value) noexcept {
    if (value.empty() || value.size() > 64 || value.front() < 'a' || value.front() > 'z')
        return false;
    return std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
               c == '-' || c == '.';
    });
}

void canonical_field(std::string& output, std::string_view value) {
    output += std::to_string(value.size());
    output.push_back(':');
    output.append(value);
    output.push_back('\n');
}

std::string canonical_graph(const std::vector<GraphNodeTemp>& nodes,
                            const std::vector<std::size_t>& order) {
    std::string output("on1x.build-graph/v1\n");
    for (const auto index : order) {
        const auto& node = nodes[index];
        canonical_field(output, node.id);
        canonical_field(output, node.kind);
        canonical_field(output, node.digest);
        output += std::to_string(node.dependencies.size());
        output.push_back('\n');
        for (const auto& dependency : node.dependencies)
            canonical_field(output, dependency);
    }
    return output;
}

std::vector<std::size_t> topological(const std::vector<GraphNodeTemp>& nodes) {
    std::map<std::string, std::size_t, std::less<>> index;
    for (std::size_t i = 0; i < nodes.size(); ++i) index.emplace(nodes[i].id, i);
    std::vector<std::size_t> incoming(nodes.size());
    std::vector<std::vector<std::size_t>> consumers(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        incoming[i] = nodes[i].dependencies.size();
        for (const auto& dependency : nodes[i].dependencies) {
            const auto found = index.find(dependency);
            if (found == index.end())
                fail(ON1X_SDK_UNSUPPORTED, "ON1X-SDK-BUILD-MISSING",
                     "build node '" + nodes[i].id +
                         "' requires missing provider '" + dependency + "'");
            consumers[found->second].push_back(i);
        }
    }
    std::set<std::string, std::less<>> ready;
    for (std::size_t i = 0; i < nodes.size(); ++i)
        if (!incoming[i]) ready.insert(nodes[i].id);
    std::vector<std::size_t> order;
    order.reserve(nodes.size());
    while (!ready.empty()) {
        const auto id = *ready.begin();
        ready.erase(ready.begin());
        const auto current = index.at(id);
        order.push_back(current);
        for (const auto consumer : consumers[current])
            if (!--incoming[consumer]) ready.insert(nodes[consumer].id);
    }
    if (order.size() != nodes.size()) {
        std::string members;
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (incoming[i]) {
                if (!members.empty()) members += ", ";
                members += nodes[i].id;
            }
        fail(ON1X_SDK_ERROR, "ON1X-SDK-BUILD-CYCLE",
             "build graph contains a dependency cycle: " + members);
    }
    return order;
}

template <class T>
void reserve_array(std::size_t count, std::size_t& total) {
    total = aligned(total, alignof(T));
    std::size_t bytes = 0;
    if (!checked_mul(count, sizeof(T), bytes) || !checked_add(total, bytes))
        fail(ON1X_SDK_LIMIT, "ON1X-SDK-BUILD-SIZE", "build graph allocation overflow");
}

void reserve_string(std::string_view value, std::size_t& total) {
    if (value.size() == std::numeric_limits<std::size_t>::max() ||
        !checked_add(total, value.size() + 1))
        fail(ON1X_SDK_LIMIT, "ON1X-SDK-BUILD-SIZE", "build graph allocation overflow");
}

On1xSDK_BuildGraph* copy_graph(const std::vector<GraphNodeTemp>& nodes,
                               const std::vector<std::size_t>& order,
                               std::string_view canonical,
                               std::size_t max_bytes,
                               const On1xSDK_ProjectAllocator& allocator) {
    std::size_t size = 0;
    reserve_array<On1xSDK_BuildGraph>(1, size);
    reserve_array<On1xSDK_BuildNode>(nodes.size(), size);
    for (const auto index : order)
        reserve_array<On1xSDK_ProjectString>(nodes[index].dependencies.size(), size);
    for (const auto index : order) {
        const auto& node = nodes[index];
        reserve_string(node.id, size);
        reserve_string(node.kind, size);
        reserve_string(node.digest, size);
        for (const auto& dependency : node.dependencies)
            reserve_string(dependency, size);
    }
    if (size > max_bytes)
        fail(ON1X_SDK_LIMIT, "ON1X-SDK-BUILD-BYTE-LIMIT",
             "copied build graph exceeds its byte limit");
    const auto hash = sha256(std::span(
        reinterpret_cast<const unsigned char*>(canonical.data()), canonical.size()));

    void* memory = allocate(allocator, size);
    try {
        Blob blob(memory, size);
        auto* graph = blob.array<On1xSDK_BuildGraph>();
        auto* output_nodes = blob.array<On1xSDK_BuildNode>(nodes.size());
        std::vector<On1xSDK_ProjectString*> dependency_arrays;
        dependency_arrays.reserve(nodes.size());
        for (const auto index : order) {
            const auto count = nodes[index].dependencies.size();
            dependency_arrays.push_back(
                count ? blob.array<On1xSDK_ProjectString>(count) : nullptr);
        }
        graph->magic = graph_magic;
        graph->allocator = allocator;
        graph->allocation_size = size;
        graph->node_count = nodes.size();
        graph->nodes = nodes.empty() ? nullptr : output_nodes;
        std::copy(hash.hex.begin(), hash.hex.end(), graph->hash);

        for (std::size_t position = 0; position < order.size(); ++position) {
            const auto& source = nodes[order[position]];
            auto& target = output_nodes[position];
            target.abi_version = ON1X_SDK_PROJECT_ABI_VERSION;
            target.struct_size = sizeof(target);
            target.id = blob.string(source.id);
            target.kind = blob.string(source.kind);
            target.input_sha256 = blob.string(source.digest);
            target.dependency_count = source.dependencies.size();
            target.dependencies = dependency_arrays[position];
            target.topological_index = position;
            for (std::size_t j = 0; j < source.dependencies.size(); ++j)
                dependency_arrays[position][j] = blob.string(source.dependencies[j]);
        }
        if (blob.used() != size)
            fail(ON1X_SDK_ERROR, "ON1X-SDK-PROJECT-INTERNAL",
                 "build graph allocation accounting mismatch");
        return graph;
    } catch (...) {
        release(allocator, memory, size);
        throw;
    }
}

Digest graph_revision(const std::vector<GraphNodeTemp>& nodes) noexcept {
    std::string material;
    try {
        for (const auto& node : nodes) {
            material.append(node.id);
            material.push_back('\0');
            material.append(node.digest);
            material.push_back('\0');
        }
    } catch (...) {
        return sha256({});
    }
    return sha256(std::span(reinterpret_cast<const unsigned char*>(material.data()),
                            material.size()));
}

} // namespace

On1xSDK_Status create_graph(const std::vector<GraphNodeTemp>& supplied,
                            std::size_t max_nodes, std::size_t max_edges,
                            std::size_t max_bytes,
                            const On1xSDK_ProjectAllocator* allocator,
                            const On1xSDK_ProjectDiagnosticSink* sink,
                            On1xSDK_BuildGraph** output) noexcept {
    if (!output) return ON1X_SDK_INVALID_ARGUMENT;
    *output = nullptr;
    if (!allocator || !allocator_valid(allocator)) {
        if (allocator && allocator->abi_version != ON1X_SDK_PROJECT_ABI_VERSION)
            return ON1X_SDK_ABI_MISMATCH;
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (const auto status = diagnostic_sink_status(sink);
        status != ON1X_SDK_OK)
        return status;
    const auto revision = graph_revision(supplied);
    try {
        if (supplied.size() > max_nodes)
            fail(ON1X_SDK_LIMIT, "ON1X-SDK-BUILD-NODE-LIMIT",
                 "build graph node limit exceeded");
        std::map<std::string, GraphNodeTemp, std::less<>> unique;
        std::size_t edges = 0;
        for (auto node : supplied) {
            if (!graph_id(node.id) || !graph_kind(node.kind) ||
                !sha256_text(node.digest))
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-BUILD-NODE",
                     "invalid build node id, kind or input digest");
            std::sort(node.dependencies.begin(), node.dependencies.end());
            if (std::adjacent_find(node.dependencies.begin(), node.dependencies.end()) !=
                node.dependencies.end())
                fail(ON1X_SDK_DUPLICATE, "ON1X-SDK-BUILD-EDGE-DUPLICATE",
                     "duplicate dependency edge in node '" + node.id + "'");
            for (const auto& dependency : node.dependencies)
                if (!graph_id(dependency) || dependency == node.id)
                    fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-BUILD-EDGE",
                         "invalid or self dependency in node '" + node.id + "'");
            if (node.dependencies.size() > max_edges - std::min(max_edges, edges))
                fail(ON1X_SDK_LIMIT, "ON1X-SDK-BUILD-EDGE-LIMIT",
                     "build graph edge limit exceeded");
            edges += node.dependencies.size();
            auto [position, inserted] = unique.emplace(node.id, node);
            if (!inserted) {
                const auto& existing = position->second;
                if (existing.kind == node.kind && existing.digest == node.digest &&
                    existing.dependencies == node.dependencies)
                    fail(ON1X_SDK_DUPLICATE, "ON1X-SDK-BUILD-NODE-DUPLICATE",
                         "duplicate build node '" + node.id + "'");
                fail(ON1X_SDK_INVALID_ARGUMENT, "ON1X-SDK-BUILD-CONFLICT",
                     "conflicting descriptors for build node '" + node.id + "'");
            }
        }
        std::vector<GraphNodeTemp> nodes;
        nodes.reserve(unique.size());
        for (auto& [id, node] : unique) {
            (void)id;
            nodes.push_back(std::move(node));
        }
        const auto order = topological(nodes);
        const auto canonical = canonical_graph(nodes, order);
        if (canonical.size() > max_bytes)
            fail(ON1X_SDK_LIMIT, "ON1X-SDK-BUILD-BYTE-LIMIT",
                 "canonical build graph exceeds its byte limit");
        *output = copy_graph(nodes, order, canonical, max_bytes, *allocator);
        return ON1X_SDK_OK;
    } catch (const Failure& failure) {
        return emit(sink, failure, revision, 0);
    } catch (const std::bad_alloc&) {
        return emit(sink, {ON1X_SDK_LIMIT, "ON1X-SDK-PROJECT-ALLOCATION",
                           "temporary build graph allocation failed"},
                    revision, 0);
    } catch (...) {
        return emit(sink, {ON1X_SDK_ERROR, "ON1X-SDK-PROJECT-INTERNAL",
                           "unexpected build graph failure"},
                    revision, 0);
    }
}

} // namespace on1x::sdk::project

extern "C" On1xSDK_Status on1x_sdk_build_graph_create(
    const On1xSDK_BuildNodeDescriptor* descriptors, size_t node_count,
    const On1xSDK_BuildGraphLimits* limits,
    const On1xSDK_ProjectAllocator* allocator,
    const On1xSDK_ProjectDiagnosticSink* sink,
    On1xSDK_BuildGraph** output) {
    using namespace on1x::sdk::project;
    if (!output) return ON1X_SDK_INVALID_ARGUMENT;
    *output = nullptr;
    if ((node_count && !descriptors) || node_count > ON1X_SDK_BUILD_GRAPH_MAX_NODES)
        return node_count > ON1X_SDK_BUILD_GRAPH_MAX_NODES
                   ? ON1X_SDK_LIMIT
                   : ON1X_SDK_INVALID_ARGUMENT;
    std::size_t max_nodes = ON1X_SDK_BUILD_GRAPH_MAX_NODES;
    std::size_t max_edges = ON1X_SDK_BUILD_GRAPH_MAX_EDGES;
    std::size_t max_bytes = 4u * 1024u * 1024u;
    if (limits) {
        if (limits->abi_version != ON1X_SDK_PROJECT_ABI_VERSION)
            return ON1X_SDK_ABI_MISMATCH;
        if (limits->struct_size < sizeof(std::uint32_t) * 2)
            return ON1X_SDK_INVALID_ARGUMENT;
        if (limits->struct_size >=
                offsetof(On1xSDK_BuildGraphLimits, max_nodes) + sizeof(limits->max_nodes) &&
            limits->max_nodes)
            max_nodes = limits->max_nodes;
        if (limits->struct_size >=
                offsetof(On1xSDK_BuildGraphLimits, max_edges) + sizeof(limits->max_edges) &&
            limits->max_edges)
            max_edges = limits->max_edges;
        if (limits->struct_size >=
                offsetof(On1xSDK_BuildGraphLimits, max_copied_bytes) +
                    sizeof(limits->max_copied_bytes) &&
            limits->max_copied_bytes)
            max_bytes = limits->max_copied_bytes;
    }
    if (!max_nodes || max_nodes > ON1X_SDK_BUILD_GRAPH_MAX_NODES ||
        !max_edges || max_edges > ON1X_SDK_BUILD_GRAPH_MAX_EDGES ||
        !max_bytes || max_bytes > 64u * 1024u * 1024u)
        return ON1X_SDK_INVALID_ARGUMENT;
    if (node_count > max_nodes) return ON1X_SDK_LIMIT;
    try {
        std::vector<GraphNodeTemp> nodes;
        nodes.reserve(node_count);
        std::size_t copied_edges = 0;
        constexpr auto required =
            offsetof(On1xSDK_BuildNodeDescriptor, dependencies) +
            sizeof(On1xSDK_BuildNodeDescriptor::dependencies);
        for (std::size_t i = 0; i < node_count; ++i) {
            const auto& descriptor = descriptors[i];
            if (!on1x::sdk::project::descriptor(&descriptor, required))
                return descriptor.abi_version == ON1X_SDK_PROJECT_ABI_VERSION
                           ? ON1X_SDK_INVALID_ARGUMENT
                           : ON1X_SDK_ABI_MISMATCH;
            if (!valid_view(descriptor.id) || !valid_view(descriptor.kind) ||
                !valid_view(descriptor.input_sha256) ||
                (descriptor.dependency_count && !descriptor.dependencies))
                return ON1X_SDK_INVALID_ARGUMENT;
            if (descriptor.dependency_count > max_edges - copied_edges)
                return ON1X_SDK_LIMIT;
            copied_edges += descriptor.dependency_count;
            if (!graph_id(as_view(descriptor.id)) ||
                !graph_kind(as_view(descriptor.kind)) ||
                !sha256_text(as_view(descriptor.input_sha256)))
                return ON1X_SDK_INVALID_ARGUMENT;
            GraphNodeTemp node{std::string(as_view(descriptor.id)),
                               std::string(as_view(descriptor.kind)),
                               std::string(as_view(descriptor.input_sha256)), {}};
            node.dependencies.reserve(descriptor.dependency_count);
            for (std::size_t j = 0; j < descriptor.dependency_count; ++j) {
                if (!valid_view(descriptor.dependencies[j]) ||
                    !graph_id(as_view(descriptor.dependencies[j])))
                    return ON1X_SDK_INVALID_ARGUMENT;
                node.dependencies.emplace_back(as_view(descriptor.dependencies[j]));
            }
            nodes.push_back(std::move(node));
        }
        return create_graph(nodes, max_nodes, max_edges, max_bytes, allocator, sink,
                            output);
    } catch (const std::bad_alloc&) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" size_t on1x_sdk_build_graph_node_count(
    const On1xSDK_BuildGraph* graph) {
    using namespace on1x::sdk::project;
    return graph && graph->magic == graph_magic ? graph->node_count : 0;
}

extern "C" const On1xSDK_BuildNode* on1x_sdk_build_graph_node_at(
    const On1xSDK_BuildGraph* graph, size_t index) {
    using namespace on1x::sdk::project;
    return graph && graph->magic == graph_magic && index < graph->node_count
               ? &graph->nodes[index]
               : nullptr;
}

extern "C" On1xSDK_ProjectString on1x_sdk_build_graph_sha256(
    const On1xSDK_BuildGraph* graph) {
    using namespace on1x::sdk::project;
    return graph && graph->magic == graph_magic
               ? On1xSDK_ProjectString{graph->hash,
                                       ON1X_SDK_PROJECT_SHA256_TEXT_BYTES}
               : On1xSDK_ProjectString{};
}

extern "C" On1xSDK_Status on1x_sdk_reproducibility_describe(
    const On1xSDK_BuildGraph* graph,
    On1xSDK_ReproducibilityRecord* record) {
    using namespace on1x::sdk::project;
    if (!record) return ON1X_SDK_INVALID_ARGUMENT;
    if (record->abi_version != ON1X_SDK_PROJECT_ABI_VERSION)
        return ON1X_SDK_ABI_MISMATCH;
    if (record->struct_size < sizeof(On1xSDK_ReproducibilityRecord) ||
        !graph || graph->magic != graph_magic)
        return ON1X_SDK_INVALID_ARGUMENT;
    *record = {};
    record->abi_version = ON1X_SDK_PROJECT_ABI_VERSION;
    record->struct_size =
        static_cast<std::uint32_t>(sizeof(On1xSDK_ReproducibilityRecord));
    record->schema = view("on1x.build-graph/v1");
    record->graph_sha256 = {
        graph->hash, ON1X_SDK_PROJECT_SHA256_TEXT_BYTES};
    record->input_count = graph->node_count;
    return ON1X_SDK_OK;
}

extern "C" void on1x_sdk_build_graph_destroy(On1xSDK_BuildGraph* graph) {
    using namespace on1x::sdk::project;
    if (!graph || graph->magic != graph_magic) return;
    const auto allocator = graph->allocator;
    const auto size = graph->allocation_size;
    graph->magic = 0;
    release(allocator, graph, size);
}
