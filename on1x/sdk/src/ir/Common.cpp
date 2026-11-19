#include "IRInternal.hpp"
#include "On1xSDK-Status.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace on1x::sdk::ir {
namespace {

struct Prefix {
    std::uint32_t abi_version;
    std::uint32_t struct_size;
};

bool prefix(const void *record, std::size_t required, Prefix &out) noexcept {
    if (!record) return false;
    std::memcpy(&out, record, sizeof out);
    return out.abi_version == ON1X_SDK_SEMANTICS_IR_ABI_VERSION &&
           out.struct_size >= required;
}

bool continuation(unsigned char byte) noexcept {
    return (byte & 0xc0u) == 0x80u;
}

} // namespace

bool valid_utf8(std::string_view text) noexcept {
    const auto *bytes = reinterpret_cast<const unsigned char *>(text.data());
    for (std::size_t i = 0; i < text.size();) {
        const auto first = bytes[i++];
        if (first <= 0x7fu) continue;
        if (first >= 0xc2u && first <= 0xdfu) {
            if (i >= text.size() || !continuation(bytes[i])) return false;
            ++i;
            continue;
        }
        if (first >= 0xe0u && first <= 0xefu) {
            if (i + 1 >= text.size() || !continuation(bytes[i]) ||
                !continuation(bytes[i + 1])) return false;
            if ((first == 0xe0u && bytes[i] < 0xa0u) ||
                (first == 0xedu && bytes[i] >= 0xa0u)) return false;
            i += 2;
            continue;
        }
        if (first >= 0xf0u && first <= 0xf4u) {
            if (i + 2 >= text.size() || !continuation(bytes[i]) ||
                !continuation(bytes[i + 1]) || !continuation(bytes[i + 2]))
                return false;
            if ((first == 0xf0u && bytes[i] < 0x90u) ||
                (first == 0xf4u && bytes[i] >= 0x90u)) return false;
            i += 3;
            continue;
        }
        return false;
    }
    return true;
}

On1xSDK_Status copy_text(On1xSDK_IRStringView view, std::size_t maximum,
                         bool allow_empty, std::string &out) noexcept {
    if ((view.size && !view.data) || view.size > maximum ||
        (!allow_empty && !view.size))
        return view.size > maximum ? ON1X_SDK_LIMIT : ON1X_SDK_INVALID_ARGUMENT;
    const std::string_view text{view.data ? view.data : "", view.size};
    if (!valid_utf8(text) || text.find('\0') != std::string_view::npos)
        return ON1X_SDK_INVALID_ARGUMENT;
    try {
        out.assign(text);
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status read_identity(const On1xSDK_IRIdentity *source,
                             On1xSDK_IRValidationState required,
                             Identity &out) noexcept {
    Prefix header{};
    constexpr auto minimum = offsetof(On1xSDK_IRIdentity, revision) +
                             sizeof(On1xSDK_IRIdentity::revision);
    if (!source) return ON1X_SDK_INVALID_ARGUMENT;
    if (!prefix(source, minimum, header)) return ON1X_SDK_ABI_MISMATCH;

    On1xSDK_IRIdentity copy{};
    std::memcpy(&copy, source,
                std::min<std::size_t>(header.struct_size, sizeof copy));
    if (!copy.owner || !copy.generation || !copy.revision)
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto state = header.struct_size >=
        offsetof(On1xSDK_IRIdentity, state) + sizeof copy.state
        ? copy.state : ON1X_SDK_IR_CANDIDATE;
    const auto schema = header.struct_size >=
        offsetof(On1xSDK_IRIdentity, schema_version) + sizeof copy.schema_version
        ? copy.schema_version : ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION;
    if (state != ON1X_SDK_IR_CANDIDATE &&
        state != ON1X_SDK_IR_CHECKED_HIR &&
        state != ON1X_SDK_IR_VERIFIED_MIR)
        return ON1X_SDK_INVALID_ARGUMENT;
    if (required && state != required) return ON1X_SDK_INVALID_ARGUMENT;
    if (schema != ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION)
        return ON1X_SDK_ABI_MISMATCH;
    out = {copy.owner, copy.generation, copy.revision, state, schema};
    return ON1X_SDK_OK;
}

bool same_identity(const Identity &left, const Identity &right) noexcept {
    return left.owner == right.owner &&
           left.generation == right.generation &&
           left.revision == right.revision;
}

On1xSDK_Status check_revision(const Identity &identity,
                              const On1xSDK_IRIdentity *expected,
                              On1xSDK_IRFailure *failure) noexcept {
    Identity copy;
    if (const auto status = read_identity(expected, identity.state, copy);
        status != ON1X_SDK_OK) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return status;
    }
    if (copy.owner != identity.owner || copy.generation != identity.generation) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_CROSS_CONTEXT);
        return ON1X_SDK_BUSY;
    }
    if (copy.revision != identity.revision) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_STALE_REVISION);
        return ON1X_SDK_BUSY;
    }
    return ON1X_SDK_OK;
}

