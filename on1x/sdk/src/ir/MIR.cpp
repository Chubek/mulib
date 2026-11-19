#include "CanonicalBridge.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace on1x::sdk::ir {

static_assert(static_cast<unsigned>(mir::Op::Constant) ==
              ON1X_SDK_MIR_CONSTANT);
static_assert(static_cast<unsigned>(mir::Op::Numeric) ==
              ON1X_SDK_MIR_NUMERIC);
static_assert(static_cast<unsigned>(mir::Exit::Branch) ==
              ON1X_SDK_MIR_EXIT_BRANCH);
static_assert(static_cast<unsigned>(mir::Effect::IO) ==
              ON1X_SDK_MIR_EFFECT_IO);

On1xSDK_Status admit_mir(
    const mir::Module &candidate,
    const On1xSDK_IRIdentity &identity_record,
    std::string_view provenance_view,
    On1xSDK_MIRSnapshot **out,
    On1xSDK_IRFailure *failure,
    const On1xSDK_DiagnosticSink *diagnostics) noexcept {
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = nullptr;
    set_failure(failure, ON1X_SDK_IR_FAILURE_NONE);
    Identity identity;
    if (const auto status = read_identity(
            &identity_record, ON1X_SDK_IR_CANDIDATE, identity);
        status != ON1X_SDK_OK) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return status;
    }
    std::string provenance;
    if (const auto status = copy_text(
            {provenance_view.data(), provenance_view.size()},
            ON1X_SDK_IR_MAX_PROVENANCE_BYTES, false, provenance);
        status != ON1X_SDK_OK) {
        set_failure(failure, status == ON1X_SDK_LIMIT
                                 ? ON1X_SDK_IR_FAILURE_LIMIT
                                 : ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return status;
    }
    if (const auto status =
            verify_module(candidate, nullptr, failure, diagnostics);
        status != ON1X_SDK_OK)
        return status;
    const auto status = guard([&] {
        identity.state = ON1X_SDK_IR_VERIFIED_MIR;
        *out = make_mir_snapshot(identity, std::move(provenance), candidate);
        return ON1X_SDK_OK;
    });
    if (status != ON1X_SDK_OK)
        set_failure(failure, status == ON1X_SDK_LIMIT
                                 ? ON1X_SDK_IR_FAILURE_LIMIT
                                 : ON1X_SDK_IR_FAILURE_MALFORMED);
    return status;
}

namespace {

On1xSDK_Status value(const Identity &identity,
                     const mir::Value &value,
                     std::uint64_t *out_id,
                     On1xSDK_Type **out_type,
                     On1xSDK_IRSourceSpan *out_span) noexcept {
    if (!out_id || !out_type || !out_span)
        return ON1X_SDK_INVALID_ARGUMENT;
    *out_type = nullptr;
    return guard([&] {
        *out_id = value.id;
        *out_span = span(value.where);
        *out_type = make_type(identity, value.type);
        return ON1X_SDK_OK;
    });
}

struct Prefix {
    std::uint32_t abi_version;
    std::uint32_t struct_size;
};

On1xSDK_Status evaluation_options(
    const On1xSDK_MIREvaluateOptions *source,
    std::string &function,
    const std::int64_t *&arguments,
    std::size_t &argument_count,
    std::size_t &steps,
    std::size_t &depth) noexcept {
    if (!source) return ON1X_SDK_INVALID_ARGUMENT;
    Prefix prefix{};
    std::memcpy(&prefix, source, sizeof prefix);
    if (prefix.abi_version != ON1X_SDK_SEMANTICS_IR_ABI_VERSION ||
        prefix.struct_size < sizeof(Prefix))
        return ON1X_SDK_ABI_MISMATCH;
    On1xSDK_MIREvaluateOptions copy{};
    std::memcpy(&copy, source,
                 std::min<std::size_t>(prefix.struct_size, sizeof copy));
    if (const auto status = copy_text(
            {"main", 4}, ON1X_SDK_IR_MAX_STRING_BYTES, false, function);
        status != ON1X_SDK_OK)
        return status;
    arguments = nullptr;
    argument_count = 0;
    steps = 1000000;
    depth = 256;
#define ON1X_MIR_EVAL_HAS(field) \
    (prefix.struct_size >= offsetof(On1xSDK_MIREvaluateOptions, field) + \
                           sizeof copy.field)
    if (ON1X_MIR_EVAL_HAS(function) && copy.function.size) {
        if (const auto status = copy_text(
                copy.function, ON1X_SDK_IR_MAX_STRING_BYTES, false, function);
            status != ON1X_SDK_OK)
            return status;
    }
    if (ON1X_MIR_EVAL_HAS(arguments)) arguments = copy.arguments;
    if (ON1X_MIR_EVAL_HAS(argument_count))
        argument_count = copy.argument_count;
    if (ON1X_MIR_EVAL_HAS(max_steps)) steps = copy.max_steps;
    if (ON1X_MIR_EVAL_HAS(max_call_depth)) depth = copy.max_call_depth;
#undef ON1X_MIR_EVAL_HAS
    if ((argument_count && !arguments) || argument_count > 65536 ||
        !steps || !depth || depth > ON1X_SDK_IR_MAX_DEPTH)
        return ON1X_SDK_INVALID_ARGUMENT;
    return ON1X_SDK_OK;
}

} // namespace
} // namespace on1x::sdk::ir

