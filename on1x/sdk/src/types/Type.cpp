#include "../ir/IRInternal.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace on1x::sdk::ir {

static_assert(static_cast<unsigned>(Type::Invalid) == ON1X_SDK_TYPE_INVALID);
static_assert(static_cast<unsigned>(Type::Own) == ON1X_SDK_TYPE_OWN);

On1xSDK_Type *make_type(const Identity &identity, Type type) {
    auto result = std::make_unique<On1xSDK_Type>();
    result->identity = identity;
    result->type = std::move(type);
    return result.release();
}

namespace {

constexpr std::size_t max_direct_children = 16384;
constexpr std::size_t max_type_name = 4096;

bool valid_kind(On1xSDK_TypeKind kind) noexcept {
    return static_cast<std::uint32_t>(kind) <= ON1X_SDK_TYPE_OWN;
}

Type::Kind canonical_kind(On1xSDK_TypeKind kind) {
    if (!valid_kind(kind)) throw std::invalid_argument("unknown type kind");
    return static_cast<Type::Kind>(kind);
}

bool composite(Type::Kind kind) noexcept {
    return kind == Type::Function || kind == Type::Tuple ||
           kind == Type::Forall || kind == Type::Code ||
           kind == Type::Named || kind == Type::Own ||
           kind == Type::Record || kind == Type::Sum ||
           kind == Type::Array || kind == Type::Slice;
}

On1xSDK_Type *make_candidate_type(const Identity &identity, Type type) {
    struct Item {
        const Type *type;
        std::size_t depth;
    };
    std::vector<Item> pending{{&type, 0}};
    std::size_t nodes = 0;
    auto add = [&](const Type &child, std::size_t depth) {
        if (pending.size() >= ON1X_SDK_IR_MAX_NODES - nodes)
            throw std::length_error("candidate type node limit exceeded");
        pending.push_back({&child, depth});
    };
    while (!pending.empty()) {
        const auto item = pending.back();
        pending.pop_back();
        if (item.depth > ON1X_SDK_IR_MAX_DEPTH)
            throw std::length_error("candidate type depth limit exceeded");
        if (nodes++ >= ON1X_SDK_IR_MAX_NODES)
            throw std::length_error("candidate type node limit exceeded");
        if (item.type->kind() == Type::Invalid)
            throw std::invalid_argument("candidate contains invalid type");
        for (const auto &argument : item.type->arguments())
            add(argument, item.depth + 1);
        if (item.type->is_function()) {
            for (const auto &parameter : item.type->parameters())
                add(parameter, item.depth + 1);
            add(item.type->result(), item.depth + 1);
        } else if (item.type->is_tuple() || item.type->is_data()) {
            for (const auto &element : item.type->elements())
                add(element, item.depth + 1);
        } else if (item.type->is_forall()) {
            add(item.type->body(), item.depth + 1);
        } else if (item.type->is_code()) {
            add(item.type->code_result(), item.depth + 1);
        } else if (item.type->is_own()) {
            add(item.type->owned(), item.depth + 1);
        }
    }
    return make_type(identity, std::move(type));
}

On1xSDK_Status children(const Identity &identity,
                        const On1xSDK_Type *const *values,
                        std::size_t count,
                        std::vector<Type> &out) noexcept {
    if (count > max_direct_children) return ON1X_SDK_LIMIT;
    if (count && !values) return ON1X_SDK_INVALID_ARGUMENT;
    return guard([&] {
        out.clear();
        out.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            if (!values[index]) return ON1X_SDK_INVALID_ARGUMENT;
            if (!same_identity(identity, values[index]->identity))
                return ON1X_SDK_BUSY;
            out.push_back(values[index]->type);
        }
        return ON1X_SDK_OK;
    });
}

On1xSDK_Status candidate(const On1xSDK_IRIdentity *source,
                         Identity &identity) noexcept {
    return read_identity(source, ON1X_SDK_IR_CANDIDATE, identity);
}

template<class F>
On1xSDK_Status create(On1xSDK_Type **out, F &&factory) noexcept {
    if (!out) return ON1X_SDK_INVALID_ARGUMENT;
    *out = nullptr;
    return guard([&] {
        *out = factory();
        return ON1X_SDK_OK;
    });
}

