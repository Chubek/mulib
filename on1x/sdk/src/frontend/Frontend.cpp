#include "../compiler/FrontendInternal.hpp"

#include "On1xSDK-ASTBuilder.h"
#include "On1xSDK-ASTVisitor.h"
#include "On1xSDK-Attribute.h"
#include "On1xSDK-Elaborator.h"
#include "On1xSDK-Frontend.h"
#include "On1xSDK-Hygiene.h"
#include "On1xSDK-Macro.h"
#include "On1xSDK-Syntax.h"

#include <on1x/clone.hpp>
#include <on1x/meta.hpp>
#include <on1x/numeric.hpp>

#include <cstring>
#include <unordered_set>

namespace on1x::sdk2 {
namespace {

std::string type_tag(const on1x::Type &type) {
    using Kind = on1x::Type::Kind;
    switch (type.kind()) {
    case Kind::Invalid: return "inferred_type";
    case Kind::Hole: return "type_hole";
    case Kind::I64: return "i64_type";
    case Kind::F64: return "f64_type";
    case Kind::F32: return "f32_type";
    case Kind::U8: return "u8_type";
    case Kind::U16: return "u16_type";
    case Kind::U32: return "u32_type";
    case Kind::U64: return "u64_type";
    case Kind::Bool: return "bool_type";
    case Kind::String: return "string_type";
    case Kind::Char: return "character_type";
    case Kind::Void: return "void_type";
    case Kind::Json: return "json_type";
    case Kind::JsonView: return "json_view_type";
    case Kind::SExpr: return "sexpr_type";
    case Kind::Buffer: return "buffer_type";
    case Kind::Function: return "function_type";
    case Kind::Tuple: return "tuple_type";
    case Kind::Code: return "code_type";
    case Kind::Generic: return "generic_type";
    case Kind::Bound: return "bound_type";
    case Kind::Forall: return "forall_type";
    case Kind::Named: return "named_type";
    case Kind::Own: return "own_type";
    case Kind::Record: return "record_type";
    case Kind::Sum: return "sum_type";
    case Kind::Array: return "array_type";
    case Kind::Slice: return "slice_type";
    case Kind::Fiber: return "fiber_type";
    case Kind::Channel: return "channel_type";
    case Kind::Sender: return "sender_type";
    case Kind::Receiver: return "receiver_type";
    case Kind::CspGroup: return "csp_group_type";
    case Kind::ExternalFiber: return "external_fiber_type";
    case Kind::ByteSender: return "byte_sender_type";
    case Kind::ByteReceiver: return "byte_receiver_type";
    case Kind::Regex: return "regex_type";
    case Kind::RegexCaptures: return "regex_captures_type";
    default: return "internal_type";
    }
}

std::string expression_tag(on1x::Expr::Kind kind) {
    using Kind = on1x::Expr::Kind;
    switch (kind) {
    case Kind::Integer: return "integer";
    case Kind::Float: return "float";
    case Kind::Boolean: return "boolean";
    case Kind::String: return "string";
    case Kind::Variable: return "identifier";
    case Kind::ClicheMember: return "cliche_member";
    case Kind::InherentMember: return "inherent_member";
    case Kind::Unary: return "unary";
    case Kind::Binary: return "binary";
    case Kind::Call: return "call";
    case Kind::Lambda: return "lambda";
    case Kind::Tuple: return "tuple";
    case Kind::PolyPack: return "poly_pack";
    case Kind::PolyProjection: return "poly_projection";
    case Kind::MacroCall: return "macro_call";
    case Kind::SyntaxHole: return "syntax_hole";
    case Kind::SyntaxBlock: return "syntax_block";
    case Kind::Extension: return "extension_expression";
    case Kind::SyntaxList: return "syntax_list";
    case Kind::SyntaxVector: return "syntax_vector";
    case Kind::SyntaxKeyword: return "syntax_keyword";
    case Kind::Quote: return "quote";
    case Kind::Splice: return "splice";
    case Kind::Lift: return "lift";
    case Kind::Run: return "run";
    case Kind::SyntaxLet: return "syntax_let";
    case Kind::Record: return "record";
    case Kind::Variant: return "variant";
    case Kind::Match: return "value_match";
    case Kind::Array: return "array";
    case Kind::Field: return "field";
    case Kind::Index: return "index";
    case Kind::Slice: return "slice";
    case Kind::Length: return "length";
    case Kind::PatternMatch: return "pattern_match";
    case Kind::Case: return "case";
    case Kind::Unit: return "unit_value";
    case Kind::Region: return "region";
    case Kind::TagTest: return "tag_test";
    case Kind::Payload: return "payload";
    case Kind::Project: return "project";
    case Kind::Loop: return "loop_value";
    case Kind::Propagate: return "propagate";
    case Kind::SourceCsp: return "source_csp";
    case Kind::Csp: return "csp";
    case Kind::UnsafeBlock: return "unsafe_expression";
    case Kind::Character: return "character";
    case Kind::ByteBuffer: return "byte_buffer";
    case Kind::Interpolated: return "interpolated_string";
    case Kind::RegexLiteral: return "regex_literal";
    case Kind::RegexMatch: return "regex_match";
    case Kind::RegexCapture: return "regex_capture";
    }
    return "unknown_expression";
}

std::string statement_tag(on1x::Stmt::Kind kind) {
    using Kind = on1x::Stmt::Kind;
    switch (kind) {
    case Kind::Let: return "let";
    case Kind::Assign: return "assign";
    case Kind::Return: return "return";
    case Kind::If: return "if";
    case Kind::While: return "while";
    case Kind::Expr: return "expression_statement";
    case Kind::Block: return "block";
    case Kind::Template: return "template";
    case Kind::Range: return "range";
    case Kind::Break: return "break";
    case Kind::Continue: return "continue";
    case Kind::Storage: return "storage";
    case Kind::Initialize: return "initialize";
    case Kind::Region: return "region_statement";
    case Kind::Leave: return "leave";
    case Kind::Trap: return "trap";
    case Kind::Defer: return "defer";
    case Kind::PushDefer: return "push_defer";
    case Kind::TaskGroup: return "taskgroup";
    case Kind::CspScope: return "csp_scope";
    }
    return "unknown_statement";
}

std::string pattern_tag(on1x::Pattern::Kind kind) {
    using Kind = on1x::Pattern::Kind;
    switch (kind) {
    case Kind::Name: return "name_pattern";
    case Kind::Tuple: return "tuple_pattern";
    case Kind::Wildcard: return "wildcard_pattern";
    case Kind::Rest: return "rest_pattern";
    case Kind::Literal: return "literal_pattern";
    case Kind::Variant: return "variant_pattern";
    case Kind::Array: return "array_pattern";
    case Kind::Record: return "record_pattern";
    case Kind::Or: return "or_pattern";
    case Kind::As: return "as_pattern";
    }
    return "unknown_pattern";
}

class Flattener {
    std::shared_ptr<ASTData> ast;
    Operation operation;
    std::size_t maximum;