extern "C" {

void on1x_sdk_mir_snapshot_retain(On1xSDK_MIRSnapshot *snapshot) {
    if (!snapshot) return;
    auto current = snapshot->references.load(std::memory_order_relaxed);
    while (current != std::numeric_limits<std::size_t>::max() &&
           !snapshot->references.compare_exchange_weak(
               current, current + 1, std::memory_order_relaxed)) {}
}

void on1x_sdk_mir_snapshot_release(On1xSDK_MIRSnapshot *snapshot) {
    if (snapshot &&
        snapshot->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete snapshot;
}

On1xSDK_Status on1x_sdk_mir_snapshot_identity(
    const On1xSDK_MIRSnapshot *snapshot,
    On1xSDK_IRIdentity *out_identity) {
    if (!snapshot || !out_identity) return ON1X_SDK_INVALID_ARGUMENT;
    on1x::sdk::ir::write_identity(snapshot->identity, *out_identity);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_snapshot_provenance(
    const On1xSDK_MIRSnapshot *snapshot,
    On1xSDK_IRStringView *out_provenance) {
    if (!snapshot || !out_provenance) return ON1X_SDK_INVALID_ARGUMENT;
    *out_provenance = {snapshot->provenance.data(),
                       snapshot->provenance.size()};
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_snapshot_format_version(
    const On1xSDK_MIRSnapshot *snapshot,
    uint32_t *out_version) {
    if (!snapshot || !out_version) return ON1X_SDK_INVALID_ARGUMENT;
    *out_version = snapshot->module.version;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_snapshot_module_id(
    const On1xSDK_MIRSnapshot *snapshot,
    uint64_t *out_id) {
    if (!snapshot || !out_id) return ON1X_SDK_INVALID_ARGUMENT;
    *out_id = snapshot->module.id;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_snapshot_function_count(
    const On1xSDK_MIRSnapshot *snapshot,
    size_t *out_count) {
    if (!snapshot || !out_count) return ON1X_SDK_INVALID_ARGUMENT;
    *out_count = snapshot->module.functions.size();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_snapshot_function(
    const On1xSDK_MIRSnapshot *snapshot,
    size_t index,
    On1xSDK_MIRRef *out_function) {
    if (!snapshot || !out_function) return ON1X_SDK_INVALID_ARGUMENT;
    if (index >= snapshot->module.functions.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto &function = snapshot->module.functions[index];
    *out_function = on1x::sdk::ir::mir_ref(
        *snapshot, ON1X_SDK_MIR_REF_FUNCTION, index, 0, 0, function.id);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_function_name(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    On1xSDK_IRStringView *out_name) {
    if (!out_name) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Function *function = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_function(snapshot, reference, function);
        status != ON1X_SDK_OK)
        return status;
    *out_name = {function->name.data(), function->name.size()};
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_function_result(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    On1xSDK_Type **out_type) {
    if (!out_type) return ON1X_SDK_INVALID_ARGUMENT;
    *out_type = nullptr;
    const on1x::mir::Function *function = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_function(snapshot, reference, function);
        status != ON1X_SDK_OK)
        return status;
    return on1x::sdk::ir::guard([&] {
        *out_type =
            on1x::sdk::ir::make_type(snapshot->identity, function->result);
        return ON1X_SDK_OK;
    });
}

On1xSDK_Status on1x_sdk_mir_function_slot_count(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    size_t *out_count) {
    if (!out_count) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Function *function = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_function(snapshot, reference, function);
        status != ON1X_SDK_OK)
        return status;
    *out_count = function->slots.size();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_function_slot(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    size_t index,
    uint64_t *out_id,
    On1xSDK_Type **out_type,
    On1xSDK_IRSourceSpan *out_span) {
    const on1x::mir::Function *function = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_function(snapshot, reference, function);
        status != ON1X_SDK_OK)
        return status;
    if (index >= function->slots.size()) return ON1X_SDK_INVALID_ARGUMENT;
    return on1x::sdk::ir::value(snapshot->identity, function->slots[index],
                                out_id, out_type, out_span);
}

On1xSDK_Status on1x_sdk_mir_function_block_count(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    size_t *out_count) {
    if (!out_count) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Function *function = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_function(snapshot, reference, function);
        status != ON1X_SDK_OK)
        return status;
    *out_count = function->blocks.size();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_function_block(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    size_t index,
    On1xSDK_MIRRef *out_block) {
    if (!out_block) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Function *function = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_function(snapshot, reference, function);
        status != ON1X_SDK_OK)
        return status;
    if (index >= function->blocks.size()) return ON1X_SDK_INVALID_ARGUMENT;
    *out_block = on1x::sdk::ir::mir_ref(
        *snapshot, ON1X_SDK_MIR_REF_BLOCK, reference->function_index,
        index, 0, function->blocks[index].id);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_block_effect(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    uint64_t *out_effect) {
    if (!out_effect) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Block *block = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_block(snapshot, reference, block);
        status != ON1X_SDK_OK)
        return status;
    *out_effect = block->effect;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_block_parameter_count(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    size_t *out_count) {
    if (!out_count) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Block *block = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_block(snapshot, reference, block);
        status != ON1X_SDK_OK)
        return status;
    *out_count = block->parameters.size();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_block_parameter(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    size_t index,
    uint64_t *out_id,
    On1xSDK_Type **out_type,
    On1xSDK_IRSourceSpan *out_span) {
    const on1x::mir::Block *block = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_block(snapshot, reference, block);
        status != ON1X_SDK_OK)
        return status;
    if (index >= block->parameters.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    return on1x::sdk::ir::value(snapshot->identity, block->parameters[index],
                                out_id, out_type, out_span);
}

On1xSDK_Status on1x_sdk_mir_block_instruction_count(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    size_t *out_count) {
    if (!out_count) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Block *block = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_block(snapshot, reference, block);
        status != ON1X_SDK_OK)
        return status;
    *out_count = block->instructions.size();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_block_instruction(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    size_t index,
    On1xSDK_MIRRef *out_instruction) {
    if (!out_instruction) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Block *block = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_block(snapshot, reference, block);
        status != ON1X_SDK_OK)
        return status;
    if (index >= block->instructions.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    *out_instruction = on1x::sdk::ir::mir_ref(
        *snapshot, ON1X_SDK_MIR_REF_INSTRUCTION,
        reference->function_index, reference->block_index, index,
        block->instructions[index].id);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_block_terminator(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    On1xSDK_MIRTerminator *out_terminator) {
    if (!out_terminator) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Block *block = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_block(snapshot, reference, block);
        status != ON1X_SDK_OK)
        return status;
    const auto &exit = block->exit;
    *out_terminator = {
        ON1X_SDK_SEMANTICS_IR_ABI_VERSION,
        sizeof(On1xSDK_MIRTerminator),
        exit.id,
        static_cast<On1xSDK_MIRExit>(exit.kind),
        0,
        exit.value,
        exit.effect,
        exit.edges.size(),
        on1x::sdk::ir::span(exit.where)
    };
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_terminator_edge(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    size_t edge_index,
    On1xSDK_MIREdge *out_edge) {
    if (!out_edge) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Block *block = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_block(snapshot, reference, block);
        status != ON1X_SDK_OK)
        return status;
    if (edge_index >= block->exit.edges.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto &edge = block->exit.edges[edge_index];
    *out_edge = {edge.block, edge.arguments.size()};
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_terminator_edge_argument(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    size_t edge_index,
    size_t argument_index,
    uint64_t *out_value) {
    if (!out_value) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Block *block = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_block(snapshot, reference, block);
        status != ON1X_SDK_OK)
        return status;
    if (edge_index >= block->exit.edges.size() ||
        argument_index >= block->exit.edges[edge_index].arguments.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    *out_value = block->exit.edges[edge_index].arguments[argument_index];
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_instruction_op(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    On1xSDK_MIROp *out_op) {
    if (!out_op) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Instruction *instruction = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_instruction(snapshot, reference, instruction);
        status != ON1X_SDK_OK)
        return status;
    *out_op = static_cast<On1xSDK_MIROp>(instruction->op);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_instruction_effect(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    On1xSDK_MIREffect *out_effect) {
    if (!out_effect) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Instruction *instruction = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_instruction(snapshot, reference, instruction);
        status != ON1X_SDK_OK)
        return status;
    *out_effect =
        static_cast<On1xSDK_MIREffect>(on1x::mir::effect(instruction->op));
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_instruction_type(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    On1xSDK_Type **out_type) {
    if (!out_type) return ON1X_SDK_INVALID_ARGUMENT;
    *out_type = nullptr;
    const on1x::mir::Instruction *instruction = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_instruction(snapshot, reference, instruction);
        status != ON1X_SDK_OK)
        return status;
    return on1x::sdk::ir::guard([&] {
        *out_type =
            on1x::sdk::ir::make_type(snapshot->identity, instruction->type);
        return ON1X_SDK_OK;
    });
}

On1xSDK_Status on1x_sdk_mir_instruction_immediate(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    int64_t *out_immediate) {
    if (!out_immediate) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Instruction *instruction = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_instruction(snapshot, reference, instruction);
        status != ON1X_SDK_OK)
        return status;
    *out_immediate = instruction->immediate;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_instruction_target(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    uint64_t *out_target) {
    if (!out_target) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Instruction *instruction = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_instruction(snapshot, reference, instruction);
        status != ON1X_SDK_OK)
        return status;
    *out_target = instruction->target;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_instruction_effect_chain(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    uint64_t *out_effect_in,
    uint64_t *out_effect_out) {
    if (!out_effect_in || !out_effect_out)
        return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Instruction *instruction = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_instruction(snapshot, reference, instruction);
        status != ON1X_SDK_OK)
        return status;
    *out_effect_in = instruction->effect_in;
    *out_effect_out = instruction->effect_out;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_instruction_primitive(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    On1xSDK_IRStringView *out_primitive) {
    if (!out_primitive) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Instruction *instruction = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_instruction(snapshot, reference, instruction);
        status != ON1X_SDK_OK)
        return status;
    *out_primitive = {instruction->primitive.data(),
                      instruction->primitive.size()};
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_instruction_span(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    On1xSDK_IRSourceSpan *out_span) {
    if (!out_span) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Instruction *instruction = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_instruction(snapshot, reference, instruction);
        status != ON1X_SDK_OK)
        return status;
    *out_span = on1x::sdk::ir::span(instruction->where);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_instruction_operand_count(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    size_t *out_count) {
    if (!out_count) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Instruction *instruction = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_instruction(snapshot, reference, instruction);
        status != ON1X_SDK_OK)
        return status;
    *out_count = instruction->operands.size();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_instruction_operand(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIRRef *reference,
    size_t index,
    uint64_t *out_value) {
    if (!out_value) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::mir::Instruction *instruction = nullptr;
    if (const auto status =
            on1x::sdk::ir::mir_instruction(snapshot, reference, instruction);
        status != ON1X_SDK_OK)
        return status;
    if (index >= instruction->operands.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    *out_value = instruction->operands[index];
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_visit_instructions(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_IRLimits *limits_record,
    On1xSDK_MIRVisitInstructionFn visitor,
    void *userdata,
    On1xSDK_IRFailure *out_failure) {
    using namespace on1x::sdk::ir;
    set_failure(out_failure, ON1X_SDK_IR_FAILURE_NONE);
    if (!snapshot || !visitor) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    On1xSDK_IRLimits limits{};
    if (const auto status = read_limits(
            limits_record, 65536, 4194304, ON1X_SDK_IR_MAX_DEPTH, limits);
        status != ON1X_SDK_OK) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return status;
    }
    std::size_t nodes = 0, work = 0;
    for (std::size_t f = 0; f < snapshot->module.functions.size(); ++f) {
        for (std::size_t b = 0;
             b < snapshot->module.functions[f].blocks.size(); ++b) {
            const auto &block = snapshot->module.functions[f].blocks[b];
            for (std::size_t i = 0; i < block.instructions.size(); ++i) {
                if (++nodes > limits.max_nodes || ++work > limits.max_work) {
                    set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
                    return ON1X_SDK_LIMIT;
                }
                const auto reference = mir_ref(
                    *snapshot, ON1X_SDK_MIR_REF_INSTRUCTION,
                    f, b, i, block.instructions[i].id);
                On1xSDK_Status callback = ON1X_SDK_ERROR;
                try {
                    callback = normalize_status(
                        visitor(userdata, snapshot, &reference));
                } catch (...) {
                    callback = ON1X_SDK_ERROR;
                }
                if (callback != ON1X_SDK_OK) {
                    set_failure(out_failure, ON1X_SDK_IR_FAILURE_CALLBACK);
                    return callback;
                }
            }
        }
    }
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_evaluate(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_MIREvaluateOptions *options,
    On1xSDK_MIREvaluation **out_evaluation) {
    using namespace on1x::sdk::ir;
    if (!out_evaluation) return ON1X_SDK_INVALID_ARGUMENT;
    *out_evaluation = nullptr;
    if (!snapshot) return ON1X_SDK_INVALID_ARGUMENT;
    if (snapshot->identity.state != ON1X_SDK_IR_VERIFIED_MIR)
        return ON1X_SDK_UNSUPPORTED;
    std::string function;
    const std::int64_t *arguments = nullptr;
    std::size_t argument_count = 0, steps = 0, depth = 0;
    if (const auto status = evaluation_options(
            options, function, arguments, argument_count, steps, depth);
        status != ON1X_SDK_OK)
        return status;
    return guard([&] {
        std::vector<std::int64_t> copied;
        if (argument_count)
            copied.assign(arguments, arguments + argument_count);
        auto evaluation = std::make_unique<On1xSDK_MIREvaluation>();
        evaluation->identity = snapshot->identity;
        evaluation->evaluation = on1x::mir::evaluate(
            snapshot->module, function, copied, steps, depth);
        *out_evaluation = evaluation.release();
        return ON1X_SDK_OK;
    });
}

void on1x_sdk_mir_evaluation_release(On1xSDK_MIREvaluation *evaluation) {
    delete evaluation;
}

On1xSDK_Status on1x_sdk_mir_evaluation_status(
    const On1xSDK_MIREvaluation *evaluation,
    On1xSDK_MIREvaluationStatus *out_status) {
    if (!evaluation || !out_status) return ON1X_SDK_INVALID_ARGUMENT;
    switch (evaluation->evaluation.status) {
    case on1x::mir::Status::Returned:
        *out_status = ON1X_SDK_MIR_EVALUATION_RETURNED;
        break;
    case on1x::mir::Status::Trapped:
        *out_status = ON1X_SDK_MIR_EVALUATION_TRAPPED;
        break;
    case on1x::mir::Status::BudgetExhausted:
        *out_status = ON1X_SDK_MIR_EVALUATION_BUDGET_EXHAUSTED;
        break;
    }
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_evaluation_type(
    const On1xSDK_MIREvaluation *evaluation,
    On1xSDK_Type **out_type) {
    if (!out_type) return ON1X_SDK_INVALID_ARGUMENT;
    *out_type = nullptr;
    if (!evaluation) return ON1X_SDK_INVALID_ARGUMENT;
    return on1x::sdk::ir::guard([&] {
        *out_type = on1x::sdk::ir::make_type(
            evaluation->identity, evaluation->evaluation.type);
        return ON1X_SDK_OK;
    });
}

On1xSDK_Status on1x_sdk_mir_evaluation_scalar(
    const On1xSDK_MIREvaluation *evaluation,
    int64_t *out_scalar) {
    if (!evaluation || !out_scalar) return ON1X_SDK_INVALID_ARGUMENT;
    if (evaluation->evaluation.status != on1x::mir::Status::Returned ||
        evaluation->evaluation.type.is_tuple())
        return ON1X_SDK_UNSUPPORTED;
    *out_scalar = evaluation->evaluation.value;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_evaluation_output(
    const On1xSDK_MIREvaluation *evaluation,
    On1xSDK_IRStringView *out_output) {
    if (!evaluation || !out_output) return ON1X_SDK_INVALID_ARGUMENT;
    const auto &text = evaluation->evaluation.output;
    *out_output = {text.data(), text.size()};
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_evaluation_diagnostic(
    const On1xSDK_MIREvaluation *evaluation,
    On1xSDK_IRStringView *out_diagnostic) {
    if (!evaluation || !out_diagnostic)
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto &text = evaluation->evaluation.diagnostic;
    *out_diagnostic = {text.data(), text.size()};
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_evaluation_steps(
    const On1xSDK_MIREvaluation *evaluation,
    size_t *out_steps) {
    if (!evaluation || !out_steps) return ON1X_SDK_INVALID_ARGUMENT;
    *out_steps = evaluation->evaluation.steps;
    return ON1X_SDK_OK;
}

} // extern "C"
