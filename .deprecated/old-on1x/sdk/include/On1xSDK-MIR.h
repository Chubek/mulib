/*
 * Structured immutable views over canonical scalar/numeric On1x MIR.
 * Snapshots may be retained and read concurrently. References and returned
 * string/span views are borrowed from the exact live snapshot revision.
 */
#ifndef ON1X_SDK_MIR_H
#define ON1X_SDK_MIR_H

#include "On1xSDK-Type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_MIRSnapshot On1xSDK_MIRSnapshot;
typedef struct On1xSDK_MIREvaluation On1xSDK_MIREvaluation;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_MIR_CONSTANT = 0,
    ON1X_SDK_MIR_ADD = 1,
    ON1X_SDK_MIR_SUB = 2,
    ON1X_SDK_MIR_MUL = 3,
    ON1X_SDK_MIR_DIV = 4,
    ON1X_SDK_MIR_MOD = 5,
    ON1X_SDK_MIR_NEG = 6,
    ON1X_SDK_MIR_NOT = 7,
    ON1X_SDK_MIR_EQ = 8,
    ON1X_SDK_MIR_NE = 9,
    ON1X_SDK_MIR_LT = 10,
    ON1X_SDK_MIR_LE = 11,
    ON1X_SDK_MIR_GT = 12,
    ON1X_SDK_MIR_GE = 13,
    ON1X_SDK_MIR_LOAD = 14,
    ON1X_SDK_MIR_STORE = 15,
    ON1X_SDK_MIR_CALL = 16,
    ON1X_SDK_MIR_PRINT = 17,
    ON1X_SDK_MIR_ASSERT = 18,
    ON1X_SDK_MIR_F64_ADD = 19,
    ON1X_SDK_MIR_F64_SUB = 20,
    ON1X_SDK_MIR_F64_MUL = 21,
    ON1X_SDK_MIR_F64_DIV = 22,
    ON1X_SDK_MIR_F64_NEG = 23,
    ON1X_SDK_MIR_F64_EQ = 24,
    ON1X_SDK_MIR_F64_NE = 25,
    ON1X_SDK_MIR_F64_LT = 26,
    ON1X_SDK_MIR_F64_LE = 27,
    ON1X_SDK_MIR_F64_GT = 28,
    ON1X_SDK_MIR_F64_GE = 29,
    ON1X_SDK_MIR_F64_FROM_I64 = 30,
    ON1X_SDK_MIR_F64_TO_I64 = 31,
    ON1X_SDK_MIR_F64_FROM_BITS = 32,
    ON1X_SDK_MIR_F64_TO_BITS = 33,
    ON1X_SDK_MIR_F64_IS_NAN = 34,
    ON1X_SDK_MIR_F64_IS_FINITE = 35,
    ON1X_SDK_MIR_F64_SIGNBIT = 36,
    ON1X_SDK_MIR_TUPLE = 37,
    ON1X_SDK_MIR_EXTRACT = 38,
    ON1X_SDK_MIR_NUMERIC = 39
} On1xSDK_MIROp;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_MIR_EXIT_INVALID = 0,
    ON1X_SDK_MIR_EXIT_RETURN = 1,
    ON1X_SDK_MIR_EXIT_JUMP = 2,
    ON1X_SDK_MIR_EXIT_BRANCH = 3
} On1xSDK_MIRExit;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_MIR_EFFECT_PURE = 0,
    ON1X_SDK_MIR_EFFECT_TRAP = 1,
    ON1X_SDK_MIR_EFFECT_MEMORY = 2,
    ON1X_SDK_MIR_EFFECT_CALL = 3,
    ON1X_SDK_MIR_EFFECT_IO = 4
} On1xSDK_MIREffect;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_MIR_REF_FUNCTION = 1,
    ON1X_SDK_MIR_REF_BLOCK = 2,
    ON1X_SDK_MIR_REF_INSTRUCTION = 3
} On1xSDK_MIRRefKind;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    const void *snapshot_cookie;
    const On1xSDK_Context *owner;
    uint64_t generation;
    uint64_t revision;
    On1xSDK_MIRRefKind kind;
    uint32_t reserved;
    uint64_t id;
    size_t function_index;
    size_t block_index;
    size_t instruction_index;
} On1xSDK_MIRRef;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t id;
    On1xSDK_MIRExit kind;
    uint32_t reserved;
    uint64_t value;
    uint64_t effect;
    size_t edge_count;
    On1xSDK_IRSourceSpan span;
} On1xSDK_MIRTerminator;

