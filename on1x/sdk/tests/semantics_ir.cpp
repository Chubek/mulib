#include "CanonicalBridge.hpp"

#include "On1xSDK-IRSerialize.h"
#include "On1xSDK-IRTransaction.h"
#include "On1xSDK-Semantics.h"

#include <on1x/ast.hpp>

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}

void ok(On1xSDK_Status status, const char *message) {
    if (status != ON1X_SDK_OK)
        throw std::runtime_error(std::string(message) +
                                 ": status " + std::to_string(status));
}

On1xSDK_IRIdentity identity(const On1xSDK_Context *owner,
                            std::uint64_t generation,
                            std::uint64_t revision) {
    return {ON1X_SDK_SEMANTICS_IR_ABI_VERSION,
            sizeof(On1xSDK_IRIdentity),
            owner,
            generation,
            revision,
            ON1X_SDK_IR_CANDIDATE,
            ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION};
}

On1xSDK_IRDecodeOptions decode_options(On1xSDK_IRIdentity value) {
    static constexpr char filename[] = "sdk-wire.on1x";
    return {ON1X_SDK_SEMANTICS_IR_ABI_VERSION,
            sizeof(On1xSDK_IRDecodeOptions),
            value,
            {filename, sizeof filename - 1},
            ON1X_SDK_IR_MAX_WIRE_BYTES,
            ON1X_SDK_IR_MAX_NODES,
            ON1X_SDK_IR_MAX_NODES * 16u,
            ON1X_SDK_IR_MAX_DEPTH};
}

on1x::Unit source(std::string text, std::string file = "sdk-test.on1x") {
    return on1x::parse(on1x::scan(text, file));
}

struct Diagnostics {
    std::vector<std::string> entries;
    static void emit(void *userdata, const On1xSDK_Diagnostic *record) {
        auto &self = *static_cast<Diagnostics *>(userdata);
        self.entries.emplace_back(record && record->message
                                      ? record->message : "<missing>");
    }
    On1xSDK_DiagnosticSink sink() {
        return {emit, this};
    }
};

struct Types {
    std::vector<On1xSDK_Type *> values;
    ~Types() {
        for (auto *value : values) on1x_sdk_type_release(value);
    }
    On1xSDK_Type *take(On1xSDK_Type *value) {
        values.push_back(value);
        return value;
    }
};

std::vector<std::uint8_t> bytes(const On1xSDK_IRBlob *blob) {
    On1xSDK_IRBytesView view{};
    ok(on1x_sdk_ir_blob_bytes(blob, &view), "blob bytes");
    return {view.data, view.data + view.size};
}

On1xSDK_Status count_hir(void *userdata,
                         const On1xSDK_HIRSnapshot *,
                         const On1xSDK_HIRNodeRef *,
                         size_t) {
    ++*static_cast<std::size_t *>(userdata);
    return ON1X_SDK_OK;
}

On1xSDK_Status inspect_hir(void *userdata,
                           const On1xSDK_HIRSnapshot *snapshot,
                           const On1xSDK_HIRNodeRef *node,
                           size_t) {
    auto &count = *static_cast<std::size_t *>(userdata);
    ++count;
    On1xSDK_HIRNodeClass node_class{};
    std::uint32_t kind = 0;
    On1xSDK_HIRNodeFlags flags = 0;
    On1xSDK_IRStringView name{};
    On1xSDK_IRSourceSpan span{};
    size_t children = 0;
    if (on1x_sdk_hir_node_class(snapshot, node, &node_class) !=
            ON1X_SDK_OK ||
        on1x_sdk_hir_node_kind(snapshot, node, &kind) != ON1X_SDK_OK ||
        on1x_sdk_hir_node_flags(snapshot, node, &flags) != ON1X_SDK_OK ||
        on1x_sdk_hir_node_name(snapshot, node, &name) != ON1X_SDK_OK ||
        on1x_sdk_hir_node_span(snapshot, node, &span) != ON1X_SDK_OK ||
        on1x_sdk_hir_node_child_count(snapshot, node, &children) !=
            ON1X_SDK_OK)
        return ON1X_SDK_ERROR;
    (void)kind;
    (void)flags;
    if (node_class < ON1X_SDK_HIR_NODE_FUNCTION ||
        node_class > ON1X_SDK_HIR_NODE_CAPTURE ||
        !span.source.data || !span.line || !span.column ||
        (name.size && !name.data))
        return ON1X_SDK_ERROR;
    On1xSDK_Type *type = nullptr;
    const auto type_status =
        on1x_sdk_hir_node_type(snapshot, node, &type);
    if (type_status == ON1X_SDK_OK)
        on1x_sdk_type_release(type);
    else if (type_status != ON1X_SDK_UNSUPPORTED)
        return ON1X_SDK_ERROR;
    for (size_t index = 0; index < children; ++index) {
        On1xSDK_HIREdgeKind edge{};
        On1xSDK_HIRNodeRef child{};
        if (on1x_sdk_hir_node_child(
                snapshot, node, index, &edge, &child) != ON1X_SDK_OK ||
            edge < ON1X_SDK_HIR_EDGE_PARAMETER ||
            edge > ON1X_SDK_HIR_EDGE_CAPTURE ||
            !child.id)
            return ON1X_SDK_ERROR;
    }
    return ON1X_SDK_OK;
}

On1xSDK_Status reject_hir(void *, const On1xSDK_HIRSnapshot *,
                          const On1xSDK_HIRNodeRef *, size_t) {
    return ON1X_SDK_ERROR;
}

On1xSDK_Status stale_hir(void *, const On1xSDK_HIRSnapshot *,
                         const On1xSDK_HIRNodeRef *, size_t) {
    return ON1X_SDK_STALE;
}

struct InstructionSearch {
    std::size_t count = 0;
    bool found_constant = false;
    bool found_operand = false;
    On1xSDK_MIRRef constant{};
    On1xSDK_MIRRef operand{};
};

On1xSDK_Status find_instruction(void *userdata,
                                const On1xSDK_MIRSnapshot *snapshot,
                                const On1xSDK_MIRRef *instruction) {
    auto &search = *static_cast<InstructionSearch *>(userdata);
    ++search.count;
    On1xSDK_MIROp op{};
    if (on1x_sdk_mir_instruction_op(snapshot, instruction, &op) !=
        ON1X_SDK_OK)
        return ON1X_SDK_ERROR;
    if (!search.found_constant && op == ON1X_SDK_MIR_CONSTANT) {
        search.found_constant = true;
        search.constant = *instruction;
    }
    size_t operands = 0;
    if (on1x_sdk_mir_instruction_operand_count(
            snapshot, instruction, &operands) != ON1X_SDK_OK)
        return ON1X_SDK_ERROR;
    if (!search.found_operand && operands) {
        search.found_operand = true;
        search.operand = *instruction;
    }
    return ON1X_SDK_OK;
}

On1xSDK_Status reject_instruction(void *,
                                  const On1xSDK_MIRSnapshot *,
                                  const On1xSDK_MIRRef *) {
    return static_cast<On1xSDK_Status>(999);
}