    std::uint32_t add(
        On1xSDK_ASTNodeCategory category,
        std::string tag,
        std::string text,
        on1x::Location location,
        std::uint64_t context = 0,
        On1xSDK_ASTNodeFlags flags = 0) {
        operation.charge(1);
        if (ast->nodes.size() >= maximum)
            fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                 "ON1X-SDK-AST-LIMIT", "AST node budget exhausted");
        NodeData node;
        node.category = category;
        node.flags = flags;
        node.tag = std::move(tag);
        node.text = std::move(text);
        node.hygiene_context = context;
        node.location = std::move(location);
        node.byte_end = node.location.offset;
        if (!node.location.file.empty()) {
            auto source = ast->source_by_path.find(node.location.file);
            if (source != ast->source_by_path.end()) {
                if (node.location.offset > source->second->bytes.size())
                    fail(ON1X_SDK_ERROR,
                         ON1X_SDK_COMPILER_VERIFICATION_FAILED,
                         "ON1X-SDK-SPAN",
                         "AST location exceeds its immutable source revision");
                const auto [line, column] =
                    line_column(*source->second, node.location.offset);
                if (ast->validation == ON1X_SDK_AST_PPX_READMITTED_CANDIDATE &&
                    (node.location.line != line ||
                     node.location.column != column))
                    fail(ON1X_SDK_ERROR,
                         ON1X_SDK_COMPILER_VERIFICATION_FAILED,
                         "ON1X-SDK-SPAN",
                         "PPX candidate location has inconsistent line/column");
                node.source_id = source->second->source_id;
                node.source_revision = source->second->revision;
            } else if (
                ast->validation == ON1X_SDK_AST_PPX_READMITTED_CANDIDATE &&
                !node.location.file.starts_with("<")) {
                fail(ON1X_SDK_ERROR,
                     ON1X_SDK_COMPILER_VERIFICATION_FAILED,
                     "ON1X-SDK-SPAN",
                     "PPX candidate location names an unknown source");
            }
        }
        ast->nodes.push_back(std::move(node));
        return static_cast<std::uint32_t>(ast->nodes.size() - 1u);
    }

    void link(std::uint32_t parent, std::uint32_t child) {
        ast->nodes.at(parent).children.push_back(child);
    }

    std::uint32_t type(
        const on1x::Type &value,
        const on1x::Location &where) {
        auto index = add(
            ON1X_SDK_AST_TYPE_SYNTAX, type_tag(value),
            on1x::type_name(value), where);
        for (const auto &argument : value.arguments())
            link(index, type(argument, where));
        if (value.is_function()) {
            for (const auto &parameter : value.parameters())
                link(index, type(parameter, where));
            link(index, type(value.result(), where));
        } else if (value.is_forall()) {
            link(index, type(value.body(), where));
        } else if (value.is_code()) {
            link(index, type(value.code_result(), where));
        } else if (value.is_own()) {
            link(index, type(value.owned(), where));
        } else if (value.is_tuple() || value.is_data()) {
            for (std::size_t i = 0; i < value.elements().size(); ++i) {
                auto child = type(value.elements()[i], where);
                if (value.is_data() &&
                    i < value.labels().size() &&
                    !value.labels()[i].empty()) {
                    auto field = add(
                        ON1X_SDK_AST_TYPE_SYNTAX, "type_field",
                        value.labels()[i], where);
                    link(field, child);
                    child = field;
                }
                link(index, child);
            }
        }
        return index;
    }

    std::uint32_t constraint(const on1x::ClicheConstraint &value) {
        auto index = add(
            ON1X_SDK_AST_CONSTRAINT, "constraint", value.name, value.where);
        for (const auto &argument : value.arguments)
            link(index, type(argument, value.where));
        return index;
    }

    void type_parameters(
        std::uint32_t parent,
        const std::vector<on1x::TypeParameter> &parameters) {
        for (const auto &parameter : parameters) {
            auto index = add(
                ON1X_SDK_AST_PARAMETER, "type_parameter",
                parameter.name, parameter.where);
            for (const auto &bound : parameter.bounds)
                link(index, constraint(bound));
            link(parent, index);
        }
    }

    std::uint32_t pattern(const on1x::Pattern &value) {
        On1xSDK_ASTNodeFlags flags =
            value.open
                ? static_cast<On1xSDK_ASTNodeFlags>(
                      ON1X_SDK_AST_FLAG_OPEN)
                : On1xSDK_ASTNodeFlags{0};
        auto index = add(
            ON1X_SDK_AST_PATTERN, pattern_tag(value.kind),
            value.name, value.where, value.context, flags);
        if (value.type != on1x::Type::Invalid)
            link(index, type(value.type, value.where));
        for (std::size_t i = 0; i < value.elements.size(); ++i) {
            auto child = pattern(value.elements[i]);
            if (i < value.labels.size() && !value.labels[i].empty()) {
                auto field = add(
                    ON1X_SDK_AST_PATTERN, "pattern_field",
                    value.labels[i], value.where);
                link(field, child);
                child = field;
            }
            link(index, child);
        }
        return index;
    }

    std::uint32_t parameter(const on1x::Param &value) {
        auto index = add(
            ON1X_SDK_AST_PARAMETER,
            value.pattern ? "pattern_parameter"
                          : value.exclusive ? "exclusive_parameter"
                                            : "parameter",
            value.name, value.where, value.context);
        if (value.pattern) link(index, pattern(*value.pattern));
        link(index, type(value.type, value.where));
        return index;
    }

    std::uint32_t macro_parameter(const on1x::MacroParameter &value) {
        static constexpr const char *tags[] = {
            "macro_capture", "macro_literal", "macro_wildcard",
            "macro_list", "macro_vector", "macro_tuple", "macro_call",
            "macro_unary", "macro_binary"};
        const auto raw = static_cast<std::size_t>(value.kind);
        auto index = add(
            ON1X_SDK_AST_PATTERN,
            raw < std::size(tags) ? tags[raw] : "macro_pattern",
            value.name.empty() ? value.category : value.name,
            value.where, 0,
            value.repeated
                ? static_cast<On1xSDK_ASTNodeFlags>(
                      ON1X_SDK_AST_FLAG_REPEATED)
                : On1xSDK_ASTNodeFlags{0});
        if (value.literal) link(index, expression(*value.literal));
        for (const auto &element : value.elements)
            link(index, macro_parameter(element));
        if (value.tail) link(index, macro_parameter(*value.tail));
        return index;
    }

    std::uint32_t macro_rule(const on1x::MacroRule &value) {
        auto index = add(
            ON1X_SDK_AST_MACRO_RULE,
            value.block ? "macro_block_rule" : "macro_expression_rule",
            {}, value.where);
        for (const auto &parameter : value.parameters)
            link(index, macro_parameter(parameter));
        if (value.tail) link(index, macro_parameter(*value.tail));
        if (value.expression) link(index, expression(*value.expression));
        for (const auto &item : value.body) link(index, statement(item));
        return index;
    }

    std::uint32_t macro(const on1x::Macro &value) {
        auto index = add(
            ON1X_SDK_AST_MACRO, "macro", value.name, value.where);
        ast->macros.push_back(index);
        link(index, add(
            ON1X_SDK_AST_PATTERN, "macro_ellipsis",
            value.ellipsis, value.where));
        for (const auto &literal : value.literals)
            link(index, add(
                ON1X_SDK_AST_PATTERN, "macro_literal_name",
                literal, value.where));
        for (const auto &rule : value.rules) link(index, macro_rule(rule));
        return index;
    }

