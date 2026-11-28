#include "IRInternal.hpp"

namespace on1x::sdk::ir {
namespace {

On1xSDK_Status transaction_state(
    const On1xSDK_MIRTransaction *transaction,
    On1xSDK_IRFailure *failure = nullptr) noexcept {
    if (!transaction) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!transaction->active || !transaction->source) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_LIFECYCLE);
        return ON1X_SDK_BUSY;
    }
    if (transaction->thread != std::this_thread::get_id()) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_LIFECYCLE);
        return ON1X_SDK_BUSY;
    }
    return ON1X_SDK_OK;
}

On1xSDK_Status mutable_instruction(
    On1xSDK_MIRTransaction *transaction,
    const On1xSDK_MIRRef *reference,
    mir::Instruction *&out) noexcept {
    out = nullptr;
    if (const auto status = transaction_state(transaction);
        status != ON1X_SDK_OK)
        return status;
    const mir::Instruction *source = nullptr;
    if (const auto status = mir_instruction(
            transaction->source, reference, source);
        status != ON1X_SDK_OK)
        return status;
    const auto f = reference->function_index;
    const auto b = reference->block_index;
    const auto i = reference->instruction_index;
    if (f >= transaction->working.functions.size() ||
        b >= transaction->working.functions[f].blocks.size() ||
        i >= transaction->working.functions[f].blocks[b].instructions.size())
        return ON1X_SDK_ERROR;
    auto &candidate =
        transaction->working.functions[f].blocks[b].instructions[i];
    if (candidate.id != source->id) return ON1X_SDK_ERROR;
    out = &candidate;
    return ON1X_SDK_OK;
}

On1xSDK_Status mutable_function(
    On1xSDK_MIRTransaction *transaction,
    const On1xSDK_MIRRef *reference,
    mir::Function *&out) noexcept {
    out = nullptr;
    if (const auto status = transaction_state(transaction);
        status != ON1X_SDK_OK)
        return status;
    const mir::Function *source = nullptr;
    if (const auto status = mir_function(
            transaction->source, reference, source);
        status != ON1X_SDK_OK)
        return status;
    const auto f = reference->function_index;
    if (f >= transaction->working.functions.size())
        return ON1X_SDK_ERROR;
    auto &candidate = transaction->working.functions[f];
    if (candidate.id != source->id) return ON1X_SDK_ERROR;
    out = &candidate;
    return ON1X_SDK_OK;
}

} // namespace
} // namespace on1x::sdk::ir

