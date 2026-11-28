/*
 * Immutable adapters over the canonical On1x type universe. Type handles may
 * be retained and queried concurrently; every borrowed view requires a live
 * owned handle reference and owner context.
 */
#ifndef ON1X_SDK_TYPE_H
#define ON1X_SDK_TYPE_H

#include "On1xSDK-SemanticTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_Type On1xSDK_Type;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_TYPE_INVALID = 0,
    ON1X_SDK_TYPE_I64 = 1,
    ON1X_SDK_TYPE_BOOL = 2,
    ON1X_SDK_TYPE_STRING = 3,
    ON1X_SDK_TYPE_VOID = 4,
    ON1X_SDK_TYPE_JSON = 5,
    ON1X_SDK_TYPE_JSON_VIEW = 6,
    ON1X_SDK_TYPE_FUNCTION = 7,
    ON1X_SDK_TYPE_HOLE = 8,
    ON1X_SDK_TYPE_TUPLE = 9,
    ON1X_SDK_TYPE_GENERIC = 10,
    ON1X_SDK_TYPE_BOUND = 11,
    ON1X_SDK_TYPE_FORALL = 12,
    ON1X_SDK_TYPE_CODE = 13,
    ON1X_SDK_TYPE_SEXPR = 14,
    ON1X_SDK_TYPE_BUFFER = 15,
    ON1X_SDK_TYPE_F64 = 16,
    ON1X_SDK_TYPE_RECORD = 17,
    ON1X_SDK_TYPE_SUM = 18,
    ON1X_SDK_TYPE_ARRAY = 19,
    ON1X_SDK_TYPE_SLICE = 20,
    ON1X_SDK_TYPE_F32 = 21,
    ON1X_SDK_TYPE_U8 = 22,
    ON1X_SDK_TYPE_U16 = 23,
    ON1X_SDK_TYPE_U32 = 24,
    ON1X_SDK_TYPE_U64 = 25,
    ON1X_SDK_TYPE_NAMED = 26,
    ON1X_SDK_TYPE_FIBER = 27,
    ON1X_SDK_TYPE_CHANNEL = 28,
    ON1X_SDK_TYPE_SENDER = 29,
    ON1X_SDK_TYPE_RECEIVER = 30,
    ON1X_SDK_TYPE_CSP_GROUP = 31,
    ON1X_SDK_TYPE_EXTERNAL_FIBER = 32,
    /* Reserved legacy encodings; use typed SENDER/RECEIVER with U8 payload. */
    ON1X_SDK_TYPE_BYTE_SENDER = 33,
    ON1X_SDK_TYPE_BYTE_RECEIVER = 34,
    ON1X_SDK_TYPE_CHAR = 35,
    ON1X_SDK_TYPE_REGEX = 36,
    ON1X_SDK_TYPE_REGEX_CAPTURES = 37,
    ON1X_SDK_TYPE_OWN = 38
} On1xSDK_TypeKind;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_TYPE_CHILD_PARAMETER = 1,
    ON1X_SDK_TYPE_CHILD_RESULT = 2,
    ON1X_SDK_TYPE_CHILD_ELEMENT = 3,
    ON1X_SDK_TYPE_CHILD_ARGUMENT = 4,
    ON1X_SDK_TYPE_CHILD_BODY = 5,
    ON1X_SDK_TYPE_CHILD_OWNED = 6,
    ON1X_SDK_TYPE_CHILD_PAYLOAD = 7
} On1xSDK_TypeChildRole;

typedef uint64_t On1xSDK_TypeFlags;
enum {
    ON1X_SDK_TYPE_FLAG_HAS_HOLES = UINT64_C(1) << 0,
    ON1X_SDK_TYPE_FLAG_HAS_GENERICS = UINT64_C(1) << 1,
    ON1X_SDK_TYPE_FLAG_HAS_FORALLS = UINT64_C(1) << 2,
    ON1X_SDK_TYPE_FLAG_HAS_FUNCTION_STORAGE = UINT64_C(1) << 3,
    ON1X_SDK_TYPE_FLAG_MOVE_ONLY = UINT64_C(1) << 4,
    ON1X_SDK_TYPE_FLAG_CONTAINS_UNIQUE = UINT64_C(1) << 5,
    ON1X_SDK_TYPE_FLAG_NUMERIC = UINT64_C(1) << 6,
    ON1X_SDK_TYPE_FLAG_UNSIGNED = UINT64_C(1) << 7,
    ON1X_SDK_TYPE_FLAG_FLOAT = UINT64_C(1) << 8,
    ON1X_SDK_TYPE_FLAG_DATA = UINT64_C(1) << 9,
    ON1X_SDK_TYPE_FLAG_CSP = UINT64_C(1) << 10
};

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    size_t bound_variables;
    size_t max_depth;
    size_t max_nodes;
    uint32_t allow_holes;
    uint32_t allow_generics;
    uint32_t allow_named;
    uint32_t allow_foralls;
} On1xSDK_TypeFormOptions;

/*
 * Every construction below copies canonical immutable Type values. The supplied
 * identity must be CANDIDATE. Child handles must have the exact same owner,
 * generation and revision; otherwise construction fails closed. Expanded
 * candidate forms are capped by ON1X_SDK_IR_MAX_NODES and
 * ON1X_SDK_IR_MAX_DEPTH; exhaustion returns LIMIT.
 */
On1xSDK_Status on1x_sdk_type_create_atomic(
    const On1xSDK_IRIdentity *identity,
    On1xSDK_TypeKind kind,
    uint64_t variable,
    On1xSDK_Type **out_type);