    std::uint32_t expression(const on1x::Expr &value) {
        On1xSDK_ASTNodeFlags flags =
            value.unsafe_region || value.kind == on1x::Expr::Kind::UnsafeBlock
                ? static_cast<On1xSDK_ASTNodeFlags>(
                      ON1X_SDK_AST_FLAG_UNSAFE)
                : On1xSDK_ASTNodeFlags{0};
        if (value.syntax_repeat)
            flags |= ON1X_SDK_AST_FLAG_REPEATED;
        if (value.statement_match)
            flags |= ON1X_SDK_AST_FLAG_STATEMENT_FORM;
        if (value.sum_match)
            flags |= ON1X_SDK_AST_FLAG_SUM_FORM;
        auto index = add(
            ON1X_SDK_AST_EXPRESSION, expression_tag(value.kind),
            value.text, value.where, value.context, flags);
        type_parameters(index, value.type_parameters);
        for (const auto &item : value.constraints)
            link(index, constraint(item));
        for (const auto &item : value.patterns)
            link(index, pattern(item));
        for (const auto &item : value.parameters)
            link(index, parameter(item));
        if (value.type != on1x::Type::Invalid)
            link(index, type(value.type, value.where));
        if (value.kind == on1x::Expr::Kind::Lambda ||
            value.kind == on1x::Expr::Kind::SourceCsp ||
            value.kind == on1x::Expr::Kind::Record ||
            value.kind == on1x::Expr::Kind::Variant)
            link(index, type(value.result, value.where));
        for (const auto &argument : value.cliche_arguments)
            link(index, type(argument, value.where));
        for (const auto &argument : value.member_type_arguments)
            link(index, type(argument, value.where));
        if (!value.cliche_member.empty())
            link(index, add(
                ON1X_SDK_AST_EXPRESSION, "cliche_member_name",
                value.cliche_member, value.where));
        if (value.syntax_repeat)
            link(index, add(
                ON1X_SDK_AST_EXPRESSION, "syntax_repeat_depth",
                std::to_string(value.syntax_repeat), value.where));
        if (value.callee) {
            auto callee = add(
                ON1X_SDK_AST_EXPRESSION, "callee", {}, value.callee->where);
            link(callee, expression(*value.callee));
            link(index, callee);
        }
        for (std::size_t i = 0; i < value.children.size(); ++i) {
            if (!value.children[i]) continue;
            auto child = expression(*value.children[i]);
            if (i < value.labels.size() && !value.labels[i].empty()) {
                auto field = add(
                    ON1X_SDK_AST_EXPRESSION, "labeled_operand",
                    value.labels[i], value.where);
                link(field, child);
                child = field;
            }
            link(index, child);
        }
        for (const auto &proof : value.proofs)
            if (proof) link(index, expression(*proof));
        for (const auto &definition : value.syntax_definitions)
            link(index, macro(definition));
        if (value.syntax_tail)
            link(index, expression(*value.syntax_tail));
        for (const auto &item : value.body)
            link(index, statement(item));
        return index;
    }

    std::uint32_t statement(const on1x::Stmt &value) {
        On1xSDK_ASTNodeFlags flags = 0;
        if (value.mutable_binding) flags |= ON1X_SDK_AST_FLAG_MUTABLE;
        if (value.unsafe_block) flags |= ON1X_SDK_AST_FLAG_UNSAFE;
        if (value.inclusive) flags |= ON1X_SDK_AST_FLAG_INCLUSIVE;
        if (value.ref_store) flags |= ON1X_SDK_AST_FLAG_REFERENCE;
        auto index = add(
            ON1X_SDK_AST_STATEMENT, statement_tag(value.kind),
            value.name.empty() ? value.label : value.name,
            value.where, value.context, flags);
        if (!value.name.empty() && !value.label.empty())
            link(index, add(
                ON1X_SDK_AST_STATEMENT, "label", value.label,
                value.where, value.label_context));
        if (value.pattern) link(index, pattern(*value.pattern));
        if (value.declared != on1x::Type::Invalid)
            link(index, type(value.declared, value.where));
        if (value.value) link(index, expression(*value.value));
        for (const auto &item : value.body)
            link(index, statement(item));
        if (!value.otherwise.empty()) {
            auto alternative = add(
                ON1X_SDK_AST_STATEMENT, "otherwise", {}, value.where);
            for (const auto &item : value.otherwise)
                link(alternative, statement(item));
            link(index, alternative);
        }
        return index;
    }

    std::uint32_t function(
        const on1x::Function &value,
        std::string_view tag = "function") {
        On1xSDK_ASTNodeFlags flags = 0;
        if (value.external) flags |= ON1X_SDK_AST_FLAG_EXTERNAL;
        if (value.extension_generated)
            flags |= ON1X_SDK_AST_FLAG_GENERATED;
        auto index = add(
            ON1X_SDK_AST_DECLARATION, std::string(tag),
            value.name, value.where, 0, flags);
        type_parameters(index, value.type_parameters);
        for (const auto &item : value.constraints)
            link(index, constraint(item));
        for (const auto &item : value.params)
            link(index, parameter(item));
        link(index, type(value.result, value.where));
        for (const auto &attribute : value.attributes) {
            if (!attribute) continue;
            auto child = expression(*attribute);
            ast->nodes[child].category = ON1X_SDK_AST_ATTRIBUTE;
            ast->nodes[child].tag = "attribute";
            ast->attributes.push_back(child);
            link(index, child);
        }
        for (const auto &item : value.body)
            link(index, statement(item));
        return index;
    }

