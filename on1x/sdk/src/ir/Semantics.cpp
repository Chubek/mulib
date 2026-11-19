#include "IRInternal.hpp"

#include <on1x/canonical/control.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/cliche.hpp>
#include <on1x/concurrency.hpp>
#include <on1x/extensions.hpp>
#include <on1x/inherent.hpp>
#include <on1x/nominal.hpp>

extern "C" {

On1xSDK_Status on1x_sdk_semantics_verify_checked_hir(
    const On1xSDK_HIRSnapshot *snapshot,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics) {
    using namespace on1x::sdk::ir;
    set_failure(out_failure, ON1X_SDK_IR_FAILURE_NONE);
    if (!snapshot) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (snapshot->identity.state != ON1X_SDK_IR_CHECKED_HIR ||
        !snapshot->canonical) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_UNSUPPORTED);
        return ON1X_SDK_UNSUPPORTED;
    }
    try {
        const auto &unit = *snapshot->canonical;
        on1x::verify_nominals(unit);
        on1x::cliches::verify_elaborated(unit);
        on1x::inherent::verify_elaborated(unit);
        on1x::canonical::verify_control(unit);
        on1x::canonical::verify_values(unit);
        on1x::csp::verify(unit, true);
        on1x::extensions::verify_evidence(unit);
        return ON1X_SDK_OK;
    } catch (const on1x::Error &error) {
        const auto status = error_status(error);
        set_failure(out_failure, status == ON1X_SDK_LIMIT
                             ? ON1X_SDK_IR_FAILURE_LIMIT
                             : ON1X_SDK_IR_FAILURE_SEMANTIC);
        emit_diagnostic(diagnostics, "ON1X-SDK-HIR-VERIFY",
                        error.what(), &error.where);
        return status;
    } catch (const std::bad_alloc &) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
        return ON1X_SDK_LIMIT;
    } catch (...) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_SEMANTIC);
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_sdk_semantics_lower_checked_hir(
    const On1xSDK_HIRSnapshot *snapshot,
    uint64_t output_revision,
    const On1xSDK_IRLimits *limits_record,
    On1xSDK_MIRSnapshot **out_mir,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics) {
    using namespace on1x::sdk::ir;
    if (!out_mir) return ON1X_SDK_INVALID_ARGUMENT;
    *out_mir = nullptr;
    set_failure(out_failure, ON1X_SDK_IR_FAILURE_NONE);
    if (!snapshot) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (snapshot->identity.state != ON1X_SDK_IR_CHECKED_HIR ||
        !snapshot->canonical) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_UNSUPPORTED);
        return ON1X_SDK_UNSUPPORTED;
    }
    if (!output_revision || output_revision <= snapshot->identity.revision) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_STALE_REVISION);
        return ON1X_SDK_BUSY;
    }
    On1xSDK_IRLimits limits{};
    if (const auto status = read_limits(
            limits_record, 65536, 4194304, ON1X_SDK_IR_MAX_DEPTH, limits);
        status != ON1X_SDK_OK) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return status;
    }
    if (const auto status = on1x_sdk_hir_verify_structure(
            snapshot, &limits, out_failure, diagnostics);
        status != ON1X_SDK_OK)
        return status;
    try {
        auto module = on1x::mir::lower(
            *snapshot->canonical, {limits.max_nodes, limits.max_work});
        on1x::mir::verify(module, {limits.max_nodes, limits.max_work});
        auto identity = snapshot->identity;
        identity.revision = output_revision;
        identity.state = ON1X_SDK_IR_VERIFIED_MIR;
        *out_mir = make_mir_snapshot(identity, snapshot->provenance,
                                     std::move(module));
        return ON1X_SDK_OK;
    } catch (const on1x::Error &error) {
        const auto status = error_status(error);
        if (status == ON1X_SDK_UNSUPPORTED)
            set_failure(out_failure, ON1X_SDK_IR_FAILURE_UNSUPPORTED);
        else if (status == ON1X_SDK_LIMIT)
            set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
        else
            set_failure(out_failure, ON1X_SDK_IR_FAILURE_SEMANTIC);
        emit_diagnostic(diagnostics, "ON1X-SDK-MIR-LOWER",
                        error.what(), &error.where);
        return status;
    } catch (const std::bad_alloc &) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
        return ON1X_SDK_LIMIT;
    } catch (...) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_SEMANTIC);
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_sdk_semantics_verify_mir(
    const On1xSDK_MIRSnapshot *snapshot,
    const On1xSDK_IRLimits *limits,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics) {
    if (!snapshot) {
        on1x::sdk::ir::set_failure(
            out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    return on1x::sdk::ir::verify_module(
        snapshot->module, limits, out_failure, diagnostics);
}

} // extern "C"
