/*
 * Immutable, revision-bound views of canonical or wire HIR snapshots.
 * Snapshots may be retained and read concurrently. Node and string views are
 * borrowed and remain valid only while that exact snapshot revision is live.
 */
#ifndef ON1X_SDK_HIR_H
#define ON1X_SDK_HIR_H

#include "On1xSDK-Type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_HIRSnapshot On1xSDK_HIRSnapshot;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_HIR_NODE_FUNCTION = 1,
    ON1X_SDK_HIR_NODE_PARAMETER = 2,
    ON1X_SDK_HIR_NODE_STATEMENT = 3,
    ON1X_SDK_HIR_NODE_EXPRESSION = 4,
    ON1X_SDK_HIR_NODE_PATTERN = 5,
    ON1X_SDK_HIR_NODE_CAPTURE = 6
} On1xSDK_HIRNodeClass;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_HIR_EDGE_PARAMETER = 1,
    ON1X_SDK_HIR_EDGE_BODY = 2,
    ON1X_SDK_HIR_EDGE_ATTRIBUTE = 3,
    ON1X_SDK_HIR_EDGE_PATTERN = 4,
    ON1X_SDK_HIR_EDGE_VALUE = 5,
    ON1X_SDK_HIR_EDGE_ELSE = 6,
    ON1X_SDK_HIR_EDGE_INSTANCE = 7,
    ON1X_SDK_HIR_EDGE_CALLEE = 8,
    ON1X_SDK_HIR_EDGE_OPERAND = 9,
    ON1X_SDK_HIR_EDGE_PROOF = 10,
    ON1X_SDK_HIR_EDGE_SYNTAX_TAIL = 11,
    ON1X_SDK_HIR_EDGE_ELEMENT = 12,
    ON1X_SDK_HIR_EDGE_CAPTURE = 13
} On1xSDK_HIREdgeKind;

/*
 * kind is class-specific and schema-versioned. For statement, expression and
 * pattern nodes it is the zero-based canonical enum value retained by HIR-v1.
 */
typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    const void *snapshot_cookie;
    const On1xSDK_Context *owner;
    uint64_t generation;
    uint64_t revision;
    uint64_t id;
} On1xSDK_HIRNodeRef;

typedef uint64_t On1xSDK_HIRNodeFlags;
enum {
    ON1X_SDK_HIR_FLAG_EXTERNAL = UINT64_C(1) << 0,
    ON1X_SDK_HIR_FLAG_PROTOTYPE = UINT64_C(1) << 1,
    ON1X_SDK_HIR_FLAG_MUTABLE = UINT64_C(1) << 2,
    /* Exclusive borrow expression, 'ref' parameter, or ':=' statement. */
    ON1X_SDK_HIR_FLAG_EXCLUSIVE = UINT64_C(1) << 3,
    ON1X_SDK_HIR_FLAG_UNSAFE = UINT64_C(1) << 4,
    ON1X_SDK_HIR_FLAG_MOVE_OUT = UINT64_C(1) << 5,
    ON1X_SDK_HIR_FLAG_NO_RETURN = UINT64_C(1) << 6,
    ON1X_SDK_HIR_FLAG_OPEN_PATTERN = UINT64_C(1) << 7,
    ON1X_SDK_HIR_FLAG_EXTENSION_GENERATED = UINT64_C(1) << 8,
    ON1X_SDK_HIR_FLAG_STATEMENT_MATCH = UINT64_C(1) << 9,
    ON1X_SDK_HIR_FLAG_SUM_MATCH = UINT64_C(1) << 10,
    /* Shared borrow expression or 'borrow' parameter, never with EXCLUSIVE.
       The node's type is its referent type, not a reference/pointer type. */
    ON1X_SDK_HIR_FLAG_SHARED = UINT64_C(1) << 11
};

void on1x_sdk_hir_snapshot_retain(On1xSDK_HIRSnapshot *snapshot);
void on1x_sdk_hir_snapshot_release(On1xSDK_HIRSnapshot *snapshot);

On1xSDK_Status on1x_sdk_hir_snapshot_identity(
    const On1xSDK_HIRSnapshot *snapshot,
    On1xSDK_IRIdentity *out_identity);
On1xSDK_Status on1x_sdk_hir_snapshot_provenance(
    const On1xSDK_HIRSnapshot *snapshot,
    On1xSDK_IRStringView *out_provenance);
On1xSDK_Status on1x_sdk_hir_snapshot_node_count(
    const On1xSDK_HIRSnapshot *snapshot,
    size_t *out_count);
On1xSDK_Status on1x_sdk_hir_snapshot_function_count(
    const On1xSDK_HIRSnapshot *snapshot,
    size_t *out_count);
On1xSDK_Status on1x_sdk_hir_snapshot_function(
    const On1xSDK_HIRSnapshot *snapshot,
    size_t index,
    On1xSDK_HIRNodeRef *out_function);

On1xSDK_Status on1x_sdk_hir_node_class(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *node,
    On1xSDK_HIRNodeClass *out_class);
On1xSDK_Status on1x_sdk_hir_node_kind(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *node,
    uint32_t *out_kind);
On1xSDK_Status on1x_sdk_hir_node_flags(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *node,
    On1xSDK_HIRNodeFlags *out_flags);
On1xSDK_Status on1x_sdk_hir_node_name(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *node,
    On1xSDK_IRStringView *out_name);
On1xSDK_Status on1x_sdk_hir_node_span(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *node,
    On1xSDK_IRSourceSpan *out_span);
/* Returns one owned type reference. INVALID/absent annotations are UNSUPPORTED. */
On1xSDK_Status on1x_sdk_hir_node_type(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *node,
    On1xSDK_Type **out_type);
On1xSDK_Status on1x_sdk_hir_node_child_count(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *node,
    size_t *out_count);
On1xSDK_Status on1x_sdk_hir_node_child(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *node,
    size_t index,
    On1xSDK_HIREdgeKind *out_edge,
    On1xSDK_HIRNodeRef *out_child);

/*
 * Visitors are synchronous and receive borrowed views. Unknown callback status
 * values become ERROR. A callback failure stops traversal and is reported as
 * IR_FAILURE_CALLBACK.
 */
typedef On1xSDK_Status (*On1xSDK_HIRVisitFn)(
    void *userdata,
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *node,
    size_t depth);

On1xSDK_Status on1x_sdk_hir_visit(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_IRLimits *limits,
    On1xSDK_HIRVisitFn visitor,
    void *userdata,
    On1xSDK_IRFailure *out_failure);

/*
 * This validates the flattened schema only and never upgrades CANDIDATE to
 * CHECKED_HIR. Canonical semantic admission is a separate host operation.
 */
On1xSDK_Status on1x_sdk_hir_verify_structure(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_IRLimits *limits,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics);

#ifndef __cplusplus
_Static_assert(sizeof(On1xSDK_HIRNodeClass) == sizeof(uint32_t),
               "SDK requires 32-bit enums");
_Static_assert(sizeof(On1xSDK_HIREdgeKind) == sizeof(uint32_t),
               "SDK requires 32-bit enums");
#endif

#ifdef __cplusplus
}
#endif

#endif