extern "C" {

On1xSDK_Status on1x_sdk_mir_transaction_begin(
    const On1xSDK_MIRSnapshot *source,
    const On1xSDK_IRIdentity *expected_source,
    On1xSDK_MIRTransaction **out_transaction,
    On1xSDK_IRFailure *out_failure) {
    using namespace on1x::sdk::ir;
    if (!out_transaction) return ON1X_SDK_INVALID_ARGUMENT;
    *out_transaction = nullptr;
    set_failure(out_failure, ON1X_SDK_IR_FAILURE_NONE);
    if (!source) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (const auto status =
            check_revision(source->identity, expected_source, out_failure);
        status != ON1X_SDK_OK)
        return status;
    const auto status = guard([&] {
        auto transaction = std::make_unique<On1xSDK_MIRTransaction>();
        transaction->source = source;
        transaction->source_identity = source->identity;
        transaction->working = source->module;
        transaction->thread = std::this_thread::get_id();
        on1x_sdk_mir_snapshot_retain(
            const_cast<On1xSDK_MIRSnapshot *>(source));
        *out_transaction = transaction.release();
        return ON1X_SDK_OK;
    });
    if (status != ON1X_SDK_OK)
        set_failure(out_failure, status == ON1X_SDK_LIMIT
                                     ? ON1X_SDK_IR_FAILURE_LIMIT
                                     : ON1X_SDK_IR_FAILURE_LIFECYCLE);
    return status;
}

On1xSDK_Status on1x_sdk_mir_transaction_set_immediate(
    On1xSDK_MIRTransaction *transaction,
    const On1xSDK_MIRRef *instruction,
    int64_t immediate) {
    on1x::mir::Instruction *candidate = nullptr;
    if (const auto status = on1x::sdk::ir::mutable_instruction(
            transaction, instruction, candidate);
        status != ON1X_SDK_OK)
        return status;
    candidate->immediate = immediate;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_transaction_set_operand(
    On1xSDK_MIRTransaction *transaction,
    const On1xSDK_MIRRef *instruction,
    size_t operand_index,
    uint64_t value) {
    on1x::mir::Instruction *candidate = nullptr;
    if (const auto status = on1x::sdk::ir::mutable_instruction(
            transaction, instruction, candidate);
        status != ON1X_SDK_OK)
        return status;
    if (operand_index >= candidate->operands.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    candidate->operands[operand_index] = value;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_transaction_rename_function(
    On1xSDK_MIRTransaction *transaction,
    const On1xSDK_MIRRef *function,
    On1xSDK_IRStringView name) {
    on1x::mir::Function *candidate = nullptr;
    if (const auto status = on1x::sdk::ir::mutable_function(
            transaction, function, candidate);
        status != ON1X_SDK_OK)
        return status;
    std::string copied;
    if (const auto status = on1x::sdk::ir::copy_text(
            name, 1024, false, copied);
        status != ON1X_SDK_OK)
        return status;
    candidate->name = std::move(copied);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_mir_transaction_verify(
    const On1xSDK_MIRTransaction *transaction,
    const On1xSDK_IRLimits *limits,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics) {
    if (const auto status =
            on1x::sdk::ir::transaction_state(transaction, out_failure);
        status != ON1X_SDK_OK)
        return status;
    return on1x::sdk::ir::verify_module(
        transaction->working, limits, out_failure, diagnostics);
}

On1xSDK_Status on1x_sdk_mir_transaction_commit(
    On1xSDK_MIRTransaction *transaction,
    const On1xSDK_IRIdentity *expected_source,
    uint64_t new_revision,
    On1xSDK_MIRSnapshot **out_snapshot,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics) {
    using namespace on1x::sdk::ir;
    if (!out_snapshot) return ON1X_SDK_INVALID_ARGUMENT;
    *out_snapshot = nullptr;
    set_failure(out_failure, ON1X_SDK_IR_FAILURE_NONE);
    if (const auto status = transaction_state(transaction, out_failure);
        status != ON1X_SDK_OK)
        return status;
    if (const auto status = check_revision(
            transaction->source_identity, expected_source, out_failure);
        status != ON1X_SDK_OK)
        return status;
    if (!new_revision ||
        new_revision <= transaction->source_identity.revision) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_STALE_REVISION);
        return ON1X_SDK_BUSY;
    }
    if (const auto status = verify_module(
            transaction->working, nullptr, out_failure, diagnostics);
        status != ON1X_SDK_OK)
        return status;

    const auto status = guard([&] {
        auto identity = transaction->source_identity;
        identity.revision = new_revision;
        identity.state = ON1X_SDK_IR_CANDIDATE;
        *out_snapshot = make_mir_snapshot(
            identity, transaction->source->provenance,
            transaction->working);
        return ON1X_SDK_OK;
    });
    if (status == ON1X_SDK_OK) {
        transaction->active = false;
        on1x_sdk_mir_snapshot_release(
            const_cast<On1xSDK_MIRSnapshot *>(transaction->source));
        transaction->source = nullptr;
    } else {
        set_failure(out_failure, status == ON1X_SDK_LIMIT
                                 ? ON1X_SDK_IR_FAILURE_LIMIT
                                 : ON1X_SDK_IR_FAILURE_LIFECYCLE);
    }
    return status;
}

void on1x_sdk_mir_transaction_destroy(
    On1xSDK_MIRTransaction *transaction) {
    if (!transaction) return;
    if (transaction->source)
        on1x_sdk_mir_snapshot_release(
            const_cast<On1xSDK_MIRSnapshot *>(transaction->source));
    delete transaction;
}

} // extern "C"