On1xSDK_Status on1x_sdk_type_create_function(
    const On1xSDK_IRIdentity *identity,
    const On1xSDK_Type *const *parameters,
    size_t parameter_count,
    const On1xSDK_Type *result,
    On1xSDK_Type **out_type);

On1xSDK_Status on1x_sdk_type_create_tuple(
    const On1xSDK_IRIdentity *identity,
    const On1xSDK_Type *const *elements,
    size_t element_count,
    On1xSDK_Type **out_type);

On1xSDK_Status on1x_sdk_type_create_data(
    const On1xSDK_IRIdentity *identity,
    On1xSDK_TypeKind kind,
    const On1xSDK_Type *const *elements,
    const On1xSDK_IRStringView *labels,
    size_t element_count,
    size_t extent,
    On1xSDK_IRStringView nominal_name,
    const On1xSDK_Type *const *nominal_arguments,
    size_t nominal_argument_count,
    On1xSDK_Type **out_type);

On1xSDK_Status on1x_sdk_type_create_named(
    const On1xSDK_IRIdentity *identity,
    On1xSDK_IRStringView name,
    const On1xSDK_Type *const *arguments,
    size_t argument_count,
    On1xSDK_Type **out_type);

On1xSDK_Status on1x_sdk_type_create_own(
    const On1xSDK_IRIdentity *identity,
    const On1xSDK_Type *named_target,
    On1xSDK_Type **out_type);

On1xSDK_Status on1x_sdk_type_create_forall(
    const On1xSDK_IRIdentity *identity,
    size_t variable_count,
    const On1xSDK_Type *body,
    On1xSDK_Type **out_type);

On1xSDK_Status on1x_sdk_type_create_code(
    const On1xSDK_IRIdentity *identity,
    const On1xSDK_Type *body,
    On1xSDK_Type **out_type);

/*
 * Construct the canonical typed CSP forms. CHANNEL preserves exact endpoint
 * quotas; zero/zero is the source-level wildcard. SENDER and RECEIVER require
 * sender_count == receiver_count == 0 and represent one owned endpoint view.
 */
On1xSDK_Status on1x_sdk_type_create_csp(
    const On1xSDK_IRIdentity *identity,
    On1xSDK_TypeKind kind,
    const On1xSDK_Type *payload,
    size_t sender_count,
    size_t receiver_count,
    On1xSDK_Type **out_type);

void on1x_sdk_type_retain(On1xSDK_Type *type);
void on1x_sdk_type_release(On1xSDK_Type *type);

On1xSDK_Status on1x_sdk_type_identity(
    const On1xSDK_Type *type,
    On1xSDK_IRIdentity *out_identity);
On1xSDK_Status on1x_sdk_type_kind(
    const On1xSDK_Type *type,
    On1xSDK_TypeKind *out_kind);
On1xSDK_Status on1x_sdk_type_flags(
    const On1xSDK_Type *type,
    On1xSDK_TypeFlags *out_flags);
On1xSDK_Status on1x_sdk_type_variable(
    const On1xSDK_Type *type,
    uint64_t *out_variable);
On1xSDK_Status on1x_sdk_type_extent(
    const On1xSDK_Type *type,
    size_t *out_extent);
On1xSDK_Status on1x_sdk_type_csp_cardinality(
    const On1xSDK_Type *type,
    size_t *out_sender_count,
    size_t *out_receiver_count);
On1xSDK_Status on1x_sdk_type_nominal(
    const On1xSDK_Type *type,
    On1xSDK_IRStringView *out_name);
On1xSDK_Status on1x_sdk_type_label_count(
    const On1xSDK_Type *type,
    size_t *out_count);
On1xSDK_Status on1x_sdk_type_label(
    const On1xSDK_Type *type,
    size_t index,
    On1xSDK_IRStringView *out_label);
On1xSDK_Status on1x_sdk_type_child_count(
    const On1xSDK_Type *type,
    size_t *out_count);
/* Returns one owned reference in out_child. */
On1xSDK_Status on1x_sdk_type_child(
    const On1xSDK_Type *type,
    size_t index,
    On1xSDK_TypeChildRole *out_role,
    On1xSDK_Type **out_child);

/*
 * Writes the canonical diagnostic spelling without a terminating NUL.
 * out_required is always set on success or insufficient capacity. A NULL
 * buffer is valid only when capacity is zero.
 */
On1xSDK_Status on1x_sdk_type_spelling(
    const On1xSDK_Type *type,
    char *buffer,
    size_t capacity,
    size_t *out_required);

/*
 * Structural form checking never upgrades validation state. Malformed forms
 * return ERROR; deterministic budget exhaustion returns LIMIT.
 */
On1xSDK_Status on1x_sdk_type_verify_form(
    const On1xSDK_Type *type,
    const On1xSDK_TypeFormOptions *options,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics);

On1xSDK_Status on1x_sdk_type_equal(
    const On1xSDK_Type *left,
    const On1xSDK_Type *right,
    uint32_t *out_equal);
On1xSDK_Status on1x_sdk_type_forall_instance(
    const On1xSDK_Type *schema,
    const On1xSDK_Type *instance,
    uint32_t *out_matches);

#ifndef __cplusplus
_Static_assert(sizeof(On1xSDK_TypeKind) == sizeof(uint32_t),
               "SDK requires 32-bit enums");
_Static_assert(sizeof(On1xSDK_TypeChildRole) == sizeof(uint32_t),
               "SDK requires 32-bit enums");
#endif

#ifdef __cplusplus
}
#endif

#endif