    void declarations(std::uint32_t root, const on1x::Unit &unit) {
        for (const auto &item : unit.module_imports) {
            auto index = add(
                ON1X_SDK_AST_IMPORT,
                item.quoted ? "quoted_import"
                            : item.selective ? "selective_import" : "import",
                item.module, item.where);
            if (item.relative)
                link(index, add(
                    ON1X_SDK_AST_IMPORT, "relative_import_depth",
                    std::to_string(item.relative), item.where));
            if (!item.alias.empty())
                link(index, add(
                    ON1X_SDK_AST_IMPORT, "import_alias",
                    item.alias, item.where));
            for (const auto &name : item.names) {
                auto child = add(
                    ON1X_SDK_AST_IMPORT, "import_name",
                    name.alias.empty() ? name.name
                                       : name.name + " as " + name.alias,
                    item.where);
                link(index, child);
            }
            link(root, index);
        }
        for (const auto &item : unit.kernel_imports) {
            auto index = add(
                ON1X_SDK_AST_IMPORT,
                item.is_unsafe ? "unsafe_kernel_import" : "kernel_import",
                item.alias.empty() ? item.module
                                   : item.module + " as " + item.alias,
                item.where, 0,
                item.is_unsafe
                    ? static_cast<On1xSDK_ASTNodeFlags>(
                          ON1X_SDK_AST_FLAG_UNSAFE)
                    : On1xSDK_ASTNodeFlags{0});
            link(root, index);
        }
        for (const auto &item : unit.features) {
            auto index = add(
                ON1X_SDK_AST_DIRECTIVE, "enable", item.name, item.where);
            ast->directives.push_back({
                ON1X_SDK_DIRECTIVE_FEATURE, index, item.name, {}});
            link(root, index);
        }
        for (const auto &item : unit.scrutiny) {
            auto index = add(
                ON1X_SDK_AST_DIRECTIVE, "scrutiny", item.level, item.where);
            ast->directives.push_back({
                ON1X_SDK_DIRECTIVE_SCRUTINY, index, "scrutiny", item.level});
            link(root, index);
        }
        for (const auto &item : unit.extensions) {
            auto index = add(
                ON1X_SDK_AST_EXTENSION, "extension", item.owner, item.where);
            ast->directives.push_back({
                ON1X_SDK_DIRECTIVE_EXTENSION, index, item.owner, {}});
            for (const auto &token : item.tokens)
                link(index, add(
                    ON1X_SDK_AST_EXPRESSION, "source_token",
                    token.text, token.where));
            link(root, index);
        }
        for (const auto &item : unit.nominals) {
            auto index = add(
                ON1X_SDK_AST_DECLARATION,
                item.kind == on1x::Type::Record
                    ? "record_declaration" : "enum_declaration",
                item.name, item.where);
            for (const auto &parameter_name : item.parameters)
                link(index, add(
                    ON1X_SDK_AST_PARAMETER, "nominal_type_parameter",
                    parameter_name, item.where));
            for (std::size_t i = 0; i < item.fields.size(); ++i) {
                auto field = type(item.fields[i], item.where);
                if (i < item.labels.size() && !item.labels[i].empty()) {
                    auto labeled = add(
                        ON1X_SDK_AST_TYPE_SYNTAX, "nominal_field",
                        item.labels[i], item.where);
                    link(labeled, field);
                    field = labeled;
                }
                link(index, field);
            }
            link(root, index);
        }
        for (const auto &item : unit.cliches) {
            auto index = add(
                ON1X_SDK_AST_DECLARATION, "cliche", item.name, item.where);
            type_parameters(index, item.type_parameters);
            for (const auto &constraint_value : item.constraints)
                link(index, constraint(constraint_value));
            for (const auto &member : item.members) {
                auto child = add(
                    ON1X_SDK_AST_DECLARATION,
                    member.kind == on1x::ClicheMember::Kind::Function
                        ? "cliche_function_member" : "cliche_value_member",
                    member.name, member.where);
                type_parameters(child, member.type_parameters);
                for (const auto &constraint_value : member.constraints)
                    link(child, constraint(constraint_value));
                for (const auto &parameter_value : member.params)
                    link(child, parameter(parameter_value));
                link(child, type(member.result, member.where));
                link(index, child);
            }
            link(root, index);
        }
        for (const auto &item : unit.cliche_implementations) {
            auto index = add(
                ON1X_SDK_AST_DECLARATION, "cliche_implementation",
                item.name, item.where);
            type_parameters(index, item.type_parameters);
            for (const auto &constraint_value : item.constraints)
                link(index, constraint(constraint_value));
            for (const auto &argument : item.arguments)
                link(index, type(argument, item.where));
            for (const auto &member : item.members) {
                std::uint32_t child = 0;
                if (member.kind == on1x::ClicheMember::Kind::Function) {
                    child = function(
                        member.function,
                        "cliche_implementation_function");
                    ast->nodes[child].text = member.name;
                } else {
                    child = add(
                        ON1X_SDK_AST_DECLARATION,
                        "cliche_implementation_value",
                        member.name, member.where);
                    type_parameters(
                        child, member.function.type_parameters);
                    for (const auto &constraint_value :
                         member.function.constraints)
                        link(child, constraint(constraint_value));
                    if (member.value_type != on1x::Type::Invalid)
                        link(child, type(member.value_type, member.where));
                    if (member.value)
                        link(child, expression(*member.value));
                }
                link(index, child);
            }
            link(root, index);
        }
        for (const auto &item : unit.inherent_implementations) {
            auto index = add(
                ON1X_SDK_AST_DECLARATION, "inherent_implementation",
                on1x::type_name(item.target), item.where);
            type_parameters(index, item.type_parameters);
            link(index, type(item.target, item.where));
            for (const auto &member : item.members) {
                const auto tag =
                    member.kind == on1x::InherentMember::Kind::Function
                        ? "inherent_function" : "inherent_value";
                auto child = add(
                    ON1X_SDK_AST_DECLARATION, tag,
                    member.name, member.where);
                type_parameters(child, member.type_parameters);
                const char *receiver = nullptr;
                switch (member.receiver) {
                case on1x::InherentMember::Receiver::None:
                    break;
                case on1x::InherentMember::Receiver::Value:
                    receiver = "receiver_value"; break;
                case on1x::InherentMember::Receiver::Borrow:
                    receiver = "receiver_borrow"; break;
                case on1x::InherentMember::Receiver::Move:
                    receiver = "receiver_move"; break;
                }
                if (receiver)
                    link(child, add(
                        ON1X_SDK_AST_PARAMETER, receiver,
                        "self", member.where));
                for (const auto &parameter_value :
                     member.function.params)
                    link(child, parameter(parameter_value));
                if (member.kind ==
                    on1x::InherentMember::Kind::Function) {
                    link(child, type(
                        member.function.result, member.where));
                    for (const auto &body_item :
                         member.function.body)
                        link(child, statement(body_item));
                } else {
                    if (member.value_type != on1x::Type::Invalid)
                        link(child, type(
                            member.value_type, member.where));
                    if (member.value)
                        link(child, expression(*member.value));
                }
                link(index, child);
            }
            link(root, index);
        }
        for (const auto &item : unit.macros)
            link(root, macro(item));
        for (const auto &item : unit.ppx) {
            auto index = add(
                ON1X_SDK_AST_DIRECTIVE, "ppx", item.label, item.where);
            ast->directives.push_back({
                ON1X_SDK_DIRECTIVE_PPX, index, item.label, item.program});
            link(root, index);
        }
        for (const auto &item : unit.functions)
            link(root, function(item));
        for (const auto &evidence : unit.extension_evidence) {
            auto index = add(
                ON1X_SDK_AST_EXTENSION, "extension_evidence",
                evidence.owner, {"<extension-evidence>"}, 0,
                ON1X_SDK_AST_FLAG_GENERATED);
            for (const auto &implementation : evidence.implementations)
                link(index, add(
                    ON1X_SDK_AST_EXTENSION,
                    "extension_implementation",
                    implementation.name, implementation.where));
            link(root, index);
        }
    }

public:
    Flattener(
        std::shared_ptr<ASTData> output,
        const OptionsData &options)
        : ast(std::move(output)), operation(ast->state, options),
          maximum(options.max_ast_nodes) {}

    void run(const on1x::Unit &unit) {
        on1x::Location where{"<unit>", 0, 1, 1};
        if (!ast->sources.empty()) where.file = ast->sources.back()->path;
        auto root = add(ON1X_SDK_AST_UNIT, "unit", {}, where);
        if (root != 0)
            fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_INVALID_STATE,
                 "ON1X-SDK-AST", "AST root identity is not canonical");
        declarations(root, unit);
        operation.poll();
    }
};

std::shared_ptr<ASTData> expand_ast(
    const std::shared_ptr<ASTData> &input,
    const OptionsData &options) {
    Operation operation(input->state, options);
    operation.charge(input->nodes.size());
    std::size_t budget = options.max_ast_nodes;
    auto unit = on1x::clone_unit(*input->unit, budget);
    on1x::expand_macros(unit);
    operation.poll();
    return build_ast(
        std::move(unit), input->sources, options,
        ON1X_SDK_AST_MACRO_EXPANDED_CANDIDATE);
}

void require_schema(
    const char *format,
    std::size_t size,
    std::uint32_t version) {
    auto value = checked_string(
        format, size, 128u, "PPX schema format", true, false);
    if (value != ON1X_SDK_PPX_SCHEMA_FORMAT ||
        version != ON1X_SDK_PPX_SCHEMA_VERSION)
        fail(ON1X_SDK_ABI_MISMATCH,
             ON1X_SDK_COMPILER_SCHEMA_MISMATCH,
             "ON1X-SDK-PPX-SCHEMA",
             "unsupported PPX schema format or version");
}

} // namespace

