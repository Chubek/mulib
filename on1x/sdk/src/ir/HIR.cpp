#include "CanonicalBridge.hpp"

#include <on1x/canonical/control.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/cliche.hpp>
#include <on1x/clone.hpp>
#include <on1x/concurrency.hpp>
#include <on1x/extensions.hpp>
#include <on1x/inherent.hpp>
#include <on1x/nominal.hpp>

#include <algorithm>
#include <limits>
#include <set>

namespace on1x::sdk::ir {
namespace {

class Flatten {
public:
    explicit Flatten(const Unit &unit) : unit_(unit) {
        complete_ = unit.imports.empty() && unit.module_imports.empty() &&
                    unit.kernel_imports.empty() && unit.features.empty() &&
                    unit.scrutiny.empty() && unit.extensions.empty() &&
                    unit.nominals.empty() && unit.cliches.empty() &&
                    unit.cliche_implementations.empty() &&
                    unit.inherent_implementations.empty() &&
                    unit.extension_evidence.empty() &&
                    unit.source_digests.empty() &&
                    unit.forall_layouts.empty() && unit.macros.empty() &&
                    unit.ppx.empty();
    }

    void run() {
        for (const auto &item : unit_.functions)
            functions_.push_back(function(item));
    }

    std::vector<HIRNode> take_nodes() { return std::move(nodes_); }
    std::vector<std::uint64_t> take_functions() {
        return std::move(functions_);
    }
    bool complete() const noexcept { return complete_; }

private:
    const Unit &unit_;
    std::vector<HIRNode> nodes_;
    std::vector<std::uint64_t> functions_;
    bool complete_ = true;

    std::uint64_t add(HIRNode node) {
        if (nodes_.size() >= ON1X_SDK_IR_MAX_NODES)
            throw Error(node.where, "SDK HIR node limit exceeded",
                        "ON1X-SDK-HIR-LIMIT");
        node.id = nodes_.size() + 1;
        nodes_.push_back(std::move(node));
        return nodes_.back().id;
    }

    void edge(std::uint64_t parent, On1xSDK_HIREdgeKind kind,
              std::uint64_t child) {
        nodes_.at(parent - 1).children.push_back({kind, child});
    }

    std::uint64_t pattern(const Pattern &source, std::size_t depth) {
        if (depth > ON1X_SDK_IR_MAX_DEPTH)
            throw Error(source.where, "SDK HIR pattern depth limit exceeded",
                        "ON1X-SDK-HIR-LIMIT");
        HIRNode node;
        node.node_class = ON1X_SDK_HIR_NODE_PATTERN;
        node.kind = static_cast<std::uint32_t>(source.kind);
        node.flags = source.open ? ON1X_SDK_HIR_FLAG_OPEN_PATTERN : 0;
        node.name = source.name;
        node.where = source.where;
        node.type = source.type;
        node.labels = source.labels;
        node.auxiliary[0] = source.context;
        const auto id = add(std::move(node));
        for (const auto &element : source.elements)
            edge(id, ON1X_SDK_HIR_EDGE_ELEMENT,
                 pattern(element, depth + 1));
        return id;
    }

    std::uint64_t parameter(const Param &source, std::size_t depth) {
        HIRNode node;
        node.node_class = ON1X_SDK_HIR_NODE_PARAMETER;
        node.flags = source.borrow == BorrowMode::Exclusive
            ? ON1X_SDK_HIR_FLAG_EXCLUSIVE : 0;
        node.name = source.name;
        node.where = source.where;
        node.type = source.type;
        node.auxiliary[0] = source.context;
        const auto id = add(std::move(node));
        if (source.pattern)
            edge(id, ON1X_SDK_HIR_EDGE_PATTERN,
                 pattern(*source.pattern, depth + 1));
        return id;
    }

    std::uint64_t capture(const Capture &source, const Location &where) {
        HIRNode node;
        node.node_class = ON1X_SDK_HIR_NODE_CAPTURE;
        node.name = source.name;
        node.where = where;
        node.type = source.type;
        return add(std::move(node));
    }