void test_types(const On1xSDK_Context *owner,
                const On1xSDK_Context *other) {
    Types held;
    auto id = identity(owner, 7, 11);
    auto prefix_identity = id;
    prefix_identity.struct_size =
        offsetof(On1xSDK_IRIdentity, state);
    ok(on1x_sdk_ir_identity_validate(
           &prefix_identity, ON1X_SDK_IR_CANDIDATE),
       "identity prefix negotiation");
    require(on1x_sdk_ir_identity_validate(
                &prefix_identity, ON1X_SDK_IR_CHECKED_HIR) ==
                ON1X_SDK_INVALID_ARGUMENT,
            "identity prefix forged checked state");
    On1xSDK_Type *i64 = nullptr, *boolean = nullptr;
    ok(on1x_sdk_type_create_atomic(
           &id, ON1X_SDK_TYPE_I64, 0, &i64), "create i64");
    held.take(i64);
    ok(on1x_sdk_type_create_atomic(
           &id, ON1X_SDK_TYPE_BOOL, 0, &boolean), "create bool");
    held.take(boolean);
    On1xSDK_IRIdentity type_identity{};
    ok(on1x_sdk_type_identity(i64, &type_identity), "type identity");
    require(type_identity.owner == owner &&
                type_identity.generation == id.generation &&
                type_identity.revision == id.revision &&
                type_identity.state == ON1X_SDK_IR_CANDIDATE,
            "type identity changed");

    const On1xSDK_Type *elements[]{i64, boolean};
    On1xSDK_Type *tuple = nullptr;
    ok(on1x_sdk_type_create_tuple(
           &id, elements, 2, &tuple), "create tuple");
    held.take(tuple);
    size_t child_count = 0;
    ok(on1x_sdk_type_child_count(tuple, &child_count),
       "tuple child count");
    require(child_count == 2, "tuple child count mismatch");
    On1xSDK_TypeChildRole role{};
    On1xSDK_Type *child = nullptr;
    ok(on1x_sdk_type_child(tuple, 1, &role, &child),
       "tuple child");
    held.take(child);
    require(role == ON1X_SDK_TYPE_CHILD_ELEMENT,
            "tuple child role mismatch");
    std::uint32_t equal = 0;
    ok(on1x_sdk_type_equal(boolean, child, &equal), "type equality");
    require(equal == 1, "type child changed");
    On1xSDK_TypeKind kind{};
    ok(on1x_sdk_type_kind(tuple, &kind), "tuple kind");
    require(kind == ON1X_SDK_TYPE_TUPLE, "tuple kind mismatch");

    size_t required = 0;
    require(on1x_sdk_type_spelling(tuple, nullptr, 0, &required) ==
                ON1X_SDK_LIMIT &&
            required != 0,
            "spelling sizing contract failed");
    std::string spelling(required, '\0');
    ok(on1x_sdk_type_spelling(
           tuple, spelling.data(), spelling.size(), &required),
       "type spelling");
    require(spelling == "(i64, bool)", "canonical type spelling mismatch");

    On1xSDK_TypeFormOptions tiny{
        ON1X_SDK_SEMANTICS_IR_ABI_VERSION,
        sizeof(On1xSDK_TypeFormOptions),
        0, 32, 1, 0, 0, 0, 1
    };
    On1xSDK_IRFailure failure{};
    On1xSDK_TypeFormOptions prefix_form{
        ON1X_SDK_SEMANTICS_IR_ABI_VERSION,
        sizeof(std::uint32_t) * 2u,
        999, 999, 999, 1, 1, 1, 0
    };
    ok(on1x_sdk_type_verify_form(
           tuple, &prefix_form, &failure, nullptr),
       "type-form prefix negotiation");
    require(on1x_sdk_type_verify_form(
                tuple, &tiny, &failure, nullptr) == ON1X_SDK_LIMIT &&
            failure == ON1X_SDK_IR_FAILURE_LIMIT,
            "type form node limit was not distinct");

    On1xSDK_Type *hole = nullptr;
    ok(on1x_sdk_type_create_atomic(
           &id, ON1X_SDK_TYPE_HOLE, 0, &hole), "create hole");
    held.take(hole);
    require(on1x_sdk_type_verify_form(
                hole, nullptr, &failure, nullptr) == ON1X_SDK_ERROR &&
            failure == ON1X_SDK_IR_FAILURE_MALFORMED,
            "unresolved hole passed default type-form checking");

    On1xSDK_Type *bound = nullptr, *body = nullptr, *scheme = nullptr;
    ok(on1x_sdk_type_create_atomic(
           &id, ON1X_SDK_TYPE_BOUND, 0, &bound), "create bound");
    held.take(bound);
    const On1xSDK_Type *parameter[]{bound};
    ok(on1x_sdk_type_create_function(
           &id, parameter, 1, bound, &body), "create forall body");
    held.take(body);
    ok(on1x_sdk_type_create_forall(
           &id, 1, body, &scheme), "create forall");
    held.take(scheme);
    On1xSDK_Type *instance = nullptr;
    const On1xSDK_Type *concrete_parameter[]{i64};
    ok(on1x_sdk_type_create_function(
           &id, concrete_parameter, 1, i64, &instance),
       "create forall instance");
    held.take(instance);
    std::uint32_t matches = 0;
    ok(on1x_sdk_type_forall_instance(
           scheme, instance, &matches), "forall instance");
    require(matches == 1, "forall instance mismatch");
    std::uint64_t variable = 0;
    ok(on1x_sdk_type_variable(bound, &variable), "bound variable");
    require(variable == 0, "bound variable mismatch");
    ok(on1x_sdk_type_variable(scheme, &variable), "forall arity");
    require(variable == 1, "forall arity mismatch");
    require(on1x_sdk_type_variable(i64, &variable) ==
                ON1X_SDK_UNSUPPORTED,
            "atomic type exposed a variable");

    static constexpr char right[] = "right";
    static constexpr char left[] = "left";
    const On1xSDK_IRStringView labels[]{
        {right, sizeof right - 1}, {left, sizeof left - 1}
    };
    On1xSDK_Type *record = nullptr;
    ok(on1x_sdk_type_create_data(
           &id, ON1X_SDK_TYPE_RECORD, elements, labels, 2, 0,
           {}, nullptr, 0, &record), "create record");
    held.take(record);
    size_t label_count = 0;
    ok(on1x_sdk_type_label_count(record, &label_count),
       "record label count");
    require(label_count == 2, "record labels lost");
    On1xSDK_IRStringView label{};
    ok(on1x_sdk_type_label(record, 0, &label), "record label");
    require(std::string(label.data, label.size) == "left",
            "record labels are not canonical");

    const On1xSDK_Type *array_element[]{i64};
    On1xSDK_Type *array = nullptr;
    ok(on1x_sdk_type_create_data(
           &id, ON1X_SDK_TYPE_ARRAY, array_element, nullptr, 1, 4,
           {}, nullptr, 0, &array), "create array");
    held.take(array);
    size_t extent = 0;
    ok(on1x_sdk_type_extent(array, &extent), "array extent");
    require(extent == 4, "array extent mismatch");
    require(on1x_sdk_type_extent(i64, &extent) ==
                 ON1X_SDK_UNSUPPORTED,
             "atomic type exposed a data extent");

    On1xSDK_Type *u8 = nullptr, *channel = nullptr;
    ok(on1x_sdk_type_create_atomic(
           &id, ON1X_SDK_TYPE_U8, 0, &u8), "create u8");
    held.take(u8);
    ok(on1x_sdk_type_create_csp(
           &id, ON1X_SDK_TYPE_CHANNEL, u8, 2, 2, &channel),
       "create typed channel");
    held.take(channel);
    ok(on1x_sdk_type_kind(channel, &kind), "channel kind");
    require(kind == ON1X_SDK_TYPE_CHANNEL, "channel kind mismatch");
    size_t senders = 0, receivers = 0;
    ok(on1x_sdk_type_csp_cardinality(
           channel, &senders, &receivers), "channel cardinality");
    require(senders == 2 && receivers == 2,
            "channel cardinality mismatch");
    ok(on1x_sdk_type_child_count(channel, &child_count),
       "channel child count");
    require(child_count == 1, "channel payload count mismatch");
    ok(on1x_sdk_type_child(channel, 0, &role, &child),
       "channel payload");
    held.take(child);
    require(role == ON1X_SDK_TYPE_CHILD_PAYLOAD,
            "channel payload role mismatch");
    ok(on1x_sdk_type_equal(u8, child, &equal),
       "channel payload equality");
    require(equal == 1, "channel payload changed");
    On1xSDK_Type *legacy_endpoint = reinterpret_cast<On1xSDK_Type *>(1);
    require(on1x_sdk_type_create_atomic(
                &id, ON1X_SDK_TYPE_BYTE_SENDER, 0, &legacy_endpoint) ==
                ON1X_SDK_UNSUPPORTED &&
            legacy_endpoint == nullptr,
            "legacy byte endpoint did not reject explicitly");

    static constexpr char pair_name[] = "Pair";
    const On1xSDK_Type *nominal_arguments[]{i64};
    On1xSDK_Type *nominal_record = nullptr;
    ok(on1x_sdk_type_create_data(
           &id, ON1X_SDK_TYPE_RECORD, elements, labels, 2, 0,
           {pair_name, sizeof pair_name - 1},
           nominal_arguments, 1, &nominal_record),
       "create nominal record");
    held.take(nominal_record);
    On1xSDK_IRStringView nominal{};
    ok(on1x_sdk_type_nominal(nominal_record, &nominal),
       "nominal identity");
    require(std::string(nominal.data, nominal.size) == "Pair",
            "nominal identity mismatch");

    static constexpr char node_name[] = "Node";
    On1xSDK_Type *named = nullptr, *own = nullptr, *code = nullptr;
    ok(on1x_sdk_type_create_named(
           &id, {node_name, sizeof node_name - 1},
           nominal_arguments, 1, &named), "create named type");
    held.take(named);
    ok(on1x_sdk_type_create_own(&id, named, &own), "create owning type");
    held.take(own);
    On1xSDK_TypeFlags own_flags = 0;
    ok(on1x_sdk_type_flags(own, &own_flags), "own flags");
    require((own_flags & ON1X_SDK_TYPE_FLAG_MOVE_ONLY) &&
                (own_flags & ON1X_SDK_TYPE_FLAG_CONTAINS_UNIQUE),
            "owning type flags missing");
    ok(on1x_sdk_type_create_code(&id, boolean, &code),
       "create code type");
    held.take(code);
    ok(on1x_sdk_type_child(code, 0, &role, &child),
       "code body");
    held.take(child);
    require(role == ON1X_SDK_TYPE_CHILD_BODY,
            "code child role mismatch");

    On1xSDK_Type *invalid_type =
        reinterpret_cast<On1xSDK_Type *>(1);
    require(on1x_sdk_type_create_own(&id, i64, &invalid_type) ==
                ON1X_SDK_INVALID_ARGUMENT &&
            invalid_type == nullptr,
            "owning type accepted a non-named target");
    require(on1x_sdk_type_create_atomic(
                &id, ON1X_SDK_TYPE_FUNCTION, 0, &invalid_type) ==
                ON1X_SDK_INVALID_ARGUMENT &&
            invalid_type == nullptr,
            "atomic factory accepted a composite kind");
    require(on1x_sdk_type_create_tuple(
                &id, elements, 1, &invalid_type) ==
                ON1X_SDK_INVALID_ARGUMENT &&
            invalid_type == nullptr,
            "tuple factory accepted one element");
    On1xSDK_Type *deep_type = i64;
    on1x_sdk_type_retain(deep_type);
    for (std::size_t depth = 0; depth < ON1X_SDK_IR_MAX_DEPTH; ++depth) {
        On1xSDK_Type *next = nullptr;
        ok(on1x_sdk_type_create_code(&id, deep_type, &next),
           "bounded nested type");
        on1x_sdk_type_release(deep_type);
        deep_type = next;
    }
    invalid_type = reinterpret_cast<On1xSDK_Type *>(1);
    require(on1x_sdk_type_create_code(
                &id, deep_type, &invalid_type) == ON1X_SDK_LIMIT &&
            invalid_type == nullptr,
            "candidate type depth limit was not enforced");
    on1x_sdk_type_release(deep_type);

    auto other_id = identity(other, 7, 11);
    On1xSDK_Type *other_i64 = nullptr;
    ok(on1x_sdk_type_create_atomic(
           &other_id, ON1X_SDK_TYPE_I64, 0, &other_i64),
       "create other-context type");
    held.take(other_i64);
    const On1xSDK_Type *mixed[]{i64, other_i64};
    On1xSDK_Type *bad = reinterpret_cast<On1xSDK_Type *>(1);
    require(on1x_sdk_type_create_tuple(&id, mixed, 2, &bad) ==
                ON1X_SDK_BUSY &&
            bad == nullptr,
            "cross-context type construction was accepted");

    auto forged = id;
    forged.state = ON1X_SDK_IR_CHECKED_HIR;
    bad = reinterpret_cast<On1xSDK_Type *>(1);
    require(on1x_sdk_type_create_atomic(
                &forged, ON1X_SDK_TYPE_I64, 0, &bad) ==
                ON1X_SDK_INVALID_ARGUMENT &&
            bad == nullptr,
            "external type construction forged checked state");
}