typedef struct {
    uint64_t block;
    size_t argument_count;
} On1xSDK_MIREdge;

/* Portable metadata, not a backend selection, capability grant, or proof.
 * Initialize abi_version to ON1X_SDK_SEMANTICS_IR_ABI_VERSION and struct_size
 * to sizeof(On1xSDK_MIRDirectiveView). Strings are borrowed from the snapshot. */
typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_IRStringView backend;
    On1xSDK_IRStringView target;
    On1xSDK_IRStringView kind;  /* "hint" or "rewrite" */
    On1xSDK_IRStringView action;
    uint64_t function;         /* Canonical MIR function ID, or 0 for module scope */
    uint32_t speculative;      /* 0 or 1; does not enable speculation */
    uint32_t reserved;
} On1xSDK_MIRDirectiveView;

void on1x_sdk_mir_snapshot_retain(On1xSDK_MIRSnapshot *snapshot);
void on1x_sdk_mir_snapshot_release(On1xSDK_MIRSnapshot *snapshot);

On1xSDK_Status on1x_sdk_mir_snapshot_identity(
    const On1xSDK_MIRSnapshot *snapshot,
    On1xSDK_IRIdentity *out_identity);
On1xSDK_Status on1x_sdk_mir_snapshot_provenance(
    const On1xSDK_MIRSnapshot *snapshot,
    On1xSDK_IRStringView *out_provenance);
On1xSDK_Status on1x_sdk_mir_snapshot_format_version(
    const On1xSDK_MIRSnapshot *snapshot,
    uint32_t *out_version);
On1xSDK_Status on1x_sdk_mir_snapshot_module_id(
    const On1xSDK_MIRSnapshot *snapshot,
    uint64_t *out_id);
/* Available for candidate and verified snapshots, in portable MIR order.
 * Older canonical MIR versions have zero directives. */
On1xSDK_Status on1x_sdk_mir_snapshot_directive_count(
    const On1xSDK_MIRSnapshot *snapshot,
    size_t *out_count);
On1xSDK_Status on1x_sdk_mir_snapshot_directive(
    const On1xSDK_MIRSnapshot *snapshot,
    size_t index,
    On1xSDK_MIRDirectiveView *out_directive);
On1xSDK_Status on1x_sdk_mir_snapshot_function_count(
    const On1xSDK_MIRSnapshot *snapshot,
    size_t *out_count);
On1xSDK_Status on1x_sdk_mir_snapshot_function(
    const On1xSDK_MIRSnapshot *snapshot,
    size_t index,
    On1xSDK_MIRRef *out_function);

On1xSDK_Status on1x_sdk_mir_function_name(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *function,
    On1xSDK_IRStringView *out_name);
On1xSDK_Status on1x_sdk_mir_function_result(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *function,
    On1xSDK_Type **out_type);
On1xSDK_Status on1x_sdk_mir_function_slot_count(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *function,
    size_t *out_count);
On1xSDK_Status on1x_sdk_mir_function_slot(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *function,
    size_t index,
    uint64_t *out_id,
    On1xSDK_Type **out_type,
    On1xSDK_IRSourceSpan *out_span);
On1xSDK_Status on1x_sdk_mir_function_block_count(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *function,
    size_t *out_count);
On1xSDK_Status on1x_sdk_mir_function_block(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *function,
    size_t index,
    On1xSDK_MIRRef *out_block);

On1xSDK_Status on1x_sdk_mir_block_effect(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *block,
    uint64_t *out_effect);
On1xSDK_Status on1x_sdk_mir_block_parameter_count(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *block,
    size_t *out_count);
On1xSDK_Status on1x_sdk_mir_block_parameter(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *block,
    size_t index,
    uint64_t *out_id,
    On1xSDK_Type **out_type,
    On1xSDK_IRSourceSpan *out_span);
On1xSDK_Status on1x_sdk_mir_block_instruction_count(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *block,
    size_t *out_count);
On1xSDK_Status on1x_sdk_mir_block_instruction(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *block,
    size_t index,
    On1xSDK_MIRRef *out_instruction);
On1xSDK_Status on1x_sdk_mir_block_terminator(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *block,
    On1xSDK_MIRTerminator *out_terminator);
On1xSDK_Status on1x_sdk_mir_terminator_edge(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *block,
    size_t edge_index,
    On1xSDK_MIREdge *out_edge);