On1xSDK_TokenKind classify_token(std::string_view text) {
    if (text == "<eof>") return ON1X_SDK_TOKEN_EOF;
    static const std::unordered_set<std::string_view> keywords = {
        "fn", "let", "var", "if", "else", "while", "return", "true",
        "false", "extern", "import", "from", "as", "forall", "macro",
        "rule", "quote", "splice", "lift", "run", "let_syntax",
        "letrec_syntax", "record", "enum", "impl", "cliche", "match",
        "for", "in", "break", "continue", "defer", "taskgroup", "unsafe"};
    if (keywords.contains(text)) return ON1X_SDK_TOKEN_KEYWORD;
    if (text.starts_with("\"") || text.starts_with("r!\"") ||
        text.starts_with("b!\"") || text.starts_with("`"))
        return ON1X_SDK_TOKEN_STRING;
    if (text.starts_with("#\\") || text.starts_with("b!#\\"))
        return ON1X_SDK_TOKEN_CHARACTER;
    if (!text.empty() && text.front() >= '0' && text.front() <= '9')
        return on1x::float_spelling(text)
            ? ON1X_SDK_TOKEN_FLOAT : ON1X_SDK_TOKEN_INTEGER;
    if (!text.empty() &&
        ((text.front() >= 'A' && text.front() <= 'Z') ||
         (text.front() >= 'a' && text.front() <= 'z') ||
         text.front() == '_'))
        return ON1X_SDK_TOKEN_IDENTIFIER;
    return ON1X_SDK_TOKEN_PUNCTUATION;
}

std::shared_ptr<TokenData> lex_impl(
    const std::shared_ptr<RevisionData> &source,
    const OptionsData &options) {
    Operation operation(source->state, options);
    if (source->bytes.size() > options.max_source_bytes)
        fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
             "ON1X-SDK-SOURCE-LIMIT",
             "source exceeds configured source byte limit");
    if ((options.strict_utf8 ||
         source->encoding == ON1X_SDK_SOURCE_UTF8) &&
        !valid_utf8(source->bytes))
        fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_VERIFICATION_FAILED,
             "ON1X-SDK-UTF8", "source is not valid shortest-form UTF-8");
    operation.charge(source->bytes.size() + 1u);
    auto scanned = on1x::scan(source->bytes, source->path);
    operation.poll();
    if (scanned.size() > options.max_tokens)
        fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
             "ON1X-SDK-TOKEN-LIMIT", "token count limit exceeded");
    operation.charge(scanned.size());
    auto result = std::make_shared<TokenData>();
    result->state = source->state;
    result->source = source;
    result->tokens = std::move(scanned);
    result->generation = object_generation(source->state);
    return result;
}

std::shared_ptr<ASTData> build_ast(
    on1x::Unit unit,
    std::vector<std::shared_ptr<RevisionData>> sources,
    const OptionsData &options,
    On1xSDK_ASTValidationState validation) {
    if (sources.empty()) invalid("AST requires at least one source revision");
    auto state = sources.front()->state;
    for (const auto &source : sources) require_same(state, source->state);
    auto result = std::make_shared<ASTData>();
    result->state = state;
    result->sources = std::move(sources);
    result->generation = object_generation(state);
    result->revision = publish_revision(state);
    result->validation = validation;
    for (const auto &source : result->sources) {
        auto [it, inserted] =
            result->source_by_path.emplace(source->path, source);
        if (!inserted &&
            (it->second->source_id != source->source_id ||
             it->second->revision != source->revision))
            fail(ON1X_SDK_ERROR,
                 ON1X_SDK_COMPILER_VERIFICATION_FAILED,
                 "ON1X-SDK-SOURCE-IDENTITY",
                 "AST contains conflicting revisions for one path");
    }
    auto retained = std::make_shared<on1x::Unit>(std::move(unit));
    result->unit = retained;
    Flattener(result, options).run(*retained);
    return result;
}

std::shared_ptr<ASTData> parse_impl(
    const std::shared_ptr<TokenData> &tokens,
    const OptionsData &options) {
    Operation operation(tokens->state, options);
    if (tokens->tokens.size() > options.max_tokens)
        fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
             "ON1X-SDK-TOKEN-LIMIT", "token count limit exceeded");
    operation.charge(tokens->tokens.size());
    auto unit = on1x::parse(tokens->tokens, options.features);
    operation.poll();
    return build_ast(
        std::move(unit), {tokens->source}, options,
        ON1X_SDK_AST_PARSED_CANDIDATE);
}

std::shared_ptr<ASTData> parse_revision_impl(
    const std::shared_ptr<RevisionData> &source,
    const OptionsData &options) {
    return parse_impl(lex_impl(source, options), options);
}

std::uint64_t node_id(const ASTData &ast, std::uint32_t index) {
    return (static_cast<std::uint64_t>(ast.generation) << 32u) |
           (static_cast<std::uint64_t>(index) + 1u);
}

std::uint32_t node_index(const ASTData &ast, On1xSDK_ASTNode node) {
    if (!node || static_cast<std::uint32_t>(node >> 32u) != ast.generation)
        stale("AST node handle belongs to another snapshot");
    const auto slot = static_cast<std::uint32_t>(node);
    if (!slot || slot > ast.nodes.size())
        not_found("AST node handle is out of range");
    return slot - 1u;
}

} // namespace on1x::sdk2

using namespace on1x::sdk2;