void write_identity(const Identity &identity, On1xSDK_IRIdentity &out) noexcept {
    out = {};
    out.abi_version = ON1X_SDK_SEMANTICS_IR_ABI_VERSION;
    out.struct_size = sizeof out;
    out.owner = identity.owner;
    out.generation = identity.generation;
    out.revision = identity.revision;
    out.state = identity.state;
    out.schema_version = identity.schema_version;
}

On1xSDK_Status read_limits(const On1xSDK_IRLimits *source,
                           std::size_t default_nodes,
                           std::size_t default_work,
                           std::size_t default_depth,
                           On1xSDK_IRLimits &out) noexcept {
    out = {ON1X_SDK_SEMANTICS_IR_ABI_VERSION, sizeof out,
           default_nodes, default_work, default_depth};
    if (!source) return ON1X_SDK_OK;
    Prefix header{};
    if (!prefix(source, sizeof(Prefix), header))
        return ON1X_SDK_ABI_MISMATCH;
    On1xSDK_IRLimits copy{};
    std::memcpy(&copy, source,
                std::min<std::size_t>(header.struct_size, sizeof copy));
    if (header.struct_size >= offsetof(On1xSDK_IRLimits, max_nodes) +
                              sizeof copy.max_nodes)
        out.max_nodes = copy.max_nodes;
    if (header.struct_size >= offsetof(On1xSDK_IRLimits, max_work) +
                              sizeof copy.max_work)
        out.max_work = copy.max_work;
    if (header.struct_size >= offsetof(On1xSDK_IRLimits, max_depth) +
                              sizeof copy.max_depth)
        out.max_depth = copy.max_depth;
    if (!out.max_nodes || !out.max_work || !out.max_depth ||
        out.max_nodes > ON1X_SDK_IR_MAX_NODES ||
        out.max_depth > ON1X_SDK_IR_MAX_DEPTH)
        return ON1X_SDK_INVALID_ARGUMENT;
    return ON1X_SDK_OK;
}

On1xSDK_Status normalize_status(On1xSDK_Status status) noexcept {
    return on1x_sdk_status_is_known(status) ? status : ON1X_SDK_ERROR;
}

On1xSDK_Status error_status(const Error &error) noexcept {
    const std::string_view id = error.diagnostic_id;
    const std::string_view message = error.what();
    if (id == "ON1X-MIR-UNSUPPORTED" ||
        message.find("UNSUPPORTED") != std::string_view::npos ||
        message.find("unsupported") != std::string_view::npos)
        return ON1X_SDK_UNSUPPORTED;
    if (id.find("LIMIT") != std::string_view::npos ||
        message.find("budget exhausted") != std::string_view::npos ||
        message.find("limit exceeded") != std::string_view::npos ||
        message.find("byte limit") != std::string_view::npos ||
        message.find("node limit") != std::string_view::npos)
        return ON1X_SDK_LIMIT;
    return ON1X_SDK_ERROR;
}

void set_failure(On1xSDK_IRFailure *out, On1xSDK_IRFailure value) noexcept {
    if (out) *out = value;
}

void emit_diagnostic(const On1xSDK_DiagnosticSink *sink,
                     const char *code,
                     const std::string &message,
                     const Location *where) noexcept {
    if (!sink || !sink->emit) return;
    const On1xSDK_Diagnostic diagnostic{
        ON1X_SDK_DIAGNOSTIC_ERROR,
        code,
        message.c_str(),
        where ? where->offset : SIZE_MAX,
        where ? where->offset : SIZE_MAX
    };
    try {
        sink->emit(sink->userdata, &diagnostic);
    } catch (...) {
        // ABI-v1 sinks cannot report failure. Never unwind through C.
    }
}

On1xSDK_IRSourceSpan span(const Location &where) noexcept {
    return {{where.file.data(), where.file.size()},
            static_cast<std::uint64_t>(where.offset),
            static_cast<std::uint64_t>(where.offset),
            static_cast<std::uint64_t>(where.line),
            static_cast<std::uint64_t>(where.column)};
}