On1xSDK_TypeFlags flags(const Type &type) noexcept {
    On1xSDK_TypeFlags result = 0;
    if (type.has_holes()) result |= ON1X_SDK_TYPE_FLAG_HAS_HOLES;
    if (type.has_generics()) result |= ON1X_SDK_TYPE_FLAG_HAS_GENERICS;
    if (type.has_foralls()) result |= ON1X_SDK_TYPE_FLAG_HAS_FORALLS;
    if (type.has_functions())
        result |= ON1X_SDK_TYPE_FLAG_HAS_FUNCTION_STORAGE;
    if (type.is_move_only()) result |= ON1X_SDK_TYPE_FLAG_MOVE_ONLY;
    if (type.contains_unique()) result |= ON1X_SDK_TYPE_FLAG_CONTAINS_UNIQUE;
    if (type.is_numeric()) result |= ON1X_SDK_TYPE_FLAG_NUMERIC;
    if (type.is_unsigned()) result |= ON1X_SDK_TYPE_FLAG_UNSIGNED;
    if (type.is_float()) result |= ON1X_SDK_TYPE_FLAG_FLOAT;
    if (type.is_data()) result |= ON1X_SDK_TYPE_FLAG_DATA;
    if (type.is_csp()) result |= ON1X_SDK_TYPE_FLAG_CSP;
    return result;
}

struct Prefix {
    std::uint32_t abi_version;
    std::uint32_t struct_size;
};

On1xSDK_Status form_options(const On1xSDK_TypeFormOptions *source,
                            TypeFormOptions &out) noexcept {
    out = {};
    if (!source) return ON1X_SDK_OK;
    Prefix prefix{};
    std::memcpy(&prefix, source, sizeof prefix);
    if (prefix.abi_version != ON1X_SDK_SEMANTICS_IR_ABI_VERSION ||
        prefix.struct_size < sizeof(Prefix))
        return ON1X_SDK_ABI_MISMATCH;
    On1xSDK_TypeFormOptions copy{};
    std::memcpy(&copy, source,
                std::min<std::size_t>(prefix.struct_size, sizeof copy));
    const auto has_bound_variables =
        prefix.struct_size >= offsetof(On1xSDK_TypeFormOptions, bound_variables) +
                              sizeof copy.bound_variables;
    const auto has_max_depth =
        prefix.struct_size >= offsetof(On1xSDK_TypeFormOptions, max_depth) +
                              sizeof copy.max_depth;
    const auto has_max_nodes =
        prefix.struct_size >= offsetof(On1xSDK_TypeFormOptions, max_nodes) +
                              sizeof copy.max_nodes;
    const auto has_holes =
        prefix.struct_size >= offsetof(On1xSDK_TypeFormOptions, allow_holes) +
                              sizeof copy.allow_holes;
    const auto has_generics =
        prefix.struct_size >= offsetof(On1xSDK_TypeFormOptions, allow_generics) +
                              sizeof copy.allow_generics;
    const auto has_named =
        prefix.struct_size >= offsetof(On1xSDK_TypeFormOptions, allow_named) +
                              sizeof copy.allow_named;
    const auto has_foralls =
        prefix.struct_size >= offsetof(On1xSDK_TypeFormOptions, allow_foralls) +
                              sizeof copy.allow_foralls;
    if (has_bound_variables)
        out.bound_variables = copy.bound_variables;
    if (has_max_depth) out.max_depth = copy.max_depth;
    if (has_max_nodes) out.max_nodes = copy.max_nodes;
    if (has_holes) out.allow_holes = copy.allow_holes != 0;
    if (has_generics)
        out.allow_generics = copy.allow_generics != 0;
    if (has_named) out.allow_named = copy.allow_named != 0;
    if (has_foralls)
        out.allow_foralls = copy.allow_foralls != 0;
    if ((copy.allow_holes > 1 && has_holes) ||
        (copy.allow_generics > 1 && has_generics) ||
        (copy.allow_named > 1 && has_named) ||
        (copy.allow_foralls > 1 && has_foralls) ||
        !out.max_depth || !out.max_nodes ||
        out.max_depth > ON1X_SDK_IR_MAX_DEPTH ||
        out.max_nodes > ON1X_SDK_IR_MAX_NODES)
        return ON1X_SDK_INVALID_ARGUMENT;
    return ON1X_SDK_OK;
}

} // namespace
} // namespace on1x::sdk::ir

using on1x::Type;
using on1x::TypeFormOptions;
using on1x::TypeFormStatus;
using on1x::check_type_form;