extern "C" {

On1xSDK_Status on1x_sdk_grammar_info(On1xSDK_GrammarInfo *out_info) {
    try {
        On1xSDK_GrammarInfo value =
            ON1X_SDK_COMPILER_INIT(On1xSDK_GrammarInfo);
        value.grammar_version = ON1X_SDK_FRONTEND_SCHEMA_VERSION;
        value.ppx_schema_version = ON1X_SDK_PPX_SCHEMA_VERSION;
        value.max_source_bytes = on1x::max_source_bytes;
        value.max_syntax_depth = on1x::max_syntax_depth;
        value.max_ppx_wire_bytes = on1x::meta::max_wire_bytes;
        value.max_ppx_wire_nodes = on1x::meta::max_wire_nodes;
#ifdef ON1X_SDK_GENERATED_SCANNER
        static constexpr char profile[] =
            "LibStone ASDL+Aurocks generated frontend";
#else
        static constexpr char profile[] =
            "portable fallback scanner (bootstrap only)";
#endif
        value.scanner_profile = profile;
        value.scanner_profile_size = sizeof(profile) - 1u;
        write_descriptor(out_info, value);
        return ON1X_SDK_OK;
    } catch (const StatusFailure &failure) {
        return failure.status;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_sdk_lex(
    const On1xSDK_SourceRevision *source,
    const On1xSDK_Options *options,
    On1xSDK_TokenStream **out_tokens) {
    if (!out_tokens) return ON1X_SDK_INVALID_ARGUMENT;
    *out_tokens = nullptr;
    if (!source || !options)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(source->data->state, [&] {
        require_same(source->data->state, options->state);
        auto tokens = std::make_unique<On1xSDK_TokenStream>();
        tokens->data = lex_impl(source->data, options->data);
        *out_tokens = tokens.release();
    });
}

On1xSDK_Status on1x_sdk_token_stream_retain(
    On1xSDK_TokenStream *tokens) {
    return retain_object(tokens);
}

void on1x_sdk_token_stream_release(On1xSDK_TokenStream *tokens) {
    release_object(tokens);
}

On1xSDK_Status on1x_sdk_token_stream_count(
    const On1xSDK_TokenStream *tokens,
    size_t *out_count) {
    if (!tokens || !out_count) return ON1X_SDK_INVALID_ARGUMENT;
    return api(tokens->data->state, [&] {
        *out_count = tokens->data->tokens.size();
    }, true);
}

On1xSDK_Status on1x_sdk_token_stream_get(
    const On1xSDK_TokenStream *tokens,
    size_t index,
    On1xSDK_TokenInfo *out_token) {
    if (!tokens) return ON1X_SDK_INVALID_ARGUMENT;
    return api(tokens->data->state, [&] {
        if (index >= tokens->data->tokens.size())
            not_found("token index is out of range");
        const auto &token = tokens->data->tokens[index];
        On1xSDK_TokenInfo value =
            ON1X_SDK_COMPILER_INIT(On1xSDK_TokenInfo);
        value.token_id =
            (static_cast<std::uint64_t>(tokens->data->generation) << 32u) |
            (index + 1u);
        value.kind = classify_token(token.text);
        value.text = token.text.data();
        value.text_size = token.text.size();
        value.source_id = tokens->data->source->source_id;
        value.source_revision = tokens->data->source->revision;
        value.byte_begin = token.where.offset;
        value.byte_end = token.where.offset +
                         (token.text == "<eof>" ? 0u : token.text.size());
        value.line = token.where.line;
        value.column = token.where.column;
        write_descriptor(out_token, value);
    }, true);
}

On1xSDK_Status on1x_sdk_token_stream_source(
    const On1xSDK_TokenStream *tokens,
    On1xSDK_SourceRevision **out_revision) {
    if (!out_revision) return ON1X_SDK_INVALID_ARGUMENT;
    *out_revision = nullptr;
    if (!tokens) return ON1X_SDK_INVALID_ARGUMENT;
    return api(tokens->data->state, [&] {
        auto revision = std::make_unique<On1xSDK_SourceRevision>();
        revision->data = tokens->data->source;
        *out_revision = revision.release();
    }, true);
}

On1xSDK_Status on1x_sdk_parse(
    const On1xSDK_TokenStream *tokens,
    const On1xSDK_Options *options,
    On1xSDK_AST **out_ast) {
    if (!out_ast) return ON1X_SDK_INVALID_ARGUMENT;
    *out_ast = nullptr;
    if (!tokens || !options)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(tokens->data->state, [&] {
        require_same(tokens->data->state, options->state);
        auto result = parse_impl(tokens->data, options->data);
        if (options->data.expand_macros &&
            on1x::needs_macro_expansion(*result->unit))
            result = expand_ast(result, options->data);
        auto ast = std::make_unique<On1xSDK_AST>();
        ast->data = std::move(result);
        *out_ast = ast.release();
    });
}

On1xSDK_Status on1x_sdk_frontend_parse(
    const On1xSDK_SourceRevision *source,
    const On1xSDK_Options *options,
    On1xSDK_AST **out_ast) {
    if (!out_ast) return ON1X_SDK_INVALID_ARGUMENT;
    *out_ast = nullptr;
    if (!source || !options)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(source->data->state, [&] {
        require_same(source->data->state, options->state);
        auto result = parse_revision_impl(source->data, options->data);
        if (options->data.expand_macros &&
            on1x::needs_macro_expansion(*result->unit))
            result = expand_ast(result, options->data);
        auto ast = std::make_unique<On1xSDK_AST>();
        ast->data = std::move(result);
        *out_ast = ast.release();
    });
}

On1xSDK_Status on1x_sdk_ast_retain(On1xSDK_AST *ast) {
    return retain_object(ast);
}

void on1x_sdk_ast_release(On1xSDK_AST *ast) {
    release_object(ast);
}

On1xSDK_Status on1x_sdk_ast_info(
    const On1xSDK_AST *ast,
    On1xSDK_ASTInfo *out_info) {
    if (!ast) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        On1xSDK_ASTInfo value =
            ON1X_SDK_COMPILER_INIT(On1xSDK_ASTInfo);
        value.schema_version = ON1X_SDK_FRONTEND_SCHEMA_VERSION;
        value.validation_state = ast->data->validation;
        value.revision = ast->data->revision;
        value.root = node_id(*ast->data, 0);
        value.node_count = ast->data->nodes.size();
        value.source_count = ast->data->sources.size();
        write_descriptor(out_info, value);
    }, true);
}

On1xSDK_Status on1x_sdk_ast_node(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode node,
    On1xSDK_ASTNodeInfo *out_info) {
    if (!ast) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        const auto index = node_index(*ast->data, node);
        const auto &item = ast->data->nodes[index];
        On1xSDK_ASTNodeInfo value =
            ON1X_SDK_COMPILER_INIT(On1xSDK_ASTNodeInfo);
        value.node = node;
        value.category = item.category;
        value.flags = item.flags;
        value.tag = item.tag.data();
        value.tag_size = item.tag.size();
        value.text = item.text.data();
        value.text_size = item.text.size();
        value.hygiene_context = item.hygiene_context;
        value.source_id = item.source_id;
        value.source_revision = item.source_revision;
        value.path = item.location.file.data();
        value.path_size = item.location.file.size();
        value.byte_begin = item.location.offset;
        value.byte_end = item.byte_end;
        value.line = item.location.line;
        value.column = item.location.column;
        value.child_count = item.children.size();
        write_descriptor(out_info, value);
    }, true);
}

On1xSDK_Status on1x_sdk_ast_child(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode node,
    size_t child_index,
    On1xSDK_ASTNode *out_child) {
    if (!ast || !out_child) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        const auto index = node_index(*ast->data, node);
        const auto &children = ast->data->nodes[index].children;
        if (child_index >= children.size())
            not_found("AST child index is out of range");
        *out_child = node_id(*ast->data, children[child_index]);
    }, true);
}

On1xSDK_Status on1x_sdk_ast_source(
    const On1xSDK_AST *ast,
    size_t source_index,
    On1xSDK_SourceRevision **out_revision) {
    if (!out_revision) return ON1X_SDK_INVALID_ARGUMENT;
    *out_revision = nullptr;
    if (!ast) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        if (source_index >= ast->data->sources.size())
            not_found("AST source index is out of range");
        auto revision = std::make_unique<On1xSDK_SourceRevision>();
        revision->data = ast->data->sources[source_index];
        *out_revision = revision.release();
    }, true);
}

On1xSDK_Status on1x_sdk_ast_visit(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode root,
    On1xSDK_ASTVisitOrder order,
    On1xSDK_ASTVisitorFn visitor,
    void *userdata) {
    if (!ast || !visitor ||
        (order != ON1X_SDK_AST_PREORDER &&
         order != ON1X_SDK_AST_POSTORDER))
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        struct Frame {
            std::uint32_t index;
            std::size_t depth;
            bool exit;
        };
        std::vector<Frame> pending{{
            node_index(*ast->data, root), 0u, false}};
        std::size_t visited = 0;
        while (!pending.empty()) {
            auto frame = pending.back();
            pending.pop_back();
            if (++visited > ast->data->nodes.size() * 2u + 1u)
                fail(ON1X_SDK_ERROR,
                     ON1X_SDK_COMPILER_VERIFICATION_FAILED,
                     "ON1X-SDK-AST-CYCLE",
                     "AST traversal encountered a cycle");
            if ((order == ON1X_SDK_AST_PREORDER && !frame.exit) ||
                (order == ON1X_SDK_AST_POSTORDER && frame.exit)) {
                std::uint32_t raw = ON1X_SDK_ERROR;
                try {
                    raw = static_cast<std::uint32_t>(
                        visitor(
                            userdata, ast,
                            node_id(*ast->data, frame.index),
                            frame.depth));
                } catch (...) {
                    fail(ON1X_SDK_ERROR,
                         ON1X_SDK_COMPILER_INVALID_STATE,
                         "ON1X-SDK-CALLBACK",
                         "AST visitor unwound through the C boundary");
                }
                if (raw > ON1X_SDK_BUSY)
                    fail(ON1X_SDK_ERROR,
                         ON1X_SDK_COMPILER_INVALID_STATE,
                         "ON1X-SDK-CALLBACK-STATUS",
                         "AST visitor returned an unknown status");
                if (raw != ON1X_SDK_OK)
                    fail(static_cast<On1xSDK_Status>(raw),
                         ON1X_SDK_COMPILER_CONDITION_NONE,
                         "ON1X-SDK-VISITOR-STOP",
                         "AST visitor stopped traversal");
            }
            if (frame.exit) continue;
            if (order == ON1X_SDK_AST_POSTORDER)
                pending.push_back({frame.index, frame.depth, true});
            const auto &children = ast->data->nodes[frame.index].children;
            for (auto child = children.rbegin(); child != children.rend(); ++child)
                pending.push_back({*child, frame.depth + 1u, false});
        }
    }, true);
}