void test_hir_and_mir(const On1xSDK_Context *owner) {
    auto candidate_id = identity(owner, 9, 1);
    Diagnostics diagnostics;
    auto sink = diagnostics.sink();
    On1xSDK_IRFailure failure{};

    auto invalid = source("fn main()->i64{return true;}", "invalid.on1x");
    On1xSDK_HIRSnapshot *bad_hir =
        reinterpret_cast<On1xSDK_HIRSnapshot *>(1);
    require(on1x::sdk::ir::check_hir_candidate(
                invalid, candidate_id, "invalid", &bad_hir,
                &failure, &sink) == ON1X_SDK_ERROR &&
            bad_hir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_SEMANTIC &&
            !diagnostics.entries.empty(),
            "invalid HIR candidate was published");

    auto foreign = source(
        "extern fn native(x:i64)->i64;"
        "fn main()->i64{return native(1);}",
        "foreign.on1x");
    bad_hir = reinterpret_cast<On1xSDK_HIRSnapshot *>(1);
    require(on1x::sdk::ir::check_hir_candidate(
                foreign, candidate_id, "foreign", &bad_hir,
                &failure, nullptr) == ON1X_SDK_UNSUPPORTED &&
            bad_hir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_UNSUPPORTED,
            "checked HIR bypassed host foreign admission");

    auto unit = source(
        "fn add(x:i64,y:i64)->i64{return x+y;}"
        "fn main()->i64{return add(40,2);}");
    On1xSDK_HIRSnapshot *hir = nullptr;
    ok(on1x::sdk::ir::check_hir_candidate(
           unit, candidate_id, "sdk-test", &hir, &failure, nullptr),
       "canonical HIR checking");
    require(hir != nullptr, "checked HIR missing");
    On1xSDK_IRIdentity hir_identity{};
    ok(on1x_sdk_hir_snapshot_identity(hir, &hir_identity),
       "HIR identity");
    require(hir_identity.state == ON1X_SDK_IR_CHECKED_HIR,
            "canonical HIR state mismatch");
    ok(on1x_sdk_semantics_verify_checked_hir(
           hir, &failure, nullptr), "checked HIR verification");
    ok(on1x_sdk_hir_verify_structure(
           hir, nullptr, &failure, nullptr), "HIR structure");

    size_t function_count = 0, node_count = 0, visits = 0;
    ok(on1x_sdk_hir_snapshot_function_count(hir, &function_count),
       "HIR function count");
    ok(on1x_sdk_hir_snapshot_node_count(hir, &node_count),
       "HIR node count");
    require(function_count == 2 && node_count >= function_count,
            "HIR view lost functions or nodes");
    ok(on1x_sdk_hir_visit(
           hir, nullptr, count_hir, &visits, &failure), "HIR visitor");
    require(visits == node_count, "HIR visitor did not cover snapshot");
    visits = 0;
    ok(on1x_sdk_hir_visit(
           hir, nullptr, inspect_hir, &visits, &failure),
       "structured HIR inspection");
    require(visits == node_count,
            "structured HIR inspection did not cover snapshot");
    require(on1x_sdk_hir_visit(
                hir, nullptr, reject_hir, nullptr, &failure) ==
                ON1X_SDK_ERROR &&
            failure == ON1X_SDK_IR_FAILURE_CALLBACK,
            "HIR callback failure was not propagated");
    require(on1x_sdk_hir_visit(
                hir, nullptr, stale_hir, nullptr, &failure) ==
                ON1X_SDK_STALE &&
            failure == ON1X_SDK_IR_FAILURE_CALLBACK,
            "shared HIR callback status was not propagated");

    On1xSDK_HIRNodeRef first_function{};
    ok(on1x_sdk_hir_snapshot_function(
           hir, 0, &first_function), "HIR function view");
    On1xSDK_IRStringView function_name{};
    ok(on1x_sdk_hir_node_name(
           hir, &first_function, &function_name), "HIR function name");
    require(std::string(function_name.data, function_name.size) == "add",
            "HIR function order/name changed");

    On1xSDK_IRBlob *hir_blob = nullptr;
    ok(on1x_sdk_hir_serialize(hir, &hir_blob), "HIR serialization");
    const auto hir_wire = bytes(hir_blob);
    auto hir_decode = decode_options(candidate_id);
    auto prefix_hir_decode = hir_decode;
    prefix_hir_decode.struct_size =
        offsetof(On1xSDK_IRDecodeOptions, filename);
    On1xSDK_HIRSnapshot *prefix_wire_hir = nullptr;
    ok(on1x_sdk_hir_deserialize(
           hir_wire.data(), hir_wire.size(), &prefix_hir_decode,
           &prefix_wire_hir, &failure, nullptr),
       "HIR decode prefix negotiation");
    on1x_sdk_hir_snapshot_release(prefix_wire_hir);
    On1xSDK_HIRSnapshot *wire_hir = nullptr;
    ok(on1x_sdk_hir_deserialize(
           hir_wire.data(), hir_wire.size(), &hir_decode, &wire_hir,
           &failure, nullptr), "HIR deserialization");
    On1xSDK_IRIdentity wire_hir_identity{};
    ok(on1x_sdk_hir_snapshot_identity(wire_hir, &wire_hir_identity),
       "wire HIR identity");
    require(wire_hir_identity.state == ON1X_SDK_IR_CANDIDATE,
            "external HIR was promoted");
    require(on1x_sdk_semantics_verify_checked_hir(
                wire_hir, &failure, nullptr) == ON1X_SDK_UNSUPPORTED &&
            failure == ON1X_SDK_IR_FAILURE_UNSUPPORTED,
            "inspection HIR was treated as semantically checked");
    require(on1x_sdk_hir_node_name(
                wire_hir, &first_function, &function_name) ==
                ON1X_SDK_BUSY,
            "cross-snapshot HIR view was accepted");
    On1xSDK_IRBlob *hir_blob_again = nullptr;
    ok(on1x_sdk_hir_serialize(wire_hir, &hir_blob_again),
       "wire HIR reserialization");
    require(bytes(hir_blob_again) == hir_wire,
            "HIR wire round-trip is not deterministic");

    On1xSDK_HIRSnapshot *malformed_hir =
        reinterpret_cast<On1xSDK_HIRSnapshot *>(1);
    require(on1x_sdk_hir_deserialize(
                hir_wire.data(), hir_wire.size() - 1, &hir_decode,
                &malformed_hir, &failure, nullptr) == ON1X_SDK_ERROR &&
            malformed_hir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_MALFORMED,
            "truncated HIR wire was accepted");
    auto limited_hir_decode = hir_decode;
    limited_hir_decode.max_nodes = 1;
    require(on1x_sdk_hir_deserialize(
                hir_wire.data(), hir_wire.size(), &limited_hir_decode,
                &malformed_hir, &failure, nullptr) == ON1X_SDK_LIMIT &&
            malformed_hir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_LIMIT,
            "HIR decode node budget was not enforced");
    auto stale_hir_decode = hir_decode;
    ++stale_hir_decode.identity.revision;
    require(on1x_sdk_hir_deserialize(
                hir_wire.data(), hir_wire.size(), &stale_hir_decode,
                &malformed_hir, &failure, nullptr) == ON1X_SDK_BUSY &&
            malformed_hir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_STALE_REVISION,
            "stale HIR revision was accepted");
    auto wrong_generation_hir_decode = hir_decode;
    ++wrong_generation_hir_decode.identity.generation;
    require(on1x_sdk_hir_deserialize(
                hir_wire.data(), hir_wire.size(),
                &wrong_generation_hir_decode, &malformed_hir,
                &failure, nullptr) == ON1X_SDK_BUSY &&
            malformed_hir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_CROSS_CONTEXT,
            "wrong HIR generation was accepted");

    On1xSDK_IRLimits tiny{
        ON1X_SDK_SEMANTICS_IR_ABI_VERSION,
        sizeof(On1xSDK_IRLimits), 1, 100, 32
    };
    On1xSDK_MIRSnapshot *limited_mir =
        reinterpret_cast<On1xSDK_MIRSnapshot *>(1);
    require(on1x_sdk_semantics_lower_checked_hir(
                hir, 2, &tiny, &limited_mir, &failure, nullptr) ==
                ON1X_SDK_LIMIT &&
            limited_mir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_LIMIT,
            "lowering budget exhaustion was not distinct");

    On1xSDK_MIRSnapshot *mir = nullptr;
    ok(on1x_sdk_semantics_lower_checked_hir(
           hir, 2, nullptr, &mir, &failure, nullptr),
       "checked HIR lowering");
    On1xSDK_IRIdentity mir_identity{};
    ok(on1x_sdk_mir_snapshot_identity(mir, &mir_identity),
       "MIR identity");
    require(mir_identity.state == ON1X_SDK_IR_VERIFIED_MIR &&
                mir_identity.revision == 2,
            "lowered MIR identity mismatch");
    ok(on1x_sdk_semantics_verify_mir(
           mir, nullptr, &failure, nullptr), "MIR verification");
    std::uint32_t format_version = 0;
    std::uint64_t module_id = 0;
    size_t mir_function_count = 0;
    On1xSDK_IRStringView mir_provenance{};
    ok(on1x_sdk_mir_snapshot_format_version(
           mir, &format_version), "MIR format version");
    ok(on1x_sdk_mir_snapshot_module_id(mir, &module_id), "MIR module ID");
    ok(on1x_sdk_mir_snapshot_provenance(
           mir, &mir_provenance), "MIR provenance");
    ok(on1x_sdk_mir_snapshot_function_count(
           mir, &mir_function_count), "MIR function count");
    require(format_version >= 1 && format_version <= 3 &&
                module_id && mir_function_count == 2 &&
                std::string(mir_provenance.data, mir_provenance.size) ==
                    "sdk-test",
            "MIR snapshot metadata mismatch");

    On1xSDK_MIRRef first_mir_function{};
    for (size_t function_index = 0;
         function_index < mir_function_count; ++function_index) {
        On1xSDK_MIRRef function{};
        ok(on1x_sdk_mir_snapshot_function(
               mir, function_index, &function), "MIR function");
        if (!function_index) first_mir_function = function;
        On1xSDK_IRStringView name{};
        On1xSDK_Type *result_type = nullptr;
        size_t slots = 0, blocks = 0;
        ok(on1x_sdk_mir_function_name(mir, &function, &name),
           "MIR function name");
        ok(on1x_sdk_mir_function_result(
               mir, &function, &result_type), "MIR function result");
        on1x_sdk_type_release(result_type);
        ok(on1x_sdk_mir_function_slot_count(
               mir, &function, &slots), "MIR slot count");
        ok(on1x_sdk_mir_function_block_count(
               mir, &function, &blocks), "MIR block count");
        require(name.size && blocks, "MIR function metadata missing");
        for (size_t slot_index = 0; slot_index < slots; ++slot_index) {
            std::uint64_t slot_id = 0;
            On1xSDK_Type *slot_type = nullptr;
            On1xSDK_IRSourceSpan slot_span{};
            ok(on1x_sdk_mir_function_slot(
                   mir, &function, slot_index, &slot_id,
                   &slot_type, &slot_span), "MIR slot");
            require(slot_id && slot_span.line && slot_span.column,
                    "MIR slot metadata missing");
            on1x_sdk_type_release(slot_type);
        }
        for (size_t block_index = 0; block_index < blocks; ++block_index) {
            On1xSDK_MIRRef block{};
            ok(on1x_sdk_mir_function_block(
                   mir, &function, block_index, &block), "MIR block");
            std::uint64_t block_effect = 0;
            size_t parameters = 0, instructions = 0;
            ok(on1x_sdk_mir_block_effect(
                   mir, &block, &block_effect), "MIR block effect");
            ok(on1x_sdk_mir_block_parameter_count(
                   mir, &block, &parameters), "MIR parameter count");
            ok(on1x_sdk_mir_block_instruction_count(
                   mir, &block, &instructions), "MIR instruction count");
            require(block_effect, "MIR block effect token missing");
            for (size_t parameter_index = 0;
                 parameter_index < parameters; ++parameter_index) {
                std::uint64_t parameter_id = 0;
                On1xSDK_Type *parameter_type = nullptr;
                On1xSDK_IRSourceSpan parameter_span{};
                ok(on1x_sdk_mir_block_parameter(
                       mir, &block, parameter_index, &parameter_id,
                       &parameter_type, &parameter_span), "MIR parameter");
                require(parameter_id && parameter_span.line &&
                            parameter_span.column,
                        "MIR parameter metadata missing");
                on1x_sdk_type_release(parameter_type);
            }
            for (size_t instruction_index = 0;
                 instruction_index < instructions; ++instruction_index) {
                On1xSDK_MIRRef instruction{};
                ok(on1x_sdk_mir_block_instruction(
                       mir, &block, instruction_index, &instruction),
                   "MIR instruction");
                On1xSDK_MIROp instruction_op{};
                On1xSDK_MIREffect instruction_effect{};
                On1xSDK_Type *instruction_type = nullptr;
                std::int64_t immediate = 0;
                std::uint64_t target = 0, effect_in = 0, effect_out = 0;
                On1xSDK_IRStringView primitive{};
                On1xSDK_IRSourceSpan instruction_span{};
                size_t operands = 0;
                ok(on1x_sdk_mir_instruction_op(
                       mir, &instruction, &instruction_op), "MIR opcode");
                ok(on1x_sdk_mir_instruction_effect(
                       mir, &instruction, &instruction_effect), "MIR effect");
                ok(on1x_sdk_mir_instruction_type(
                       mir, &instruction, &instruction_type), "MIR type");
                ok(on1x_sdk_mir_instruction_immediate(
                       mir, &instruction, &immediate), "MIR immediate");
                ok(on1x_sdk_mir_instruction_target(
                       mir, &instruction, &target), "MIR target");
                ok(on1x_sdk_mir_instruction_effect_chain(
                       mir, &instruction, &effect_in, &effect_out),
                   "MIR effect chain");
                ok(on1x_sdk_mir_instruction_primitive(
                       mir, &instruction, &primitive), "MIR primitive");
                ok(on1x_sdk_mir_instruction_span(
                       mir, &instruction, &instruction_span), "MIR span");
                ok(on1x_sdk_mir_instruction_operand_count(
                       mir, &instruction, &operands), "MIR operand count");
                (void)instruction_op;
                (void)instruction_effect;
                (void)immediate;
                (void)target;
                (void)effect_in;
                (void)effect_out;
                require(instruction_span.line && instruction_span.column &&
                            (!primitive.size || primitive.data),
                        "MIR instruction metadata missing");
                for (size_t operand_index = 0;
                     operand_index < operands; ++operand_index) {
                    std::uint64_t operand = 0;
                    ok(on1x_sdk_mir_instruction_operand(
                           mir, &instruction, operand_index, &operand),
                       "MIR operand");
                    require(operand, "MIR operand identity missing");
                }
                on1x_sdk_type_release(instruction_type);
            }
            On1xSDK_MIRTerminator terminator{};
            ok(on1x_sdk_mir_block_terminator(
                   mir, &block, &terminator), "MIR terminator");
            require(terminator.id && terminator.span.line &&
                        terminator.span.column,
                    "MIR terminator metadata missing");
            for (size_t edge_index = 0;
                 edge_index < terminator.edge_count; ++edge_index) {
                On1xSDK_MIREdge edge{};
                ok(on1x_sdk_mir_terminator_edge(
                       mir, &block, edge_index, &edge), "MIR edge");
                require(edge.block, "MIR edge target missing");
                for (size_t argument_index = 0;
                     argument_index < edge.argument_count; ++argument_index) {
                    std::uint64_t argument = 0;
                    ok(on1x_sdk_mir_terminator_edge_argument(
                           mir, &block, edge_index, argument_index, &argument),
                       "MIR edge argument");
                    require(argument, "MIR edge argument missing");
                }
            }
        }
    }

    InstructionSearch search;
    ok(on1x_sdk_mir_visit_instructions(
           mir, nullptr, find_instruction, &search, &failure),
       "MIR visitor");
    require(search.count != 0 && search.found_constant &&
                search.found_operand,
            "structured MIR traversal lost instructions");
    require(on1x_sdk_mir_visit_instructions(
                mir, nullptr, reject_instruction, nullptr, &failure) ==
                ON1X_SDK_ERROR &&
            failure == ON1X_SDK_IR_FAILURE_CALLBACK,
            "unknown MIR callback status was not normalized");

    const char main_name[] = "main";
    On1xSDK_MIREvaluateOptions evaluate{
        ON1X_SDK_SEMANTICS_IR_ABI_VERSION,
        sizeof(On1xSDK_MIREvaluateOptions),
        {main_name, sizeof main_name - 1},
        nullptr, 0, 100000, 64
    };
    On1xSDK_MIREvaluation *execution = nullptr;
    ok(on1x_sdk_mir_evaluate(mir, &evaluate, &execution),
       "MIR evaluation");
    On1xSDK_MIREvaluationStatus evaluation_status{};
    ok(on1x_sdk_mir_evaluation_status(
           execution, &evaluation_status), "evaluation status");
    require(evaluation_status == ON1X_SDK_MIR_EVALUATION_RETURNED,
            "verified MIR did not return");
    std::int64_t scalar = 0;
    ok(on1x_sdk_mir_evaluation_scalar(execution, &scalar),
       "evaluation scalar");
    require(scalar == 42, "MIR evaluator result mismatch");
    On1xSDK_Type *evaluation_type = nullptr;
    On1xSDK_IRStringView evaluation_output{}, evaluation_diagnostic{};
    size_t evaluation_steps = 0;
    ok(on1x_sdk_mir_evaluation_type(
           execution, &evaluation_type), "evaluation type");
    ok(on1x_sdk_mir_evaluation_output(
           execution, &evaluation_output), "evaluation output");
    ok(on1x_sdk_mir_evaluation_diagnostic(
           execution, &evaluation_diagnostic), "evaluation diagnostic");
    ok(on1x_sdk_mir_evaluation_steps(
           execution, &evaluation_steps), "evaluation steps");
    require(evaluation_steps && evaluation_output.size == 0 &&
                evaluation_diagnostic.size == 0,
            "successful evaluation metadata mismatch");
    on1x_sdk_type_release(evaluation_type);
    on1x_sdk_mir_evaluation_release(execution);
    On1xSDK_MIREvaluateOptions prefix_evaluate{
        ON1X_SDK_SEMANTICS_IR_ABI_VERSION,
        sizeof(std::uint32_t) * 2u,
        {}, reinterpret_cast<const std::int64_t *>(1),
        999, 0, 0
    };
    execution = nullptr;
    ok(on1x_sdk_mir_evaluate(mir, &prefix_evaluate, &execution),
       "MIR evaluator prefix negotiation");
    ok(on1x_sdk_mir_evaluation_scalar(execution, &scalar),
       "prefix evaluation scalar");
    require(scalar == 42, "prefix evaluator result mismatch");
    on1x_sdk_mir_evaluation_release(execution);
    evaluate.max_steps = 1;
    execution = nullptr;
    ok(on1x_sdk_mir_evaluate(mir, &evaluate, &execution),
       "budgeted MIR evaluation");
    ok(on1x_sdk_mir_evaluation_status(
           execution, &evaluation_status), "budget evaluation status");
    require(evaluation_status ==
                ON1X_SDK_MIR_EVALUATION_BUDGET_EXHAUSTED,
            "evaluator step budget was interpreted as success");
    require(on1x_sdk_mir_evaluation_scalar(execution, &scalar) ==
                ON1X_SDK_UNSUPPORTED,
            "budget-exhausted evaluation exposed a scalar result");
    on1x_sdk_mir_evaluation_release(execution);

    On1xSDK_IRBlob *mir_envelope = nullptr, *mir_canonical = nullptr;
    ok(on1x_sdk_mir_serialize(
           mir, ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1, &mir_envelope),
       "MIR envelope");
    ok(on1x_sdk_mir_serialize(
           mir, ON1X_SDK_IR_WIRE_MIR_CANONICAL, &mir_canonical),
       "canonical MIR");
    const auto envelope_wire = bytes(mir_envelope);
    const auto canonical_wire = bytes(mir_canonical);
    auto mir_decode = decode_options(identity(owner, 9, 2));
    On1xSDK_MIRSnapshot *wire_mir = nullptr;
    ok(on1x_sdk_mir_deserialize(
           envelope_wire.data(), envelope_wire.size(),
           ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1, &mir_decode,
           &wire_mir, &failure, nullptr), "MIR envelope decode");
    On1xSDK_IRIdentity wire_mir_identity{};
    ok(on1x_sdk_mir_snapshot_identity(wire_mir, &wire_mir_identity),
       "wire MIR identity");
    require(wire_mir_identity.state == ON1X_SDK_IR_CANDIDATE,
            "external MIR was promoted");
    execution = reinterpret_cast<On1xSDK_MIREvaluation *>(1);
    require(on1x_sdk_mir_evaluate(
                wire_mir, &evaluate, &execution) ==
                ON1X_SDK_UNSUPPORTED &&
            execution == nullptr,
            "candidate MIR was executed");
    On1xSDK_IRBlob *envelope_again = nullptr;
    ok(on1x_sdk_mir_serialize(
           wire_mir, ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1,
           &envelope_again), "MIR envelope reserialization");
    require(bytes(envelope_again) == envelope_wire,
            "MIR envelope round-trip is not deterministic");

    auto wrong_version_wire = envelope_wire;
    require(wrong_version_wire.size() > 15,
            "MIR envelope header missing");
    std::fill(wrong_version_wire.begin() + 12,
              wrong_version_wire.begin() + 16, 0);
    On1xSDK_MIRSnapshot *wrong_version_mir =
        reinterpret_cast<On1xSDK_MIRSnapshot *>(1);
    require(on1x_sdk_mir_deserialize(
                wrong_version_wire.data(), wrong_version_wire.size(),
                ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1, &mir_decode,
                &wrong_version_mir, &failure, nullptr) == ON1X_SDK_ERROR &&
            wrong_version_mir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_MALFORMED,
            "MIR envelope version mismatch was accepted");

    On1xSDK_MIRSnapshot *canonical_candidate = nullptr;
    ok(on1x_sdk_mir_deserialize(
           canonical_wire.data(), canonical_wire.size(),
           ON1X_SDK_IR_WIRE_MIR_CANONICAL, &mir_decode,
           &canonical_candidate, &failure, nullptr),
       "canonical MIR decode");
    ok(on1x_sdk_semantics_verify_mir(
           canonical_candidate, nullptr, &failure, nullptr),
       "candidate MIR structural verification");
    ok(on1x_sdk_mir_snapshot_identity(
           canonical_candidate, &wire_mir_identity),
       "candidate MIR identity");
    require(wire_mir_identity.state == ON1X_SDK_IR_CANDIDATE,
            "structural verification promoted MIR");
    On1xSDK_IRBlob *canonical_again = nullptr;
    ok(on1x_sdk_mir_serialize(
           canonical_candidate, ON1X_SDK_IR_WIRE_MIR_CANONICAL,
           &canonical_again), "canonical MIR reserialization");
    require(bytes(canonical_again) == canonical_wire,
            "canonical MIR round-trip is not deterministic");
    on1x_sdk_ir_blob_release(canonical_again);

    auto round_trip_id = identity(owner, 9, 7);
    On1xSDK_MIRSnapshot *round_trip_mir = nullptr;
    ok(on1x::sdk::ir::admit_mir(
           canonical_candidate->module, round_trip_id, "round-trip",
           &round_trip_mir, &failure, nullptr),
       "host MIR round-trip admission");
    auto round_trip_evaluate = evaluate;
    round_trip_evaluate.max_steps = 100000;
    execution = nullptr;
    ok(on1x_sdk_mir_evaluate(
           round_trip_mir, &round_trip_evaluate, &execution),
       "round-trip MIR evaluation");
    ok(on1x_sdk_mir_evaluation_scalar(execution, &scalar),
       "round-trip evaluation scalar");
    require(scalar == 42, "round-trip MIR evaluator disagreement");
    on1x_sdk_mir_evaluation_release(execution);
    on1x_sdk_mir_snapshot_release(round_trip_mir);

    auto limited_mir_decode = mir_decode;
    limited_mir_decode.max_work = 1;
    On1xSDK_MIRSnapshot *limited_wire_mir =
        reinterpret_cast<On1xSDK_MIRSnapshot *>(1);
    require(on1x_sdk_mir_deserialize(
                canonical_wire.data(), canonical_wire.size(),
                ON1X_SDK_IR_WIRE_MIR_CANONICAL, &limited_mir_decode,
                &limited_wire_mir, &failure, nullptr) == ON1X_SDK_LIMIT &&
            limited_wire_mir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_LIMIT,
            "MIR decode work budget was not enforced");
    auto depth_mir_decode = mir_decode;
    depth_mir_decode.max_depth = 1;
    static constexpr char nested_mir[] = "((()))";
    limited_wire_mir = reinterpret_cast<On1xSDK_MIRSnapshot *>(1);
    require(on1x_sdk_mir_deserialize(
                nested_mir, sizeof nested_mir - 1,
                ON1X_SDK_IR_WIRE_MIR_CANONICAL, &depth_mir_decode,
                &limited_wire_mir, &failure, nullptr) == ON1X_SDK_LIMIT &&
            limited_wire_mir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_LIMIT,
            "MIR decode depth budget was not enforced");
    std::string malformed_utf8_mir(
        reinterpret_cast<const char *>(canonical_wire.data()),
        canonical_wire.size());
    const auto filename_at = malformed_utf8_mir.find("sdk-test.on1x");
    require(filename_at != std::string::npos,
            "canonical MIR omitted source provenance");
    malformed_utf8_mir.replace(filename_at, 1, "\\xff");
    limited_wire_mir = reinterpret_cast<On1xSDK_MIRSnapshot *>(1);
    require(on1x_sdk_mir_deserialize(
                malformed_utf8_mir.data(), malformed_utf8_mir.size(),
                ON1X_SDK_IR_WIRE_MIR_CANONICAL, &mir_decode,
                &limited_wire_mir, &failure, nullptr) == ON1X_SDK_ERROR &&
            limited_wire_mir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_MALFORMED,
            "non-UTF-8 MIR metadata was accepted");

    On1xSDK_MIROp op{};
    require(on1x_sdk_mir_instruction_op(
                wire_mir, &search.constant, &op) == ON1X_SDK_BUSY,
            "cross-snapshot MIR view was accepted");
    auto stale_mir_decode = mir_decode;
    ++stale_mir_decode.identity.revision;
    On1xSDK_MIRSnapshot *stale_mir =
        reinterpret_cast<On1xSDK_MIRSnapshot *>(1);
    require(on1x_sdk_mir_deserialize(
                envelope_wire.data(), envelope_wire.size(),
                ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1, &stale_mir_decode,
                &stale_mir, &failure, nullptr) == ON1X_SDK_BUSY &&
            stale_mir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_STALE_REVISION,
            "stale MIR envelope revision was accepted");
    auto wrong_generation_mir_decode = mir_decode;
    ++wrong_generation_mir_decode.identity.generation;
    require(on1x_sdk_mir_deserialize(
                envelope_wire.data(), envelope_wire.size(),
                ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1,
                &wrong_generation_mir_decode, &stale_mir,
                &failure, nullptr) == ON1X_SDK_BUSY &&
            stale_mir == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_CROSS_CONTEXT,
            "wrong MIR generation was accepted");
    require(on1x_sdk_mir_deserialize(
                envelope_wire.data(), envelope_wire.size() - 1,
                ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1, &mir_decode,
                &stale_mir, &failure, nullptr) == ON1X_SDK_ERROR &&
            stale_mir == nullptr,
            "truncated MIR envelope was accepted");

    On1xSDK_MIRTransaction *transaction = nullptr;
    ok(on1x_sdk_mir_transaction_begin(
           mir, &mir_identity, &transaction, &failure),
       "transaction begin");
    std::int64_t old_immediate = 0;
    ok(on1x_sdk_mir_instruction_immediate(
           mir, &search.constant, &old_immediate),
       "constant immediate");
    ok(on1x_sdk_mir_transaction_set_immediate(
           transaction, &search.constant, old_immediate + 1),
       "transaction edit");
    static constexpr char edited_name[] = "add.edited";
    ok(on1x_sdk_mir_transaction_rename_function(
           transaction, &first_mir_function,
           {edited_name, sizeof edited_name - 1}),
       "transaction rename");
    ok(on1x_sdk_mir_transaction_verify(
           transaction, nullptr, &failure, nullptr),
       "transaction verify");
    auto stale_identity = mir_identity;
    ++stale_identity.revision;
    On1xSDK_MIRSnapshot *edited =
        reinterpret_cast<On1xSDK_MIRSnapshot *>(1);
    require(on1x_sdk_mir_transaction_commit(
                transaction, &stale_identity, 3, &edited,
                &failure, nullptr) == ON1X_SDK_BUSY &&
            edited == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_STALE_REVISION,
            "stale transaction committed");
    ok(on1x_sdk_mir_transaction_commit(
           transaction, &mir_identity, 3, &edited, &failure, nullptr),
       "transaction commit");
    On1xSDK_IRIdentity edited_identity{};
    ok(on1x_sdk_mir_snapshot_identity(edited, &edited_identity),
       "edited MIR identity");
    require(edited_identity.state == ON1X_SDK_IR_CANDIDATE &&
                edited_identity.revision == 3,
            "transaction claimed verification or wrong revision");
    InstructionSearch edited_search;
    ok(on1x_sdk_mir_visit_instructions(
           edited, nullptr, find_instruction, &edited_search, &failure),
       "edited MIR traversal");
    std::int64_t edited_immediate = 0;
    ok(on1x_sdk_mir_instruction_immediate(
           edited, &edited_search.constant, &edited_immediate),
       "edited immediate");
    require(edited_immediate == old_immediate + 1,
            "transaction did not commit private edit");
    On1xSDK_MIRRef edited_function{};
    On1xSDK_IRStringView committed_name{};
    ok(on1x_sdk_mir_snapshot_function(
           edited, 0, &edited_function), "edited function");
    ok(on1x_sdk_mir_function_name(
           edited, &edited_function, &committed_name),
       "edited function name");
    require(std::string(committed_name.data, committed_name.size) ==
                "add.edited",
            "transaction did not commit function rename");
    require(on1x_sdk_mir_transaction_set_immediate(
                transaction, &search.constant, 0) == ON1X_SDK_BUSY,
            "consumed transaction remained active");
    on1x_sdk_mir_transaction_destroy(transaction);

    evaluate.max_steps = 100000;
    execution = nullptr;
    ok(on1x_sdk_mir_evaluate(mir, &evaluate, &execution),
       "source MIR after transaction");
    ok(on1x_sdk_mir_evaluation_scalar(execution, &scalar),
       "source scalar after transaction");
    require(scalar == 42, "transaction mutated source snapshot");
    on1x_sdk_mir_evaluation_release(execution);

    transaction = nullptr;
    ok(on1x_sdk_mir_transaction_begin(
           mir, &mir_identity, &transaction, &failure),
       "rollback transaction begin");
    ok(on1x_sdk_mir_transaction_set_operand(
           transaction, &search.operand, 0, UINT64_C(999999)),
       "malformed transaction edit");
    require(on1x_sdk_mir_transaction_verify(
                transaction, nullptr, &failure, nullptr) ==
                ON1X_SDK_ERROR &&
            failure == ON1X_SDK_IR_FAILURE_MALFORMED,
            "malformed transaction verified");
    on1x_sdk_mir_transaction_destroy(transaction);

    transaction = nullptr;
    ok(on1x_sdk_mir_transaction_begin(
           mir, &mir_identity, &transaction, &failure),
       "thread-confined transaction begin");
    On1xSDK_Status foreign_thread_status = ON1X_SDK_OK;
    On1xSDK_IRFailure foreign_thread_failure = ON1X_SDK_IR_FAILURE_NONE;
    std::thread foreign_thread([&] {
        foreign_thread_status = on1x_sdk_mir_transaction_verify(
            transaction, nullptr, &foreign_thread_failure, nullptr);
    });
    foreign_thread.join();
    require(foreign_thread_status == ON1X_SDK_BUSY &&
                foreign_thread_failure == ON1X_SDK_IR_FAILURE_LIFECYCLE,
            "transaction escaped its creating thread");
    on1x_sdk_mir_transaction_destroy(transaction);

    auto unsupported_unit =
        source("fn main(){let text=\"hello\";println(text);}",
               "unsupported.on1x");
    auto unsupported_id = identity(owner, 9, 4);
    On1xSDK_HIRSnapshot *unsupported_hir = nullptr;
    ok(on1x::sdk::ir::check_hir_candidate(
           unsupported_unit, unsupported_id, "unsupported",
           &unsupported_hir, &failure, nullptr),
       "unsupported HIR checking");
    On1xSDK_MIRSnapshot *unsupported_mir =
        reinterpret_cast<On1xSDK_MIRSnapshot *>(1);
    diagnostics.entries.clear();
    const auto unsupported_status =
        on1x_sdk_semantics_lower_checked_hir(
            unsupported_hir, 5, nullptr, &unsupported_mir,
            &failure, &sink);
    if (unsupported_status != ON1X_SDK_UNSUPPORTED ||
        unsupported_mir != nullptr ||
        failure != ON1X_SDK_IR_FAILURE_UNSUPPORTED)
        throw std::runtime_error(
            "unsupported HIR semantics were erased during lowering: status " +
            std::to_string(unsupported_status) + ", failure " +
            std::to_string(failure) +
            (diagnostics.entries.empty()
                 ? std::string{}
                 : ", diagnostic " + diagnostics.entries.front()));

    auto canonical_module = on1x::mir::lower(*hir->canonical);
    canonical_module.id = 0;
    On1xSDK_MIRSnapshot *bad_admission =
        reinterpret_cast<On1xSDK_MIRSnapshot *>(1);
    auto admission_id = identity(owner, 9, 6);
    require(on1x::sdk::ir::admit_mir(
                canonical_module, admission_id, "bad-admission",
                &bad_admission, &failure, nullptr) == ON1X_SDK_ERROR &&
            bad_admission == nullptr &&
            failure == ON1X_SDK_IR_FAILURE_MALFORMED,
            "host bridge admitted malformed MIR");

    on1x_sdk_hir_snapshot_release(unsupported_hir);
    on1x_sdk_mir_snapshot_release(edited);
    on1x_sdk_mir_snapshot_release(canonical_candidate);
    on1x_sdk_mir_snapshot_release(wire_mir);
    on1x_sdk_ir_blob_release(envelope_again);
    on1x_sdk_ir_blob_release(mir_canonical);
    on1x_sdk_ir_blob_release(mir_envelope);
    on1x_sdk_mir_snapshot_release(mir);
    on1x_sdk_ir_blob_release(hir_blob_again);
    on1x_sdk_hir_snapshot_release(wire_hir);
    on1x_sdk_ir_blob_release(hir_blob);
    on1x_sdk_hir_snapshot_release(hir);
}

} // namespace

int main() {
    try {
        static int owner_token, other_token;
        const auto *owner =
            reinterpret_cast<const On1xSDK_Context *>(&owner_token);
        const auto *other =
            reinterpret_cast<const On1xSDK_Context *>(&other_token);
        test_types(owner, other);
        test_hir_and_mir(owner);
        std::cout << "SDK type/HIR/MIR semantics PASS\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