    std::uint64_t expression(const Expr &source, std::size_t depth) {
        if (depth > ON1X_SDK_IR_MAX_DEPTH)
            throw Error(source.where, "SDK HIR expression depth limit exceeded",
                        "ON1X-SDK-HIR-LIMIT");
        HIRNode node;
        node.node_class = ON1X_SDK_HIR_NODE_EXPRESSION;
        node.kind = static_cast<std::uint32_t>(source.kind);
        node.name = source.text;
        node.where = source.where;
        node.type = source.type;
        node.labels = source.labels;
        node.auxiliary[0] = static_cast<std::uint64_t>(source.reference);
        node.auxiliary[1] = source.context;
        node.auxiliary[2] = source.projection;
        node.auxiliary[3] = source.control_target;
        if (source.unsafe_region) node.flags |= ON1X_SDK_HIR_FLAG_UNSAFE;
        if (source.move_out) node.flags |= ON1X_SDK_HIR_FLAG_MOVE_OUT;
        if (source.noreturn) node.flags |= ON1X_SDK_HIR_FLAG_NO_RETURN;
        if (source.prototype) node.flags |= ON1X_SDK_HIR_FLAG_PROTOTYPE;
        if (source.statement_match)
            node.flags |= ON1X_SDK_HIR_FLAG_STATEMENT_MATCH;
        if (source.sum_match) node.flags |= ON1X_SDK_HIR_FLAG_SUM_MATCH;
        if (!source.type_parameters.empty() || !source.constraints.empty() ||
            !source.cliche_arguments.empty() ||
            !source.member_type_arguments.empty() ||
            !source.cliche_member.empty() ||
            !source.syntax_definitions.empty() || source.syntax_repeat)
            complete_ = false;
        const auto id = add(std::move(node));
        if (source.callee)
            edge(id, ON1X_SDK_HIR_EDGE_CALLEE,
                 expression(*source.callee, depth + 1));
        for (const auto &child : source.children) {
            if (!child)
                throw Error(source.where, "null HIR expression child",
                            "ON1X-SDK-HIR-MALFORMED");
            edge(id, ON1X_SDK_HIR_EDGE_OPERAND,
                 expression(*child, depth + 1));
        }
        for (const auto &proof : source.proofs) {
            if (!proof)
                throw Error(source.where, "null HIR proof child",
                            "ON1X-SDK-HIR-MALFORMED");
            edge(id, ON1X_SDK_HIR_EDGE_PROOF,
                 expression(*proof, depth + 1));
        }
        if (source.syntax_tail)
            edge(id, ON1X_SDK_HIR_EDGE_SYNTAX_TAIL,
                 expression(*source.syntax_tail, depth + 1));
        for (const auto &parameter_value : source.parameters)
            edge(id, ON1X_SDK_HIR_EDGE_PARAMETER,
                 parameter(parameter_value, depth + 1));
        for (const auto &item : source.patterns)
            edge(id, ON1X_SDK_HIR_EDGE_PATTERN,
                 pattern(item, depth + 1));
        for (const auto &item : source.captures)
            edge(id, ON1X_SDK_HIR_EDGE_CAPTURE, capture(item, source.where));
        for (const auto &item : source.body)
            edge(id, ON1X_SDK_HIR_EDGE_BODY,
                 statement(item, depth + 1));
        return id;
    }

    std::uint64_t statement(const Stmt &source, std::size_t depth) {
        if (depth > ON1X_SDK_IR_MAX_DEPTH)
            throw Error(source.where, "SDK HIR statement depth limit exceeded",
                        "ON1X-SDK-HIR-LIMIT");
        HIRNode node;
        node.node_class = ON1X_SDK_HIR_NODE_STATEMENT;
        node.kind = static_cast<std::uint32_t>(source.kind);
        node.name = source.name;
        node.where = source.where;
        node.type = source.declared;
        if (!source.label.empty()) node.labels.push_back(source.label);
        node.auxiliary[0] = source.context;
        node.auxiliary[1] = source.label_context;
        node.auxiliary[2] = source.control_target;
        node.auxiliary[3] = source.inclusive;
        if (source.mutable_binding) node.flags |= ON1X_SDK_HIR_FLAG_MUTABLE;
        if (source.prototype) node.flags |= ON1X_SDK_HIR_FLAG_PROTOTYPE;
        if (source.unsafe_block) node.flags |= ON1X_SDK_HIR_FLAG_UNSAFE;
        if (source.ref_store) node.flags |= ON1X_SDK_HIR_FLAG_EXCLUSIVE;
        const auto id = add(std::move(node));
        if (source.pattern)
            edge(id, ON1X_SDK_HIR_EDGE_PATTERN,
                 pattern(*source.pattern, depth + 1));
        if (source.value)
            edge(id, ON1X_SDK_HIR_EDGE_VALUE,
                 expression(*source.value, depth + 1));
        for (const auto &item : source.body)
            edge(id, ON1X_SDK_HIR_EDGE_BODY,
                 statement(item, depth + 1));
        for (const auto &item : source.otherwise)
            edge(id, ON1X_SDK_HIR_EDGE_ELSE,
                 statement(item, depth + 1));
        for (const auto &item : source.instances) {
            if (!item)
                throw Error(source.where, "null HIR statement instance",
                            "ON1X-SDK-HIR-MALFORMED");
            edge(id, ON1X_SDK_HIR_EDGE_INSTANCE,
                 statement(*item, depth + 1));
        }
        return id;
    }