On1xSDK_Status on1x_sdk_syntax_is_category(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode node,
    On1xSDK_ASTNodeCategory category,
    uint32_t *out_matches) {
    if (!ast || !out_matches) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        const auto index = node_index(*ast->data, node);
        *out_matches =
            ast->data->nodes[index].category == category ? 1u : 0u;
    }, true);
}

On1xSDK_Status on1x_sdk_syntax_count_tag(
    const On1xSDK_AST *ast,
    const char *tag,
    size_t tag_size,
    size_t *out_count) {
    if (!ast || !out_count) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        auto value = checked_string(
            tag, tag_size, 128u, "syntax tag", true, false);
        *out_count = static_cast<std::size_t>(std::count_if(
            ast->data->nodes.begin(), ast->data->nodes.end(),
            [&](const auto &node) { return node.tag == value; }));
    }, true);
}

On1xSDK_Status on1x_sdk_ast_attribute_count(
    const On1xSDK_AST *ast,
    size_t *out_count) {
    if (!ast || !out_count) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        *out_count = ast->data->attributes.size();
    }, true);
}

On1xSDK_Status on1x_sdk_ast_attribute(
    const On1xSDK_AST *ast,
    size_t index,
    On1xSDK_ASTNode *out_node) {
    if (!ast || !out_node) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        if (index >= ast->data->attributes.size())
            not_found("attribute index is out of range");
        *out_node = node_id(*ast->data, ast->data->attributes[index]);
    }, true);
}

On1xSDK_Status on1x_sdk_ast_directive_count(
    const On1xSDK_AST *ast,
    size_t *out_count) {
    if (!ast || !out_count) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        *out_count = ast->data->directives.size();
    }, true);
}

On1xSDK_Status on1x_sdk_ast_directive(
    const On1xSDK_AST *ast,
    size_t index,
    On1xSDK_DirectiveInfo *out_directive) {
    if (!ast) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        if (index >= ast->data->directives.size())
            not_found("directive index is out of range");
        const auto &item = ast->data->directives[index];
        On1xSDK_DirectiveInfo value =
            ON1X_SDK_COMPILER_INIT(On1xSDK_DirectiveInfo);
        value.kind = item.kind;
        value.node = node_id(*ast->data, item.node);
        value.name = item.name.data();
        value.name_size = item.name.size();
        value.value = item.value.data();
        value.value_size = item.value.size();
        write_descriptor(out_directive, value);
    }, true);
}

On1xSDK_Status on1x_sdk_ast_macro_count(
    const On1xSDK_AST *ast,
    size_t *out_count) {
    if (!ast || !out_count) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        *out_count = ast->data->macros.size();
    }, true);
}

On1xSDK_Status on1x_sdk_macro_expand(
    const On1xSDK_AST *ast,
    const On1xSDK_Options *options,
    On1xSDK_AST **out_ast) {
    if (!out_ast) return ON1X_SDK_INVALID_ARGUMENT;
    *out_ast = nullptr;
    if (!ast || !options)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        require_same(ast->data->state, options->state);
        auto expanded = std::make_unique<On1xSDK_AST>();
        expanded->data = expand_ast(ast->data, options->data);
        *out_ast = expanded.release();
    });
}

On1xSDK_Status on1x_sdk_hygiene_context(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode node,
    uint64_t *out_context) {
    if (!ast || !out_context) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        *out_context =
            ast->data->nodes[node_index(*ast->data, node)].hygiene_context;
    }, true);
}

On1xSDK_Status on1x_sdk_hygiene_same_context(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode left,
    On1xSDK_ASTNode right,
    uint32_t *out_same) {
    if (!ast || !out_same) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        const auto left_index = node_index(*ast->data, left);
        const auto right_index = node_index(*ast->data, right);
        *out_same =
            ast->data->nodes[left_index].hygiene_context ==
                    ast->data->nodes[right_index].hygiene_context
                ? 1u : 0u;
    }, true);
}

On1xSDK_Status on1x_sdk_ppx_limits(On1xSDK_PPXLimits *out_limits) {
    try {
        On1xSDK_PPXLimits value =
            ON1X_SDK_COMPILER_INIT(On1xSDK_PPXLimits);
        value.max_wire_bytes = on1x::meta::max_wire_bytes;
        value.max_wire_nodes = on1x::meta::max_wire_nodes;
        value.max_depth = on1x::max_syntax_depth;
        write_descriptor(out_limits, value);
        return ON1X_SDK_OK;
    } catch (const StatusFailure &failure) {
        return failure.status;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_sdk_ppx_validate_schema(
    const char *format,
    size_t format_size,
    uint32_t version) {
    try {
        require_schema(format, format_size, version);
        return ON1X_SDK_OK;
    } catch (const StatusFailure &failure) {
        return failure.status;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_sdk_ppx_encode(
    const On1xSDK_AST *ast,
    char *buffer,
    size_t capacity,
    size_t *out_size) {
    if (out_size) *out_size = 0;
    if (!ast || !out_size || (!buffer && capacity))
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        auto tree = on1x::meta::encode(*ast->data->unit);
        on1x::meta::base_schema().validate(tree);
        auto wire = on1x::meta::write_sexpr(tree);
        *out_size = wire.size();
        if (!buffer && !capacity) return;
        if (capacity < wire.size())
            fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                 "ON1X-SDK-BUFFER-LIMIT",
                 "caller PPX output buffer is too small");
        std::memcpy(buffer, wire.data(), wire.size());
    }, true);
}

On1xSDK_Status on1x_sdk_ppx_readmit(
    const On1xSDK_AST *base,
    const char *format,
    size_t format_size,
    uint32_t version,
    const void *wire,
    size_t wire_size,
    const On1xSDK_Options *options,
    On1xSDK_AST **out_candidate) {
    if (!out_candidate) return ON1X_SDK_INVALID_ARGUMENT;
    *out_candidate = nullptr;
    if (!base || !options)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(base->data->state, [&] {
        require_same(base->data->state, options->state);
        require_schema(format, format_size, version);
        auto bytes = checked_bytes(
            wire, wire_size, on1x::meta::max_wire_bytes, "PPX wire");
        Operation operation(base->data->state, options->data);
        operation.charge(bytes.size() + 1u);
        auto tree = on1x::meta::read_sexpr(bytes, "<sdk-ppx-candidate>");
        on1x::meta::base_schema().validate(tree);
        auto unit = on1x::meta::decode(tree);
        operation.poll();
        auto candidate = std::make_unique<On1xSDK_AST>();
        candidate->data = build_ast(
            std::move(unit), base->data->sources, options->data,
            ON1X_SDK_AST_PPX_READMITTED_CANDIDATE);
        *out_candidate = candidate.release();
    });
}

On1xSDK_Status on1x_sdk_staging_inspect(
    const On1xSDK_AST *ast,
    On1xSDK_StagingInfo *out_info) {
    if (!ast) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        On1xSDK_StagingInfo value =
            ON1X_SDK_COMPILER_INIT(On1xSDK_StagingInfo);
        for (const auto &node : ast->data->nodes) {
            if (node.tag == "quote") ++value.quote_count;
            else if (node.tag == "splice") ++value.splice_count;
            else if (node.tag == "lift") ++value.lift_count;
            else if (node.tag == "run") ++value.run_count;
            else if (node.tag == "call") {
                const auto source_run = std::any_of(
                    node.children.begin(), node.children.end(),
                    [&](std::uint32_t child) {
                        const auto &role = ast->data->nodes[child];
                        if (role.tag != "callee" ||
                            role.children.size() != 1u)
                            return false;
                        const auto &callee =
                            ast->data->nodes[role.children.front()];
                        return callee.tag == "identifier" &&
                               callee.text == "run";
                    });
                if (source_run) ++value.run_count;
            }
        }
        struct Frame { std::uint32_t node; std::size_t depth; };
        std::vector<Frame> pending{{0, 1}};
        while (!pending.empty()) {
            auto frame = pending.back();
            pending.pop_back();
            value.maximum_syntax_depth =
                std::max(value.maximum_syntax_depth, frame.depth);
            for (auto child : ast->data->nodes[frame.node].children)
                pending.push_back({child, frame.depth + 1u});
        }
        value.requires_semantic_lowering =
            value.quote_count || value.splice_count ||
            value.lift_count || value.run_count;
        write_descriptor(out_info, value);
    }, true);
}

