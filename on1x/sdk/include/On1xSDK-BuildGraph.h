/* Deterministic copied build graphs with dependency-before-consumer order. */
#ifndef ON1X_SDK_BUILD_GRAPH_H
#define ON1X_SDK_BUILD_GRAPH_H

#include "On1xSDK-Project.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_BUILD_GRAPH_MAX_NODES 1024u
#define ON1X_SDK_BUILD_GRAPH_MAX_EDGES 8192u

typedef struct On1xSDK_BuildGraph On1xSDK_BuildGraph;

typedef struct On1xSDK_BuildNodeDescriptor {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_ProjectString id;
    On1xSDK_ProjectString kind;
    On1xSDK_ProjectString input_sha256; /* Exactly 64 lowercase hexadecimal bytes. */
    size_t dependency_count;
    const On1xSDK_ProjectString *dependencies;
} On1xSDK_BuildNodeDescriptor;

typedef struct On1xSDK_BuildGraphLimits {
    uint32_t abi_version;
    uint32_t struct_size;
    size_t max_nodes; /* Zero selects BUILD_GRAPH_MAX_NODES. */
    size_t max_edges; /* Zero selects BUILD_GRAPH_MAX_EDGES. */
    size_t max_copied_bytes; /* Zero selects 4 MiB. */
} On1xSDK_BuildGraphLimits;

typedef struct On1xSDK_BuildNode {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_ProjectString id;
    On1xSDK_ProjectString kind;
    On1xSDK_ProjectString input_sha256;
    size_t dependency_count;
    const On1xSDK_ProjectString *dependencies;
    size_t topological_index;
} On1xSDK_BuildNode;

/*
 * Copies and validates all descriptors transactionally.  Node IDs and each
 * dependency list are unique.  Missing dependencies, conflicting duplicate
 * IDs, and cycles are distinct diagnostic codes.  Ordering is deterministic:
 * all providers precede consumers and otherwise IDs use unsigned byte order.
 * On success one owned graph is returned.  Accessor views are borrowed and
 * immutable until destroy; on failure *out_graph is NULL.
 */
ON1X_SDK_PROJECT_API On1xSDK_Status on1x_sdk_build_graph_create(
    const On1xSDK_BuildNodeDescriptor *nodes, size_t node_count,
    const On1xSDK_BuildGraphLimits *limits,
    const On1xSDK_ProjectAllocator *allocator,
    const On1xSDK_ProjectDiagnosticSink *sink,
    On1xSDK_BuildGraph **out_graph);

ON1X_SDK_PROJECT_API size_t on1x_sdk_build_graph_node_count(
    const On1xSDK_BuildGraph *graph);
ON1X_SDK_PROJECT_API const On1xSDK_BuildNode *on1x_sdk_build_graph_node_at(
    const On1xSDK_BuildGraph *graph, size_t topological_index);
ON1X_SDK_PROJECT_API On1xSDK_ProjectString on1x_sdk_build_graph_sha256(
    const On1xSDK_BuildGraph *graph); /* Borrowed 64-byte lowercase hexadecimal view. */
ON1X_SDK_PROJECT_API void on1x_sdk_build_graph_destroy(
    On1xSDK_BuildGraph *graph);

#ifdef __cplusplus
}
#endif

#endif