    std::uint64_t function(const Function &source) {
        HIRNode node;
        node.node_class = ON1X_SDK_HIR_NODE_FUNCTION;
        node.name = source.name;
        node.where = source.where;
        std::vector<Type> parameters;
        parameters.reserve(source.params.size());
        for (const auto &parameter_value : source.params)
            parameters.push_back(parameter_value.type);
        node.type = Type::function(std::move(parameters), source.result);
        if (source.external) node.flags |= ON1X_SDK_HIR_FLAG_EXTERNAL;
        if (source.prototype) node.flags |= ON1X_SDK_HIR_FLAG_PROTOTYPE;
        if (source.extension_generated)
            node.flags |= ON1X_SDK_HIR_FLAG_EXTENSION_GENERATED;
        if (!source.type_parameters.empty() || !source.constraints.empty() ||
            !source.kernel_symbol.empty() || source.kernel_link_only)
            complete_ = false;
        const auto id = add(std::move(node));
        for (const auto &parameter_value : source.params)
            edge(id, ON1X_SDK_HIR_EDGE_PARAMETER,
                 parameter(parameter_value, 1));
        for (const auto &attribute : source.attributes) {
            if (!attribute)
                throw Error(source.where, "null HIR function attribute",
                            "ON1X-SDK-HIR-MALFORMED");
            edge(id, ON1X_SDK_HIR_EDGE_ATTRIBUTE,
                 expression(*attribute, 1));
        }
        for (const auto &item : source.body)
            edge(id, ON1X_SDK_HIR_EDGE_BODY, statement(item, 1));
        return id;
    }
};

On1xSDK_HIRNodeFlags allowed_flags(On1xSDK_HIRNodeClass node_class) noexcept {
    switch (node_class) {
    case ON1X_SDK_HIR_NODE_FUNCTION:
        return ON1X_SDK_HIR_FLAG_EXTERNAL |
               ON1X_SDK_HIR_FLAG_PROTOTYPE |
               ON1X_SDK_HIR_FLAG_EXTENSION_GENERATED;
    case ON1X_SDK_HIR_NODE_PARAMETER:
        return ON1X_SDK_HIR_FLAG_EXCLUSIVE;
    case ON1X_SDK_HIR_NODE_STATEMENT:
        return ON1X_SDK_HIR_FLAG_MUTABLE |
               ON1X_SDK_HIR_FLAG_PROTOTYPE |
               ON1X_SDK_HIR_FLAG_UNSAFE |
               ON1X_SDK_HIR_FLAG_EXCLUSIVE;
    case ON1X_SDK_HIR_NODE_EXPRESSION:
        return ON1X_SDK_HIR_FLAG_UNSAFE |
               ON1X_SDK_HIR_FLAG_MOVE_OUT |
               ON1X_SDK_HIR_FLAG_NO_RETURN |
               ON1X_SDK_HIR_FLAG_PROTOTYPE |
               ON1X_SDK_HIR_FLAG_STATEMENT_MATCH |
               ON1X_SDK_HIR_FLAG_SUM_MATCH;
    case ON1X_SDK_HIR_NODE_PATTERN:
        return ON1X_SDK_HIR_FLAG_OPEN_PATTERN;
    case ON1X_SDK_HIR_NODE_CAPTURE:
        return 0;
    }
    return 0;
}

bool valid_edge(const HIRNode &parent, const HIREdge &edge,
                const HIRNode &child) noexcept {
    switch (parent.node_class) {
    case ON1X_SDK_HIR_NODE_FUNCTION:
        return (edge.kind == ON1X_SDK_HIR_EDGE_PARAMETER &&
                child.node_class == ON1X_SDK_HIR_NODE_PARAMETER) ||
               (edge.kind == ON1X_SDK_HIR_EDGE_ATTRIBUTE &&
                child.node_class == ON1X_SDK_HIR_NODE_EXPRESSION) ||
               (edge.kind == ON1X_SDK_HIR_EDGE_BODY &&
                child.node_class == ON1X_SDK_HIR_NODE_STATEMENT);
    case ON1X_SDK_HIR_NODE_PARAMETER:
        return edge.kind == ON1X_SDK_HIR_EDGE_PATTERN &&
               child.node_class == ON1X_SDK_HIR_NODE_PATTERN;
    case ON1X_SDK_HIR_NODE_STATEMENT:
        return (edge.kind == ON1X_SDK_HIR_EDGE_PATTERN &&
                child.node_class == ON1X_SDK_HIR_NODE_PATTERN) ||
               (edge.kind == ON1X_SDK_HIR_EDGE_VALUE &&
                child.node_class == ON1X_SDK_HIR_NODE_EXPRESSION) ||
               ((edge.kind == ON1X_SDK_HIR_EDGE_BODY ||
                 edge.kind == ON1X_SDK_HIR_EDGE_ELSE ||
                 edge.kind == ON1X_SDK_HIR_EDGE_INSTANCE) &&
                child.node_class == ON1X_SDK_HIR_NODE_STATEMENT);
    case ON1X_SDK_HIR_NODE_EXPRESSION:
        return ((edge.kind == ON1X_SDK_HIR_EDGE_CALLEE ||
                 edge.kind == ON1X_SDK_HIR_EDGE_OPERAND ||
                 edge.kind == ON1X_SDK_HIR_EDGE_PROOF ||
                 edge.kind == ON1X_SDK_HIR_EDGE_SYNTAX_TAIL) &&
                child.node_class == ON1X_SDK_HIR_NODE_EXPRESSION) ||
               (edge.kind == ON1X_SDK_HIR_EDGE_PARAMETER &&
                child.node_class == ON1X_SDK_HIR_NODE_PARAMETER) ||
               (edge.kind == ON1X_SDK_HIR_EDGE_PATTERN &&
                child.node_class == ON1X_SDK_HIR_NODE_PATTERN) ||
               (edge.kind == ON1X_SDK_HIR_EDGE_CAPTURE &&
                child.node_class == ON1X_SDK_HIR_NODE_CAPTURE) ||
               (edge.kind == ON1X_SDK_HIR_EDGE_BODY &&
                child.node_class == ON1X_SDK_HIR_NODE_STATEMENT);
    case ON1X_SDK_HIR_NODE_PATTERN:
        return edge.kind == ON1X_SDK_HIR_EDGE_ELEMENT &&
               child.node_class == ON1X_SDK_HIR_NODE_PATTERN;
    case ON1X_SDK_HIR_NODE_CAPTURE:
        return false;
    }
    return false;
}

On1xSDK_Status validate_tree(const On1xSDK_HIRSnapshot &snapshot,
                             const On1xSDK_IRLimits &limits,
                             On1xSDK_IRFailure *failure,
                             const On1xSDK_DiagnosticSink *diagnostics) {
    if (snapshot.nodes.size() > limits.max_nodes) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_LIMIT);
        return ON1X_SDK_LIMIT;
    }
    if (snapshot.provenance.size() > ON1X_SDK_IR_MAX_PROVENANCE_BYTES ||
        !valid_utf8(snapshot.provenance)) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_MALFORMED);
        return ON1X_SDK_ERROR;
    }
    std::vector<unsigned char> color(snapshot.nodes.size());
    std::size_t work = 0;
    struct Frame {
        std::uint64_t id;
        std::size_t next;
        std::size_t depth;
    };
    for (const auto root : snapshot.functions) {
        if (!root || root > snapshot.nodes.size() ||
            snapshot.nodes[root - 1].node_class != ON1X_SDK_HIR_NODE_FUNCTION) {
            set_failure(failure, ON1X_SDK_IR_FAILURE_MALFORMED);
            return ON1X_SDK_ERROR;
        }
        if (color[root - 1]) {
            set_failure(failure, ON1X_SDK_IR_FAILURE_MALFORMED);
            return ON1X_SDK_ERROR;
        }
        std::vector<Frame> stack{{root, 0, 0}};
        while (!stack.empty()) {
            auto &frame = stack.back();
            if (frame.depth > limits.max_depth) {
                set_failure(failure, ON1X_SDK_IR_FAILURE_LIMIT);
                return ON1X_SDK_LIMIT;
            }
            if (color[frame.id - 1] == 0) {
                if (++work > limits.max_work) {
                    set_failure(failure, ON1X_SDK_IR_FAILURE_LIMIT);
                    return ON1X_SDK_LIMIT;
                }
                color[frame.id - 1] = 1;
                const auto &node = snapshot.nodes[frame.id - 1];
                const bool valid_kind =
                    (node.node_class == ON1X_SDK_HIR_NODE_FUNCTION &&
                     node.kind == 0) ||
                    (node.node_class == ON1X_SDK_HIR_NODE_PARAMETER &&
                     node.kind == 0) ||
                    (node.node_class == ON1X_SDK_HIR_NODE_CAPTURE &&
                     node.kind == 0) ||
                    (node.node_class == ON1X_SDK_HIR_NODE_STATEMENT &&
                     node.kind <= static_cast<std::uint32_t>(
                         Stmt::Kind::CspScope)) ||
                    (node.node_class == ON1X_SDK_HIR_NODE_EXPRESSION &&
                     node.kind <= static_cast<std::uint32_t>(
                         Expr::Kind::RegexCapture)) ||
                    (node.node_class == ON1X_SDK_HIR_NODE_PATTERN &&
                     node.kind <= static_cast<std::uint32_t>(
                         Pattern::Kind::As));
                if (node.id != frame.id || !valid_kind ||
                    (node.flags & ~allowed_flags(node.node_class)) ||
                    node.labels.size() > 256 ||
                    ((node.node_class == ON1X_SDK_HIR_NODE_FUNCTION ||
                      node.node_class == ON1X_SDK_HIR_NODE_PARAMETER ||
                      node.node_class == ON1X_SDK_HIR_NODE_CAPTURE) &&
                     !node.labels.empty()) ||
                    (node.node_class == ON1X_SDK_HIR_NODE_STATEMENT &&
                     node.labels.size() > 1) ||
                    node.name.size() >
                    ON1X_SDK_IR_MAX_STRING_BYTES ||
                    !valid_utf8(node.name) ||
                    node.name.find('\0') != std::string::npos ||
                    node.where.file.size() > ON1X_SDK_IR_MAX_STRING_BYTES ||
                    !valid_utf8(node.where.file) ||
                    node.where.file.find('\0') != std::string::npos ||
                    !node.where.line ||
                    !node.where.column) {
                    set_failure(failure, ON1X_SDK_IR_FAILURE_MALFORMED);
                    emit_diagnostic(diagnostics, "ON1X-SDK-HIR-STRUCTURE",
                                    "malformed HIR node metadata", &node.where);
                    return ON1X_SDK_ERROR;
                }
                TypeFormOptions options;
                options.allow_holes = !snapshot.wire_complete;
                options.allow_generics = !snapshot.wire_complete;
                options.allow_named = !snapshot.wire_complete;
                options.max_depth = limits.max_depth;
                options.max_nodes = limits.max_nodes;
                const bool intrinsic_name =
                    node.node_class == ON1X_SDK_HIR_NODE_EXPRESSION &&
                    node.kind == static_cast<std::uint32_t>(
                        Expr::Kind::Variable) &&
                    node.auxiliary[0] == static_cast<std::uint64_t>(
                        Expr::Reference::Intrinsic);
                if (snapshot.wire_complete &&
                    node.node_class != ON1X_SDK_HIR_NODE_STATEMENT &&
                    !intrinsic_name && node.type == Type::Invalid) {
                    set_failure(failure, ON1X_SDK_IR_FAILURE_MALFORMED);
                    emit_diagnostic(
                        diagnostics, "ON1X-SDK-HIR-STRUCTURE",
                        "elaborated HIR node class " +
                            std::to_string(node.node_class) + " kind " +
                            std::to_string(node.kind) +
                            " ('" + node.name +
                            "') is missing its canonical type",
                        &node.where);
                    return ON1X_SDK_ERROR;
                }
                if (node.type != Type::Invalid) {
                    const auto form = check_type_form(node.type, options);
                    if (form.status == TypeFormStatus::Limit) {
                        set_failure(failure, ON1X_SDK_IR_FAILURE_LIMIT);
                        return ON1X_SDK_LIMIT;
                    }
                    if (!form) {
                        set_failure(failure, ON1X_SDK_IR_FAILURE_MALFORMED);
                        return ON1X_SDK_ERROR;
                    }
                    if (form.nodes > limits.max_work - work) {
                        set_failure(failure, ON1X_SDK_IR_FAILURE_LIMIT);
                        return ON1X_SDK_LIMIT;
                    }
                    work += form.nodes;
                }
                for (const auto &label : node.labels)
                    if (label.size() > ON1X_SDK_IR_MAX_STRING_BYTES ||
                        !valid_utf8(label) ||
                        label.find('\0') != std::string::npos) {
                        set_failure(failure, ON1X_SDK_IR_FAILURE_MALFORMED);
                        return ON1X_SDK_ERROR;
                    }
                for (const auto &edge : node.children) {
                    if (edge.kind < ON1X_SDK_HIR_EDGE_PARAMETER ||
                        edge.kind > ON1X_SDK_HIR_EDGE_CAPTURE) {
                        set_failure(
                            failure, ON1X_SDK_IR_FAILURE_MALFORMED);
                        return ON1X_SDK_ERROR;
                    }
                    if (!edge.target || edge.target > snapshot.nodes.size() ||
                        !valid_edge(node, edge,
                                    snapshot.nodes[edge.target - 1])) {
                        set_failure(
                            failure, ON1X_SDK_IR_FAILURE_MALFORMED);
                        emit_diagnostic(
                            diagnostics, "ON1X-SDK-HIR-STRUCTURE",
                            "invalid HIR edge for parent/child node classes",
                            &node.where);
                        return ON1X_SDK_ERROR;
                    }
                }
            }
            const auto &children = snapshot.nodes[frame.id - 1].children;
            if (frame.next == children.size()) {
                color[frame.id - 1] = 2;
                stack.pop_back();
                continue;
            }
            const auto child = children[frame.next++].target;
            if (!child || child > snapshot.nodes.size() ||
                color[child - 1] != 0) {
                set_failure(failure, ON1X_SDK_IR_FAILURE_MALFORMED);
                return ON1X_SDK_ERROR;
            }
            stack.push_back({child, 0, frame.depth + 1});
        }
    }
    if (std::find(color.begin(), color.end(), 0) != color.end()) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_MALFORMED);
        return ON1X_SDK_ERROR;
    }
    return ON1X_SDK_OK;
}

} // namespace