On1xSDK_Status on1x_sdk_staging_lower(
    const On1xSDK_AST *ast,
    On1xSDK_AST **out_ast) {
    if (!out_ast) return ON1X_SDK_INVALID_ARGUMENT;
    *out_ast = nullptr;
    if (!ast) return ON1X_SDK_INVALID_ARGUMENT;
    return api(ast->data->state, [&] {
        fail(ON1X_SDK_UNSUPPORTED,
             ON1X_SDK_COMPILER_CAPABILITY_DENIED,
             "ON1X-SDK-STAGING-SEMANTICS",
             "staging lowering requires authoritative semantic inference");
    }, true);
}

On1xSDK_Status on1x_sdk_elaborate(
    const On1xSDK_AST *candidate,
    void **out_checked_hir) {
    if (!out_checked_hir) return ON1X_SDK_INVALID_ARGUMENT;
    *out_checked_hir = nullptr;
    if (!candidate) return ON1X_SDK_INVALID_ARGUMENT;
    return api(candidate->data->state, [&] {
        fail(ON1X_SDK_UNSUPPORTED,
             ON1X_SDK_COMPILER_CAPABILITY_DENIED,
             "ON1X-SDK-ELABORATOR",
             "checked-HIR publication belongs to the semantic SDK");
    }, true);
}

On1xSDK_Status on1x_sdk_ast_builder_create(
    On1xSDK_Source *source,
    uint64_t expected_revision,
    const On1xSDK_Options *options,
    On1xSDK_ASTBuilder **out_builder) {
    if (!out_builder) return ON1X_SDK_INVALID_ARGUMENT;
    *out_builder = nullptr;
    if (!source || !options)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(source->data->state, [&] {
        require_same(source->data->state, options->state);
        if (source->data->current->revision != expected_revision)
            stale("source changed before builder creation");
        auto builder = std::make_unique<On1xSDK_ASTBuilder>();
        builder->state = source->data->state;
        builder->source = source->data;
        builder->options = options->data;
        builder->expected_revision = expected_revision;
        *out_builder = builder.release();
    });
}

On1xSDK_Status on1x_sdk_ast_builder_set_source(
    On1xSDK_ASTBuilder *builder,
    const void *bytes,
    size_t byte_size,
    On1xSDK_SourceEncoding encoding) {
    if (!builder) return ON1X_SDK_INVALID_ARGUMENT;
    return api(builder->state, [&] {
        if (byte_size > builder->options.max_source_bytes)
            fail(ON1X_SDK_LIMIT, ON1X_SDK_COMPILER_CONDITION_NONE,
                 "ON1X-SDK-SOURCE-LIMIT",
                 "candidate exceeds configured source byte limit");
        auto text = checked_bytes(
            bytes, byte_size, builder->options.max_source_bytes,
            "candidate source");
        if (encoding != ON1X_SDK_SOURCE_BYTES &&
            encoding != ON1X_SDK_SOURCE_UTF8)
            invalid("unknown source encoding policy");
        if ((encoding == ON1X_SDK_SOURCE_UTF8 ||
             builder->options.strict_utf8) &&
            !valid_utf8(text))
            fail(ON1X_SDK_ERROR,
                 ON1X_SDK_COMPILER_VERIFICATION_FAILED,
                 "ON1X-SDK-UTF8",
                 "candidate is not valid shortest-form UTF-8");
        builder->bytes = std::move(text);
        builder->encoding = encoding;
        builder->has_candidate = true;
    });
}

On1xSDK_Status on1x_sdk_ast_builder_validate(
    On1xSDK_ASTBuilder *builder,
    On1xSDK_AST **out_candidate) {
    if (!out_candidate) return ON1X_SDK_INVALID_ARGUMENT;
    *out_candidate = nullptr;
    if (!builder) return ON1X_SDK_INVALID_ARGUMENT;
    return api(builder->state, [&] {
        if (!builder->has_candidate)
            fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_INVALID_STATE,
                 "ON1X-SDK-BUILDER", "builder has no source candidate");
        if (builder->source->current->revision != builder->expected_revision)
            stale("source changed before candidate validation");
        auto candidate = std::make_unique<On1xSDK_AST>();
        auto revision = make_revision(
            builder->state, builder->source->source_id,
            builder->source->current->path, builder->bytes,
            builder->encoding);
        revision->identity = builder->source->identity;
        auto result = parse_revision_impl(revision, builder->options);
        candidate->data = std::move(result);
        *out_candidate = candidate.release();
    });
}

On1xSDK_Status on1x_sdk_ast_builder_commit(
    On1xSDK_ASTBuilder *builder,
    On1xSDK_SourceRevision **out_revision,
    On1xSDK_AST **out_ast) {
    if (out_revision) *out_revision = nullptr;
    if (out_ast) *out_ast = nullptr;
    if (!builder || !out_revision || !out_ast)
        return ON1X_SDK_INVALID_ARGUMENT;
    return api(builder->state, [&] {
        if (!builder->has_candidate)
            fail(ON1X_SDK_ERROR, ON1X_SDK_COMPILER_INVALID_STATE,
                 "ON1X-SDK-BUILDER", "builder has no source candidate");
        if (builder->source->current->revision != builder->expected_revision)
            stale("source changed before candidate commit");
        auto revision_handle =
            std::make_unique<On1xSDK_SourceRevision>();
        auto ast_handle = std::make_unique<On1xSDK_AST>();
        auto revision = make_revision(
            builder->state, builder->source->source_id,
            builder->source->current->path, builder->bytes,
            builder->encoding);
        revision->identity = builder->source->identity;
        auto ast_data = parse_revision_impl(revision, builder->options);
        if (builder->options.expand_macros &&
            on1x::needs_macro_expansion(*ast_data->unit))
            ast_data = expand_ast(ast_data, builder->options);
        if (builder->source->current->revision != builder->expected_revision)
            stale("source changed while candidate was validated");
        revision_handle->data = revision;
        ast_handle->data = std::move(ast_data);
        set_current_revision(builder->source, revision);
        builder->expected_revision = revision->revision;
        builder->has_candidate = false;
        *out_revision = revision_handle.release();
        *out_ast = ast_handle.release();
    });
}

void on1x_sdk_ast_builder_destroy(On1xSDK_ASTBuilder *builder) {
    delete builder;
}

} // extern "C"