On1xSDK_HIRSnapshot *make_hir_snapshot(
    Identity identity,
    std::string provenance,
    std::vector<HIRNode> nodes,
    std::vector<std::uint64_t> functions,
    bool wire_complete,
    std::shared_ptr<const Unit> canonical) {
    auto snapshot = std::make_unique<On1xSDK_HIRSnapshot>();
    snapshot->identity = identity;
    snapshot->provenance = std::move(provenance);
    snapshot->nodes = std::move(nodes);
    snapshot->functions = std::move(functions);
    snapshot->wire_complete = wire_complete;
    snapshot->canonical = std::move(canonical);
    return snapshot.release();
}

On1xSDK_MIRSnapshot *make_mir_snapshot(
    Identity identity,
    std::string provenance,
    mir::Module module) {
    auto snapshot = std::make_unique<On1xSDK_MIRSnapshot>();
    snapshot->identity = identity;
    snapshot->provenance = std::move(provenance);
    snapshot->module = std::move(module);
    return snapshot.release();
}

On1xSDK_HIRNodeRef hir_ref(const On1xSDK_HIRSnapshot &snapshot,
                           std::uint64_t id) noexcept {
    return {ON1X_SDK_SEMANTICS_IR_ABI_VERSION,
            sizeof(On1xSDK_HIRNodeRef),
            &snapshot,
            snapshot.identity.owner,
            snapshot.identity.generation,
            snapshot.identity.revision,
            id};
}