On1xSDK_Status on1x_sdk_mir_terminator_edge_argument(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *block,
    size_t edge_index,
    size_t argument_index,
    uint64_t *out_value);

On1xSDK_Status on1x_sdk_mir_instruction_op(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *instruction,
    On1xSDK_MIROp *out_op);
On1xSDK_Status on1x_sdk_mir_instruction_effect(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *instruction,
    On1xSDK_MIREffect *out_effect);
On1xSDK_Status on1x_sdk_mir_instruction_type(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *instruction,
    On1xSDK_Type **out_type);
On1xSDK_Status on1x_sdk_mir_instruction_immediate(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *instruction,
    int64_t *out_immediate);
On1xSDK_Status on1x_sdk_mir_instruction_target(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *instruction,
    uint64_t *out_target);
On1xSDK_Status on1x_sdk_mir_instruction_effect_chain(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *instruction,
    uint64_t *out_effect_in,
    uint64_t *out_effect_out);
On1xSDK_Status on1x_sdk_mir_instruction_primitive(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *instruction,
    On1xSDK_IRStringView *out_primitive);
On1xSDK_Status on1x_sdk_mir_instruction_span(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *instruction,
    On1xSDK_IRSourceSpan *out_span);
On1xSDK_Status on1x_sdk_mir_instruction_operand_count(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *instruction,
    size_t *out_count);
On1xSDK_Status on1x_sdk_mir_instruction_operand(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *instruction,
    size_t index,
    uint64_t *out_value);

typedef On1xSDK_Status (*On1xSDK_MIRVisitInstructionFn)(
    void *userdata,
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *instruction);

On1xSDK_Status on1x_sdk_mir_visit_instructions(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_IRLimits *limits,
    On1xSDK_MIRVisitInstructionFn visitor,
    void *userdata,
    On1xSDK_IRFailure *out_failure);

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_IRStringView function;
    const int64_t *arguments;
    size_t argument_count;
    size_t max_steps;
    size_t max_call_depth;
} On1xSDK_MIREvaluateOptions;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_MIR_EVALUATION_RETURNED = 1,
    ON1X_SDK_MIR_EVALUATION_TRAPPED = 2,
    ON1X_SDK_MIR_EVALUATION_BUDGET_EXHAUSTED = 3
} On1xSDK_MIREvaluationStatus;

/* Evaluation accepts VERIFIED_MIR only and never executes foreign code. */
On1xSDK_Status on1x_sdk_mir_evaluate(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIREvaluateOptions *options,
    On1xSDK_MIREvaluation **out_evaluation);
void on1x_sdk_mir_evaluation_release(On1xSDK_MIREvaluation *evaluation);
On1xSDK_Status on1x_sdk_mir_evaluation_status(
    const On1xSDK_MIREvaluation *evaluation,
    On1xSDK_MIREvaluationStatus *out_status);
On1xSDK_Status on1x_sdk_mir_evaluation_type(
    const On1xSDK_MIREvaluation *evaluation,
    On1xSDK_Type **out_type);
/* Available only for RETURNED non-tuple results. */
On1xSDK_Status on1x_sdk_mir_evaluation_scalar(
    const On1xSDK_MIREvaluation *evaluation,
    int64_t *out_scalar);
On1xSDK_Status on1x_sdk_mir_evaluation_output(
    const On1xSDK_MIREvaluation *evaluation,
    On1xSDK_IRStringView *out_output);
On1xSDK_Status on1x_sdk_mir_evaluation_diagnostic(
    const On1xSDK_MIREvaluation *evaluation,
    On1xSDK_IRStringView *out_diagnostic);
On1xSDK_Status on1x_sdk_mir_evaluation_steps(
    const On1xSDK_MIREvaluation *evaluation,
    size_t *out_steps);

#ifndef __cplusplus
_Static_assert(sizeof(On1xSDK_MIROp) == sizeof(uint32_t),
               "SDK requires 32-bit enums");
_Static_assert(sizeof(On1xSDK_MIRExit) == sizeof(uint32_t),
               "SDK requires 32-bit enums");
_Static_assert(sizeof(On1xSDK_MIREffect) == sizeof(uint32_t),
               "SDK requires 32-bit enums");
_Static_assert(sizeof(On1xSDK_MIRRefKind) == sizeof(uint32_t),
               "SDK requires 32-bit enums");
_Static_assert(sizeof(On1xSDK_MIREvaluationStatus) == sizeof(uint32_t),
               "SDK requires 32-bit enums");
#endif

#ifdef __cplusplus
}
#endif

#endif