On1xSDK_Status check_hir_candidate(
    const Unit &candidate,
    const On1xSDK_IRIdentity &identity_record,
    std::string_view provenance_view,
    On1xSDK_HIRSnapshot **out,
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
    try {
        std::size_t budget = ON1X_SDK_IR_MAX_NODES;
        auto working = clone_unit(candidate, budget);
        check(working);
        for (const auto &function : working.functions)
            if (function.external || function.kernel_link_only ||
                !function.kernel_symbol.empty())
                throw Error(
                    function.where,
                    "SDK checked-HIR bridge requires separate host foreign "
                    "admission evidence",
                    "ON1X-MIR-UNSUPPORTED");
        verify_nominals(working);
        cliches::verify_elaborated(working);
        inherent::verify_elaborated(working);
        canonical::verify_control(working);
        canonical::verify_values(working);
        csp::verify(working, true);
        extensions::verify_evidence(working);

        auto canonical =
            std::make_shared<const Unit>(std::move(working));
        Flatten flattened(*canonical);
        flattened.run();
        identity.state = ON1X_SDK_IR_CHECKED_HIR;
        auto snapshot = std::unique_ptr<On1xSDK_HIRSnapshot>(
            make_hir_snapshot(identity, std::move(provenance),
                              flattened.take_nodes(),
                              flattened.take_functions(),
                              flattened.complete(),
                              std::move(canonical)));
        const On1xSDK_IRLimits limits{
            ON1X_SDK_SEMANTICS_IR_ABI_VERSION,
            sizeof(On1xSDK_IRLimits),
            ON1X_SDK_IR_MAX_NODES,
            ON1X_SDK_IR_MAX_NODES * 16u,
            ON1X_SDK_IR_MAX_DEPTH
        };
        if (const auto status =
                validate_tree(*snapshot, limits, failure, diagnostics);
            status != ON1X_SDK_OK)
            return status;
        *out = snapshot.release();
        return ON1X_SDK_OK;
    } catch (const Error &error) {
        const auto status = error_status(error);
        set_failure(
            failure,
            status == ON1X_SDK_LIMIT
                ? ON1X_SDK_IR_FAILURE_LIMIT
                : status == ON1X_SDK_UNSUPPORTED
                      ? ON1X_SDK_IR_FAILURE_UNSUPPORTED
                      : ON1X_SDK_IR_FAILURE_SEMANTIC);
        emit_diagnostic(diagnostics, "ON1X-SDK-HIR-CHECK",
                        error.what(), &error.where);
        return status;
    } catch (const std::bad_alloc &) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_LIMIT);
        return ON1X_SDK_LIMIT;
    } catch (...) {
        set_failure(failure, ON1X_SDK_IR_FAILURE_SEMANTIC);
        return ON1X_SDK_ERROR;
    }
}

} // namespace on1x::sdk::ir