extern "C" {

On1xSDK_Status on1x_sdk_type_create_atomic(
    const On1xSDK_IRIdentity *identity_record,
    On1xSDK_TypeKind kind,
    uint64_t variable,
    On1xSDK_Type **out_type) {
    using namespace on1x::sdk::ir;
    Identity identity;
    if (const auto status = candidate(identity_record, identity);
        status != ON1X_SDK_OK) {
        if (out_type) *out_type = nullptr;
        return status;
    }
    if (variable > std::numeric_limits<std::size_t>::max()) {
        if (out_type) *out_type = nullptr;
        return ON1X_SDK_LIMIT;
    }
    return create(out_type, [&] {
        const auto canonical = canonical_kind(kind);
        if (canonical == Type::Invalid || composite(canonical))
            throw std::invalid_argument("atomic factory received composite kind");
        Type type;
        if (canonical == Type::Generic) {
            type = Type::generic(variable);
        } else if (canonical == Type::Bound) {
            type = Type::bound(variable);
        } else {
            if (variable) throw std::invalid_argument("unexpected type variable");
            type = Type(canonical);
        }
        return make_candidate_type(identity, std::move(type));
    });
}

On1xSDK_Status on1x_sdk_type_create_function(
    const On1xSDK_IRIdentity *identity_record,
    const On1xSDK_Type *const *parameters,
    size_t parameter_count,
    const On1xSDK_Type *result,
    On1xSDK_Type **out_type) {
    using namespace on1x::sdk::ir;
    Identity identity;
    if (const auto status = candidate(identity_record, identity);
        status != ON1X_SDK_OK) {
        if (out_type) *out_type = nullptr;
        return status;
    }
    if (!result) {
        if (out_type) *out_type = nullptr;
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!same_identity(identity, result->identity)) {
        if (out_type) *out_type = nullptr;
        return ON1X_SDK_BUSY;
    }
    std::vector<Type> copied;
    if (const auto status = children(identity, parameters, parameter_count, copied);
        status != ON1X_SDK_OK) {
        if (out_type) *out_type = nullptr;
        return status;
    }
    return create(out_type, [&] {
        return make_candidate_type(
            identity, Type::function(std::move(copied), result->type));
    });
}

On1xSDK_Status on1x_sdk_type_create_tuple(
    const On1xSDK_IRIdentity *identity_record,
    const On1xSDK_Type *const *elements,
    size_t element_count,
    On1xSDK_Type **out_type) {
    using namespace on1x::sdk::ir;
    Identity identity;
    if (const auto status = candidate(identity_record, identity);
        status != ON1X_SDK_OK) {
        if (out_type) *out_type = nullptr;
        return status;
    }
    if (element_count < 2) {
        if (out_type) *out_type = nullptr;
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    std::vector<Type> copied;
    if (const auto status = children(identity, elements, element_count, copied);
        status != ON1X_SDK_OK) {
        if (out_type) *out_type = nullptr;
        return status;
    }
    return create(out_type, [&] {
        return make_candidate_type(identity, Type::tuple(std::move(copied)));
    });
}

On1xSDK_Status on1x_sdk_type_create_data(
    const On1xSDK_IRIdentity *identity_record,
    On1xSDK_TypeKind kind,
    const On1xSDK_Type *const *elements,
    const On1xSDK_IRStringView *labels,
    size_t element_count,
    size_t extent,
    On1xSDK_IRStringView nominal_name,
    const On1xSDK_Type *const *nominal_arguments,
    size_t nominal_argument_count,
    On1xSDK_Type **out_type) {
    using namespace on1x::sdk::ir;
    if (out_type) *out_type = nullptr;
    Identity identity;
    if (const auto status = candidate(identity_record, identity);
        status != ON1X_SDK_OK) return status;
    if (kind != ON1X_SDK_TYPE_RECORD && kind != ON1X_SDK_TYPE_SUM &&
        kind != ON1X_SDK_TYPE_ARRAY && kind != ON1X_SDK_TYPE_SLICE)
        return ON1X_SDK_INVALID_ARGUMENT;

    std::vector<Type> copied, arguments;
    if (const auto status = children(identity, elements, element_count, copied);
        status != ON1X_SDK_OK) return status;
    if (const auto status = children(identity, nominal_arguments,
                                     nominal_argument_count, arguments);
        status != ON1X_SDK_OK) return status;
    std::vector<std::string> copied_labels;
    if ((kind == ON1X_SDK_TYPE_RECORD || kind == ON1X_SDK_TYPE_SUM) &&
        element_count && !labels)
        return ON1X_SDK_INVALID_ARGUMENT;
    if ((kind == ON1X_SDK_TYPE_ARRAY || kind == ON1X_SDK_TYPE_SLICE) && labels)
        return ON1X_SDK_INVALID_ARGUMENT;
    if (const auto status = guard([&] {
            copied_labels.reserve(labels ? element_count : 0);
            for (std::size_t index = 0;
                 labels && index < element_count; ++index) {
                std::string label;
                if (const auto copy_status = copy_text(
                        labels[index], max_type_name, false, label);
                    copy_status != ON1X_SDK_OK)
                    return copy_status;
                copied_labels.push_back(std::move(label));
            }
            return ON1X_SDK_OK;
        });
        status != ON1X_SDK_OK)
        return status;
    std::string nominal;
    if (const auto status = copy_text(nominal_name, max_type_name, true, nominal);
        status != ON1X_SDK_OK) return status;
    if ((!nominal.empty() &&
         kind != ON1X_SDK_TYPE_RECORD && kind != ON1X_SDK_TYPE_SUM) ||
        (nominal.empty() && nominal_argument_count))
        return ON1X_SDK_INVALID_ARGUMENT;

    return create(out_type, [&] {
        auto type = Type::data(canonical_kind(kind), std::move(copied),
                               std::move(copied_labels), extent);
        if (!nominal.empty())
            type = type.with_nominal(std::move(nominal), std::move(arguments));
        return make_candidate_type(identity, std::move(type));
    });
}

On1xSDK_Status on1x_sdk_type_create_named(
    const On1xSDK_IRIdentity *identity_record,
    On1xSDK_IRStringView name,
    const On1xSDK_Type *const *arguments,
    size_t argument_count,
    On1xSDK_Type **out_type) {
    using namespace on1x::sdk::ir;
    if (out_type) *out_type = nullptr;
    Identity identity;
    if (const auto status = candidate(identity_record, identity);
        status != ON1X_SDK_OK) return status;
    std::string copied_name;
    if (const auto status = copy_text(name, max_type_name, false, copied_name);
        status != ON1X_SDK_OK) return status;
    std::vector<Type> copied;
    if (const auto status = children(identity, arguments, argument_count, copied);
        status != ON1X_SDK_OK) return status;
    return create(out_type, [&] {
        return make_candidate_type(
            identity,
            Type::named(std::move(copied_name), std::move(copied)));
    });
}

On1xSDK_Status on1x_sdk_type_create_own(
    const On1xSDK_IRIdentity *identity_record,
    const On1xSDK_Type *named_target,
    On1xSDK_Type **out_type) {
    using namespace on1x::sdk::ir;
    if (out_type) *out_type = nullptr;
    Identity identity;
    if (const auto status = candidate(identity_record, identity);
        status != ON1X_SDK_OK) return status;
    if (!named_target) return ON1X_SDK_INVALID_ARGUMENT;
    if (!same_identity(identity, named_target->identity)) return ON1X_SDK_BUSY;
    if (named_target->type.kind() != Type::Named)
        return ON1X_SDK_INVALID_ARGUMENT;
    return create(out_type, [&] {
        return make_candidate_type(identity, Type::own(named_target->type));
    });
}

On1xSDK_Status on1x_sdk_type_create_forall(
    const On1xSDK_IRIdentity *identity_record,
    size_t variable_count,
    const On1xSDK_Type *body,
    On1xSDK_Type **out_type) {
    using namespace on1x::sdk::ir;
    if (out_type) *out_type = nullptr;
    Identity identity;
    if (const auto status = candidate(identity_record, identity);
        status != ON1X_SDK_OK) return status;
    if (!body || !same_identity(identity, body->identity))
        return body ? ON1X_SDK_BUSY : ON1X_SDK_INVALID_ARGUMENT;
    return create(out_type, [&] {
        return make_candidate_type(
            identity, Type::for_all(variable_count, body->type));
    });
}

On1xSDK_Status on1x_sdk_type_create_code(
    const On1xSDK_IRIdentity *identity_record,
    const On1xSDK_Type *body,
    On1xSDK_Type **out_type) {
    using namespace on1x::sdk::ir;
    if (out_type) *out_type = nullptr;
    Identity identity;
    if (const auto status = candidate(identity_record, identity);
        status != ON1X_SDK_OK) return status;
    if (!body || !same_identity(identity, body->identity))
        return body ? ON1X_SDK_BUSY : ON1X_SDK_INVALID_ARGUMENT;
    return create(out_type, [&] {
        return make_candidate_type(identity, Type::code(body->type));
    });
}

void on1x_sdk_type_retain(On1xSDK_Type *type) {
    if (!type) return;
    auto current = type->references.load(std::memory_order_relaxed);
    while (current != std::numeric_limits<std::size_t>::max() &&
           !type->references.compare_exchange_weak(
               current, current + 1, std::memory_order_relaxed)) {}
}

void on1x_sdk_type_release(On1xSDK_Type *type) {
    if (type && type->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete type;
}

On1xSDK_Status on1x_sdk_type_identity(
    const On1xSDK_Type *type,
    On1xSDK_IRIdentity *out_identity) {
    if (!type || !out_identity) return ON1X_SDK_INVALID_ARGUMENT;
    on1x::sdk::ir::write_identity(type->identity, *out_identity);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_type_kind(
    const On1xSDK_Type *type,
    On1xSDK_TypeKind *out_kind) {
    if (!type || !out_kind) return ON1X_SDK_INVALID_ARGUMENT;
    *out_kind = static_cast<On1xSDK_TypeKind>(type->type.kind());
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_type_flags(
    const On1xSDK_Type *type,
    On1xSDK_TypeFlags *out_flags) {
    if (!type || !out_flags) return ON1X_SDK_INVALID_ARGUMENT;
    *out_flags = on1x::sdk::ir::flags(type->type);
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_type_variable(
    const On1xSDK_Type *type,
    uint64_t *out_variable) {
    if (!type || !out_variable) return ON1X_SDK_INVALID_ARGUMENT;
    if (type->type.kind() != on1x::Type::Generic &&
        type->type.kind() != on1x::Type::Bound &&
        !type->type.is_forall())
        return ON1X_SDK_UNSUPPORTED;
    *out_variable = type->type.generic_identity();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_type_extent(
    const On1xSDK_Type *type,
    size_t *out_extent) {
    if (!type || !out_extent) return ON1X_SDK_INVALID_ARGUMENT;
    if (!type->type.is_data()) return ON1X_SDK_UNSUPPORTED;
    *out_extent = type->type.extent();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_type_nominal(
    const On1xSDK_Type *type,
    On1xSDK_IRStringView *out_name) {
    if (!type || !out_name) return ON1X_SDK_INVALID_ARGUMENT;
    const auto &name = type->type.nominal();
    *out_name = {name.data(), name.size()};
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_type_label_count(
    const On1xSDK_Type *type,
    size_t *out_count) {
    if (!type || !out_count) return ON1X_SDK_INVALID_ARGUMENT;
    if (!type->type.is_data()) return ON1X_SDK_UNSUPPORTED;
    *out_count = type->type.labels().size();
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_type_label(
    const On1xSDK_Type *type,
    size_t index,
    On1xSDK_IRStringView *out_label) {
    if (!type || !out_label) return ON1X_SDK_INVALID_ARGUMENT;
    if (!type->type.is_data()) return ON1X_SDK_UNSUPPORTED;
    if (index >= type->type.labels().size())
        return ON1X_SDK_INVALID_ARGUMENT;
    const auto &label = type->type.labels()[index];
    *out_label = {label.data(), label.size()};
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_type_child_count(
    const On1xSDK_Type *type,
    size_t *out_count) {
    if (!type || !out_count) return ON1X_SDK_INVALID_ARGUMENT;
    const auto &value = type->type;
    if (value.is_function())
        *out_count = value.parameters().size() + 1;
    else if (value.is_tuple() || value.is_data())
        *out_count = value.elements().size() + value.arguments().size();
    else if (value.kind() == on1x::Type::Named)
        *out_count = value.arguments().size();
    else if (value.is_forall() || value.is_code() || value.is_own())
        *out_count = 1;
    else
        *out_count = 0;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_type_child(
    const On1xSDK_Type *type,
    size_t index,
    On1xSDK_TypeChildRole *out_role,
    On1xSDK_Type **out_child) {
    using namespace on1x::sdk::ir;
    if (!out_child) return ON1X_SDK_INVALID_ARGUMENT;
    *out_child = nullptr;
    if (!type || !out_role) return ON1X_SDK_INVALID_ARGUMENT;
    return guard([&] {
        const Type *child = nullptr;
        const auto &value = type->type;
        if (value.is_function()) {
            if (index < value.parameters().size()) {
                child = &value.parameters()[index];
                *out_role = ON1X_SDK_TYPE_CHILD_PARAMETER;
            } else if (index == value.parameters().size()) {
                child = &value.result();
                *out_role = ON1X_SDK_TYPE_CHILD_RESULT;
            }
        } else if (value.is_tuple() || value.is_data()) {
            if (index < value.elements().size()) {
                child = &value.elements()[index];
                *out_role = ON1X_SDK_TYPE_CHILD_ELEMENT;
            } else if (index - value.elements().size() <
                       value.arguments().size()) {
                child = &value.arguments()[index - value.elements().size()];
                *out_role = ON1X_SDK_TYPE_CHILD_ARGUMENT;
            }
        } else if (value.kind() == Type::Named) {
            if (index < value.arguments().size()) {
                child = &value.arguments()[index];
                *out_role = ON1X_SDK_TYPE_CHILD_ARGUMENT;
            }
        } else if (value.is_forall() && index == 0) {
            child = &value.body();
            *out_role = ON1X_SDK_TYPE_CHILD_BODY;
        } else if (value.is_code() && index == 0) {
            child = &value.code_result();
            *out_role = ON1X_SDK_TYPE_CHILD_BODY;
        } else if (value.is_own() && index == 0) {
            child = &value.owned();
            *out_role = ON1X_SDK_TYPE_CHILD_OWNED;
        }
        if (!child) return ON1X_SDK_INVALID_ARGUMENT;
        *out_child = make_type(type->identity, *child);
        return ON1X_SDK_OK;
    });
}

On1xSDK_Status on1x_sdk_type_spelling(
    const On1xSDK_Type *type,
    char *buffer,
    size_t capacity,
    size_t *out_required) {
    if (!type || !out_required || (!buffer && capacity))
        return ON1X_SDK_INVALID_ARGUMENT;
    return on1x::sdk::ir::guard([&] {
        const auto text = on1x::type_name(type->type);
        *out_required = text.size();
        if (capacity < text.size()) return ON1X_SDK_LIMIT;
        if (!text.empty()) {
            if (!buffer) return ON1X_SDK_INVALID_ARGUMENT;
            std::memcpy(buffer, text.data(), text.size());
        }
        return ON1X_SDK_OK;
    });
}

On1xSDK_Status on1x_sdk_type_verify_form(
    const On1xSDK_Type *type,
    const On1xSDK_TypeFormOptions *options,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics) {
    using namespace on1x::sdk::ir;
    set_failure(out_failure, ON1X_SDK_IR_FAILURE_NONE);
    if (!type) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    TypeFormOptions canonical;
    if (const auto status = form_options(options, canonical);
        status != ON1X_SDK_OK) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return status;
    }
    const auto status = guard([&] {
        const auto result = check_type_form(type->type, canonical);
        if (result.status == TypeFormStatus::Limit) {
            set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
            emit_diagnostic(diagnostics, "ON1X-SDK-TYPE-LIMIT", result.message);
            return ON1X_SDK_LIMIT;
        }
        if (result.status == TypeFormStatus::Malformed) {
            set_failure(out_failure, ON1X_SDK_IR_FAILURE_MALFORMED);
            emit_diagnostic(diagnostics, "ON1X-SDK-TYPE-FORM", result.message);
            return ON1X_SDK_ERROR;
        }
        return ON1X_SDK_OK;
    });
    if (status == ON1X_SDK_LIMIT &&
        (!out_failure || *out_failure == ON1X_SDK_IR_FAILURE_NONE))
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
    else if (status != ON1X_SDK_OK &&
             (!out_failure || *out_failure == ON1X_SDK_IR_FAILURE_NONE))
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_MALFORMED);
    return status;
}

On1xSDK_Status on1x_sdk_type_equal(
    const On1xSDK_Type *left,
    const On1xSDK_Type *right,
    uint32_t *out_equal) {
    if (!left || !right || !out_equal) return ON1X_SDK_INVALID_ARGUMENT;
    if (!on1x::sdk::ir::same_identity(left->identity, right->identity))
        return ON1X_SDK_BUSY;
    *out_equal = left->type == right->type;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_type_forall_instance(
    const On1xSDK_Type *schema,
    const On1xSDK_Type *instance,
    uint32_t *out_matches) {
    if (!schema || !instance || !out_matches)
        return ON1X_SDK_INVALID_ARGUMENT;
    if (!on1x::sdk::ir::same_identity(schema->identity, instance->identity))
        return ON1X_SDK_BUSY;
    return on1x::sdk::ir::guard([&] {
        *out_matches = on1x::forall_instance(schema->type, instance->type);
        return ON1X_SDK_OK;
    });
}

} // extern "C"