On1xSDK_Status hir_node(const On1xSDK_HIRSnapshot *snapshot,
                        const On1xSDK_HIRNodeRef *reference,
                        const HIRNode *&out) noexcept {
    out = nullptr;
    if (!snapshot || !reference) return ON1X_SDK_INVALID_ARGUMENT;
    Prefix header{};
    if (!prefix(reference, sizeof *reference, header))
        return ON1X_SDK_ABI_MISMATCH;
    if (reference->snapshot_cookie != snapshot ||
        reference->owner != snapshot->identity.owner ||
        reference->generation != snapshot->identity.generation ||
        reference->revision != snapshot->identity.revision)
        return ON1X_SDK_BUSY;
    if (!reference->id || reference->id > snapshot->nodes.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto &node = snapshot->nodes[reference->id - 1];
    if (node.id != reference->id) return ON1X_SDK_ERROR;
    out = &node;
    return ON1X_SDK_OK;
}

On1xSDK_MIRRef mir_ref(const On1xSDK_MIRSnapshot &snapshot,
                       On1xSDK_MIRRefKind kind,
                       std::size_t function,
                       std::size_t block,
                       std::size_t instruction,
                       std::uint64_t id) noexcept {
    return {ON1X_SDK_SEMANTICS_IR_ABI_VERSION,
            sizeof(On1xSDK_MIRRef),
            &snapshot,
            snapshot.identity.owner,
            snapshot.identity.generation,
            snapshot.identity.revision,
            kind,
            0,
            id,
            function,
            block,
            instruction};
}

namespace {

On1xSDK_Status validate_mir_ref(const On1xSDK_MIRSnapshot *snapshot,
                                const On1xSDK_MIRRef *reference,
                                On1xSDK_MIRRefKind kind) noexcept {
    if (!snapshot || !reference) return ON1X_SDK_INVALID_ARGUMENT;
    Prefix header{};
    if (!prefix(reference, sizeof *reference, header))
        return ON1X_SDK_ABI_MISMATCH;
    if (reference->snapshot_cookie != snapshot ||
        reference->owner != snapshot->identity.owner ||
        reference->generation != snapshot->identity.generation ||
        reference->revision != snapshot->identity.revision)
        return ON1X_SDK_BUSY;
    if (reference->kind != kind) return ON1X_SDK_INVALID_ARGUMENT;
    if (reference->function_index >= snapshot->module.functions.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    return ON1X_SDK_OK;
}

} // namespace

On1xSDK_Status mir_function(const On1xSDK_MIRSnapshot *snapshot,
                            const On1xSDK_MIRRef *reference,
                            const mir::Function *&out) noexcept {
    out = nullptr;
    const auto status = validate_mir_ref(snapshot, reference,
                                         ON1X_SDK_MIR_REF_FUNCTION);
    if (status != ON1X_SDK_OK) return status;
    const auto &function = snapshot->module.functions[reference->function_index];
    if (function.id != reference->id) return ON1X_SDK_INVALID_ARGUMENT;
    out = &function;
    return ON1X_SDK_OK;
}

On1xSDK_Status mir_block(const On1xSDK_MIRSnapshot *snapshot,
                         const On1xSDK_MIRRef *reference,
                         const mir::Block *&out) noexcept {
    out = nullptr;
    const auto status = validate_mir_ref(snapshot, reference,
                                         ON1X_SDK_MIR_REF_BLOCK);
    if (status != ON1X_SDK_OK) return status;
    const auto &function = snapshot->module.functions[reference->function_index];
    if (reference->block_index >= function.blocks.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto &block = function.blocks[reference->block_index];
    if (block.id != reference->id) return ON1X_SDK_INVALID_ARGUMENT;
    out = &block;
    return ON1X_SDK_OK;
}

On1xSDK_Status mir_instruction(const On1xSDK_MIRSnapshot *snapshot,
                               const On1xSDK_MIRRef *reference,
                               const mir::Instruction *&out) noexcept {
    out = nullptr;
    const auto status = validate_mir_ref(snapshot, reference,
                                         ON1X_SDK_MIR_REF_INSTRUCTION);
    if (status != ON1X_SDK_OK) return status;
    const auto &function = snapshot->module.functions[reference->function_index];
    if (reference->block_index >= function.blocks.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto &block = function.blocks[reference->block_index];
    if (reference->instruction_index >= block.instructions.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto &instruction = block.instructions[reference->instruction_index];
    if (instruction.id != reference->id) return ON1X_SDK_INVALID_ARGUMENT;
    out = &instruction;
    return ON1X_SDK_OK;
}

On1xSDK_Status verify_module(const mir::Module &module,
                             const On1xSDK_IRLimits *limits,
                             On1xSDK_IRFailure *failure,
                             const On1xSDK_DiagnosticSink *diagnostics) noexcept {
    set_failure(failure, ON1X_SDK_IR_FAILURE_NONE);
    On1xSDK_IRLimits checked{};
    if (const auto status = read_limits(limits, 65536, 4194304, 256, checked);
        status != ON1X_SDK_OK) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return status;
    }
    try {
        auto remaining_work = checked.max_work;
        auto charge = [&](std::size_t amount, const Location &where) {
            if (amount > remaining_work)
                throw Error(where, "SDK MIR text work budget exhausted",
                            "ON1X-SDK-IR-LIMIT");
            remaining_work -= amount;
        };
        auto text = [&](std::string_view value, std::size_t maximum,
                        bool allow_empty, const Location &where) {
            if ((!allow_empty && value.empty()) || value.size() > maximum ||
                !valid_utf8(value) ||
                value.find('\0') != std::string_view::npos)
                throw Error(where, "malformed UTF-8 MIR text metadata",
                            "ON1X-SDK-MIR-MALFORMED");
            charge(value.size() + 1, where);
        };
        auto location = [&](const Location &where) {
            text(where.file, 4096, false, where);
            if (!where.line || !where.column)
                throw Error(where, "malformed MIR source location",
                            "ON1X-SDK-MIR-MALFORMED");
        };
        for (const auto &function : module.functions) {
            location(function.where);
            text(function.name, 4096, false, function.where);
            for (const auto &slot : function.slots)
                location(slot.where);
            for (const auto &block : function.blocks) {
                location(block.where);
                for (const auto &parameter : block.parameters)
                    location(parameter.where);
                for (const auto &instruction : block.instructions) {
                    location(instruction.where);
                    text(instruction.primitive, 64, true,
                         instruction.where);
                }
                location(block.exit.where);
            }
        }
        mir::verify(module, {checked.max_nodes, remaining_work});
        return ON1X_SDK_OK;
    } catch (const Error &error) {
        const auto status = error_status(error);
        set_failure(failure, status == ON1X_SDK_LIMIT
                             ? ON1X_SDK_IR_FAILURE_LIMIT
                             : ON1X_SDK_IR_FAILURE_MALFORMED);
        emit_diagnostic(diagnostics,
                        status == ON1X_SDK_LIMIT ? "ON1X-SDK-IR-LIMIT"
                                                : "ON1X-SDK-MIR-VERIFY",
                        error.what(), &error.where);
        return status;
    } catch (const std::bad_alloc &) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_LIMIT);
        return ON1X_SDK_LIMIT;
    } catch (...) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_MALFORMED);
        return ON1X_SDK_ERROR;
    }
}

} // namespace on1x::sdk::ir

extern "C" On1xSDK_Status on1x_sdk_ir_identity_validate(
    const On1xSDK_IRIdentity *identity,
    On1xSDK_IRValidationState required_state) {
    on1x::sdk::ir::Identity copy;
    return on1x::sdk::ir::read_identity(identity, required_state, copy);
}