extern "C" {

void on1x_sdk_hir_snapshot_retain(On1xSDK_HIRSnapshot *snapshot) {
    if (!snapshot) return;
    auto current = snapshot->references.load(std::memory_order_relaxed);
    while (current != std::numeric_limits<std::size_t>::max() &&
           !snapshot->references.compare_exchange_weak(
               current, current + 1, std::memory_order_relaxed)) {}
}

void on1x_sdk_hir_snapshot_release(On1xSDK_HIRSnapshot *snapshot) {
    if (snapshot &&
        snapshot->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete snapshot;
}

On1xSDK_Status on1x_sdk_hir_snapshot_identity(
    const On1xSDK_HIRSnapshot *snapshot,
    On1xSDK_IRIdentity *out_identity) {
    if (!snapshot || !out_identity) return ON1X_SDK_INVALID_ARGUMENT;
    on1x::sdk::ir::write_identity(snapshot->identity, *out_identity);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_snapshot_provenance(
    const On1xSDK_HIRSnapshot *snapshot,
    On1xSDK_IRStringView *out_provenance) {
    if (!snapshot || !out_provenance) return ON1X_SDK_INVALID_ARGUMENT;
    *out_provenance = {snapshot->provenance.data(),
                       snapshot->provenance.size()};
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_snapshot_node_count(
    const On1xSDK_HIRSnapshot *snapshot,
    size_t *out_count) {
    if (!snapshot || !out_count) return ON1X_SDK_INVALID_ARGUMENT;
    *out_count = snapshot->nodes.size();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_snapshot_function_count(
    const On1xSDK_HIRSnapshot *snapshot,
    size_t *out_count) {
    if (!snapshot || !out_count) return ON1X_SDK_INVALID_ARGUMENT;
    *out_count = snapshot->functions.size();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_snapshot_function(
    const On1xSDK_HIRSnapshot *snapshot,
    size_t index,
    On1xSDK_HIRNodeRef *out_function) {
    if (!snapshot || !out_function) return ON1X_SDK_INVALID_ARGUMENT;
    if (index >= snapshot->functions.size())
        return ON1X_SDK_INVALID_ARGUMENT;
    *out_function =
        on1x::sdk::ir::hir_ref(*snapshot, snapshot->functions[index]);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_node_class(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *reference,
    On1xSDK_HIRNodeClass *out_class) {
    if (!out_class) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::sdk::ir::HIRNode *node = nullptr;
    if (const auto status =
            on1x::sdk::ir::hir_node(snapshot, reference, node);
        status != ON1X_SDK_OK)
        return status;
    *out_class = node->node_class;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_node_kind(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *reference,
    uint32_t *out_kind) {
    if (!out_kind) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::sdk::ir::HIRNode *node = nullptr;
    if (const auto status =
            on1x::sdk::ir::hir_node(snapshot, reference, node);
        status != ON1X_SDK_OK)
        return status;
    *out_kind = node->kind;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_node_flags(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *reference,
    On1xSDK_HIRNodeFlags *out_flags) {
    if (!out_flags) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::sdk::ir::HIRNode *node = nullptr;
    if (const auto status =
            on1x::sdk::ir::hir_node(snapshot, reference, node);
        status != ON1X_SDK_OK)
        return status;
    *out_flags = node->flags;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_node_name(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *reference,
    On1xSDK_IRStringView *out_name) {
    if (!out_name) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::sdk::ir::HIRNode *node = nullptr;
    if (const auto status =
            on1x::sdk::ir::hir_node(snapshot, reference, node);
        status != ON1X_SDK_OK)
        return status;
    *out_name = {node->name.data(), node->name.size()};
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_node_span(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *reference,
    On1xSDK_IRSourceSpan *out_span) {
    if (!out_span) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::sdk::ir::HIRNode *node = nullptr;
    if (const auto status =
            on1x::sdk::ir::hir_node(snapshot, reference, node);
        status != ON1X_SDK_OK)
        return status;
    *out_span = on1x::sdk::ir::span(node->where);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_node_type(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *reference,
    On1xSDK_Type **out_type) {
    if (!out_type) return ON1X_SDK_INVALID_ARGUMENT;
    *out_type = nullptr;
    const on1x::sdk::ir::HIRNode *node = nullptr;
    if (const auto status =
            on1x::sdk::ir::hir_node(snapshot, reference, node);
        status != ON1X_SDK_OK)
        return status;
    if (node->type == on1x::Type::Invalid) return ON1X_SDK_UNSUPPORTED;
    return on1x::sdk::ir::guard([&] {
        *out_type = on1x::sdk::ir::make_type(snapshot->identity, node->type);
        return ON1X_SDK_OK;
    });
}

On1xSDK_Status on1x_sdk_hir_node_child_count(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *reference,
    size_t *out_count) {
    if (!out_count) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::sdk::ir::HIRNode *node = nullptr;
    if (const auto status =
            on1x::sdk::ir::hir_node(snapshot, reference, node);
        status != ON1X_SDK_OK)
        return status;
    *out_count = node->children.size();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_node_child(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_HIRNodeRef *reference,
    size_t index,
    On1xSDK_HIREdgeKind *out_edge,
    On1xSDK_HIRNodeRef *out_child) {
    if (!out_edge || !out_child) return ON1X_SDK_INVALID_ARGUMENT;
    const on1x::sdk::ir::HIRNode *node = nullptr;
    if (const auto status =
            on1x::sdk::ir::hir_node(snapshot, reference, node);
        status != ON1X_SDK_OK)
        return status;
    if (index >= node->children.size()) return ON1X_SDK_INVALID_ARGUMENT;
    *out_edge = node->children[index].kind;
    *out_child =
        on1x::sdk::ir::hir_ref(*snapshot, node->children[index].target);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_visit(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_IRLimits *limits_record,
    On1xSDK_HIRVisitFn visitor,
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
            limits_record, ON1X_SDK_IR_MAX_NODES,
            ON1X_SDK_IR_MAX_NODES * 4u, ON1X_SDK_IR_MAX_DEPTH, limits);
        status != ON1X_SDK_OK) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return status;
    }
    try {
        struct Item { std::uint64_t id; std::size_t depth; };
        std::vector<Item> pending;
        pending.reserve(std::min(snapshot->nodes.size(), limits.max_nodes));
        for (auto iter = snapshot->functions.rbegin();
             iter != snapshot->functions.rend(); ++iter)
            pending.push_back({*iter, 0});
        std::size_t nodes = 0, work = 0;
        while (!pending.empty()) {
            const auto item = pending.back();
            pending.pop_back();
            if (++nodes > limits.max_nodes || item.depth > limits.max_depth ||
                ++work > limits.max_work) {
                set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
                return ON1X_SDK_LIMIT;
            }
            if (!item.id || item.id > snapshot->nodes.size()) {
                set_failure(out_failure, ON1X_SDK_IR_FAILURE_MALFORMED);
                return ON1X_SDK_ERROR;
            }
            const auto reference = hir_ref(*snapshot, item.id);
            On1xSDK_Status callback = ON1X_SDK_ERROR;
            try {
                callback = normalize_status(
                    visitor(userdata, snapshot, &reference, item.depth));
            } catch (...) {
                callback = ON1X_SDK_ERROR;
            }
            if (callback != ON1X_SDK_OK) {
                set_failure(out_failure, ON1X_SDK_IR_FAILURE_CALLBACK);
                return callback;
            }
            const auto &children = snapshot->nodes[item.id - 1].children;
            if (children.size() > limits.max_work - work) {
                set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
                return ON1X_SDK_LIMIT;
            }
            work += children.size();
            for (auto iter = children.rbegin();
                 iter != children.rend(); ++iter)
                pending.push_back({iter->target, item.depth + 1});
        }
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
        return ON1X_SDK_LIMIT;
    } catch (const std::length_error &) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
        return ON1X_SDK_LIMIT;
    } catch (...) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_MALFORMED);
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_sdk_hir_verify_structure(
    const On1xSDK_HIRSnapshot *snapshot,
    const On1xSDK_IRLimits *limits_record,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics) {
    using namespace on1x::sdk::ir;
    set_failure(out_failure, ON1X_SDK_IR_FAILURE_NONE);
    if (!snapshot) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    On1xSDK_IRLimits limits{};
    if (const auto status = read_limits(
            limits_record, ON1X_SDK_IR_MAX_NODES,
            ON1X_SDK_IR_MAX_NODES * 8u, ON1X_SDK_IR_MAX_DEPTH, limits);
        status != ON1X_SDK_OK) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return status;
    }
    const auto status = guard([&] {
        return validate_tree(*snapshot, limits, out_failure, diagnostics);
    });
    if (status == ON1X_SDK_LIMIT &&
        (!out_failure || *out_failure == ON1X_SDK_IR_FAILURE_NONE))
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
    else if (status != ON1X_SDK_OK &&
             (!out_failure || *out_failure == ON1X_SDK_IR_FAILURE_NONE))
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_MALFORMED);
    return status;
}

} // extern "C"
