#ifndef ON1X_SDK_IR_INTERNAL_HPP
#define ON1X_SDK_IR_INTERNAL_HPP

#include "On1xSDK-HIR.h"
#include "On1xSDK-IRSerialize.h"
#include "On1xSDK-IRTransaction.h"
#include "On1xSDK-Semantics.h"

#include <on1x/ast.hpp>
#include <on1x/backend/mir.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace on1x::sdk::ir {

struct Identity {
    const On1xSDK_Context *owner = nullptr;
    std::uint64_t generation = 0;
    std::uint64_t revision = 0;
    On1xSDK_IRValidationState state = ON1X_SDK_IR_CANDIDATE;
    std::uint32_t schema_version = ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION;
};

struct HIREdge {
    On1xSDK_HIREdgeKind kind = ON1X_SDK_HIR_EDGE_BODY;
    std::uint64_t target = 0;
};

struct HIRNode {
    std::uint64_t id = 0;
    On1xSDK_HIRNodeClass node_class = ON1X_SDK_HIR_NODE_EXPRESSION;
    std::uint32_t kind = 0;
    On1xSDK_HIRNodeFlags flags = 0;
    std::string name;
    Location where;
    Type type = Type::Invalid;
    std::vector<std::string> labels;
    std::uint64_t auxiliary[4]{};
    std::vector<HIREdge> children;
};

bool valid_utf8(std::string_view text) noexcept;
On1xSDK_Status copy_text(On1xSDK_IRStringView view, std::size_t maximum,
                         bool allow_empty, std::string &out) noexcept;
On1xSDK_Status read_identity(const On1xSDK_IRIdentity *source,
                             On1xSDK_IRValidationState required,
                             Identity &out) noexcept;
bool same_identity(const Identity &left, const Identity &right) noexcept;
On1xSDK_Status check_revision(const Identity &identity,
                              const On1xSDK_IRIdentity *expected,
                              On1xSDK_IRFailure *failure) noexcept;
void write_identity(const Identity &identity, On1xSDK_IRIdentity &out) noexcept;
On1xSDK_Status read_limits(const On1xSDK_IRLimits *source,
                           std::size_t default_nodes,
                           std::size_t default_work,
                           std::size_t default_depth,
                           On1xSDK_IRLimits &out) noexcept;
On1xSDK_Status normalize_status(On1xSDK_Status status) noexcept;
On1xSDK_Status error_status(const Error &error) noexcept;
void set_failure(On1xSDK_IRFailure *out, On1xSDK_IRFailure value) noexcept;
void emit_diagnostic(const On1xSDK_DiagnosticSink *sink,
                     const char *code,
                     const std::string &message,
                     const Location *where = nullptr) noexcept;
On1xSDK_IRSourceSpan span(const Location &where) noexcept;

template<class F>
On1xSDK_Status guard(F &&function) noexcept {
    try {
        return function();
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (const std::length_error &) {
        return ON1X_SDK_LIMIT;
    } catch (const Error &error) {
        return error_status(error);
    } catch (const std::logic_error &) {
        return ON1X_SDK_INVALID_ARGUMENT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Type *make_type(const Identity &identity, Type type);

On1xSDK_HIRNodeRef hir_ref(const On1xSDK_HIRSnapshot &snapshot,
                           std::uint64_t id) noexcept;
On1xSDK_Status hir_node(const On1xSDK_HIRSnapshot *snapshot,
                        const On1xSDK_HIRNodeRef *reference,
                        const HIRNode *&out) noexcept;

On1xSDK_MIRRef mir_ref(const On1xSDK_MIRSnapshot &snapshot,
                       On1xSDK_MIRRefKind kind,
                       std::size_t function,
                       std::size_t block,
                       std::size_t instruction,
                       std::uint64_t id) noexcept;
On1xSDK_Status mir_function(const On1xSDK_MIRSnapshot *snapshot,
                            const On1xSDK_MIRRef *reference,
                            const mir::Function *&out) noexcept;
On1xSDK_Status mir_block(const On1xSDK_MIRSnapshot *snapshot,
                         const On1xSDK_MIRRef *reference,
                         const mir::Block *&out) noexcept;
On1xSDK_Status mir_instruction(const On1xSDK_MIRSnapshot *snapshot,
                               const On1xSDK_MIRRef *reference,
                               const mir::Instruction *&out) noexcept;

On1xSDK_Status verify_module(const mir::Module &module,
                             const On1xSDK_IRLimits *limits,
                             On1xSDK_IRFailure *failure,
                             const On1xSDK_DiagnosticSink *diagnostics) noexcept;

On1xSDK_HIRSnapshot *make_hir_snapshot(
    Identity identity,
    std::string provenance,
    std::vector<HIRNode> nodes,
    std::vector<std::uint64_t> functions,
    bool wire_complete,
    std::shared_ptr<const Unit> canonical);

On1xSDK_MIRSnapshot *make_mir_snapshot(
    Identity identity,
    std::string provenance,
    mir::Module module);

} // namespace on1x::sdk::ir

struct On1xSDK_Type {
    std::atomic<std::size_t> references{1};
    on1x::sdk::ir::Identity identity;
    on1x::Type type;
};

struct On1xSDK_HIRSnapshot {
    std::atomic<std::size_t> references{1};
    on1x::sdk::ir::Identity identity;
    std::string provenance;
    std::vector<on1x::sdk::ir::HIRNode> nodes;
    std::vector<std::uint64_t> functions;
    bool wire_complete = false;
    std::shared_ptr<const on1x::Unit> canonical;
};

struct On1xSDK_MIRSnapshot {
    std::atomic<std::size_t> references{1};
    on1x::sdk::ir::Identity identity;
    std::string provenance;
    on1x::mir::Module module;
};

struct On1xSDK_IRBlob {
    std::atomic<std::size_t> references{1};
    std::vector<std::uint8_t> bytes;
};

struct On1xSDK_MIREvaluation {
    on1x::sdk::ir::Identity identity;
    on1x::mir::Evaluation evaluation;
};

struct On1xSDK_MIRTransaction {
    const On1xSDK_MIRSnapshot *source = nullptr;
    on1x::sdk::ir::Identity source_identity;
    on1x::mir::Module working;
    std::thread::id thread;
    bool active = true;
};

#endif
