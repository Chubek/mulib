#include "IRInternal.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace on1x::sdk::ir {
namespace {

constexpr std::array<std::uint8_t, 8> hir_magic{
    'O', '1', 'X', 'H', 'I', 'R', '1', 0
};
constexpr std::array<std::uint8_t, 8> mir_magic{
    'O', '1', 'X', 'M', 'I', 'R', '1', 0
};
constexpr std::size_t max_type_nodes = 16384;

struct WireError : std::runtime_error {
    On1xSDK_Status status;
    On1xSDK_IRFailure failure;
    WireError(On1xSDK_Status status_value, On1xSDK_IRFailure failure_value,
              std::string message)
        : std::runtime_error(std::move(message)),
          status(status_value), failure(failure_value) {}
};

class Writer {
public:
    explicit Writer(std::size_t maximum = ON1X_SDK_IR_MAX_WIRE_BYTES)
        : maximum_(maximum) {
        bytes_.reserve(std::min<std::size_t>(maximum, 4096));
    }

    void raw(const void *data, std::size_t size) {
        if (size > maximum_ - bytes_.size())
            throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                            "wire byte limit exhausted");
        const auto *first = static_cast<const std::uint8_t *>(data);
        bytes_.insert(bytes_.end(), first, first + size);
    }

    void u32(std::uint32_t value) {
        std::uint8_t bytes[4];
        for (unsigned i = 0; i < 4; ++i)
            bytes[i] = static_cast<std::uint8_t>(value >> (i * 8));
        raw(bytes, sizeof bytes);
    }

    void u64(std::uint64_t value) {
        std::uint8_t bytes[8];
        for (unsigned i = 0; i < 8; ++i)
            bytes[i] = static_cast<std::uint8_t>(value >> (i * 8));
        raw(bytes, sizeof bytes);
    }

    void text(std::string_view value) {
        if (value.size() > ON1X_SDK_IR_MAX_STRING_BYTES ||
            value.size() > std::numeric_limits<std::uint32_t>::max())
            throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                            "wire string limit exhausted");
        if (!valid_utf8(value) ||
            value.find('\0') != std::string_view::npos)
            throw WireError(ON1X_SDK_ERROR, ON1X_SDK_IR_FAILURE_MALFORMED,
                            "wire string is not canonical UTF-8");
        u32(static_cast<std::uint32_t>(value.size()));
        if (!value.empty()) raw(value.data(), value.size());
    }

    std::vector<std::uint8_t> take() { return std::move(bytes_); }

private:
    std::size_t maximum_;
    std::vector<std::uint8_t> bytes_;
};

class Reader {
public:
    Reader(const void *bytes, std::size_t size, std::size_t maximum,
           std::size_t work)
        : bytes_(static_cast<const std::uint8_t *>(bytes)),
          size_(size), work_(work) {
        if ((size && !bytes) || size > maximum)
            throw WireError(size > maximum ? ON1X_SDK_LIMIT
                                           : ON1X_SDK_INVALID_ARGUMENT,
                            size > maximum ? ON1X_SDK_IR_FAILURE_LIMIT
                                           : ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION,
                            "invalid or oversized wire input");
    }

    void tick(std::size_t amount = 1) {
        if (amount > work_)
            throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                            "wire work budget exhausted");
        work_ -= amount;
    }

    void expect(const std::array<std::uint8_t, 8> &magic) {
        const auto *value = raw(magic.size());
        if (!std::equal(magic.begin(), magic.end(), value))
            malformed("wire magic mismatch");
    }

    const std::uint8_t *raw(std::size_t amount) {
        tick();
        if (amount > size_ - offset_) malformed("truncated wire input");
        const auto *result = bytes_ + offset_;
        offset_ += amount;
        return result;
    }

    std::uint32_t u32() {
        const auto *bytes = raw(4);
        std::uint32_t result = 0;
        for (unsigned i = 0; i < 4; ++i)
            result |= static_cast<std::uint32_t>(bytes[i]) << (i * 8);
        return result;
    }

    std::uint64_t u64() {
        const auto *bytes = raw(8);
        std::uint64_t result = 0;
        for (unsigned i = 0; i < 8; ++i)
            result |= static_cast<std::uint64_t>(bytes[i]) << (i * 8);
        return result;
    }

    std::string text(std::size_t maximum = ON1X_SDK_IR_MAX_STRING_BYTES) {
        const auto size = static_cast<std::size_t>(u32());
        if (size > maximum)
            throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                            "wire string limit exhausted");
        const auto *bytes = raw(size);
        std::string result(reinterpret_cast<const char *>(bytes), size);
        if (!valid_utf8(result) ||
            result.find('\0') != std::string::npos)
            malformed("wire string is not canonical UTF-8");
        return result;
    }

    bool done() const noexcept { return offset_ == size_; }

    [[noreturn]] void malformed(const char *message) const {
        throw WireError(ON1X_SDK_ERROR, ON1X_SDK_IR_FAILURE_MALFORMED,
                        message);
    }

private:
    const std::uint8_t *bytes_;
    std::size_t size_;
    std::size_t offset_ = 0;
    std::size_t work_;
};

void write_type(Writer &writer, const Type &type,
                 std::size_t depth, std::size_t &nodes) {
    if (depth > ON1X_SDK_IR_MAX_DEPTH || ++nodes > max_type_nodes)
        throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                        "type wire traversal limit exhausted");
    On1xSDK_TypeKind encoded_kind = ON1X_SDK_TYPE_INVALID;
    if (!sdk_type_kind(type.kind(), encoded_kind))
        throw WireError(ON1X_SDK_UNSUPPORTED,
                        ON1X_SDK_IR_FAILURE_UNSUPPORTED,
                        "canonical type has no SDK wire kind");
    writer.u32(static_cast<std::uint32_t>(encoded_kind));
    writer.u64((type.kind() == Type::Generic ||
                type.kind() == Type::Bound || type.is_forall())
                   ? type.generic_identity()
                   : type.kind() == Type::Channel
                       ? type.sender_cardinality() : 0);
    writer.u64(type.is_data() ? type.extent()
               : type.kind() == Type::Channel
                   ? type.receiver_cardinality() : 0);
    writer.text(type.nominal());
    writer.u32(static_cast<std::uint32_t>(type.arguments().size()));
    for (const auto &argument : type.arguments())
        write_type(writer, argument, depth + 1, nodes);
    if (type.is_data()) {
        writer.u32(static_cast<std::uint32_t>(type.labels().size()));
        for (const auto &label : type.labels()) writer.text(label);
    } else {
        writer.u32(0);
    }
    if (type.is_function()) {
        writer.u32(static_cast<std::uint32_t>(type.parameters().size()));
        for (const auto &parameter : type.parameters())
            write_type(writer, parameter, depth + 1, nodes);
        write_type(writer, type.result(), depth + 1, nodes);
    } else if (type.is_tuple() || type.is_data()) {
        writer.u32(static_cast<std::uint32_t>(type.elements().size()));
        for (const auto &element : type.elements())
            write_type(writer, element, depth + 1, nodes);
    } else if (type.is_forall()) {
        writer.u32(1);
        write_type(writer, type.body(), depth + 1, nodes);
    } else if (type.is_code()) {
        writer.u32(1);
        write_type(writer, type.code_result(), depth + 1, nodes);
    } else if (type.is_own()) {
        writer.u32(1);
        write_type(writer, type.owned(), depth + 1, nodes);
    } else if (type.kind() == Type::Channel ||
               type.kind() == Type::Sender ||
               type.kind() == Type::Receiver) {
        writer.u32(1);
        write_type(writer, type.csp_payload(), depth + 1, nodes);
    } else {
        writer.u32(0);
    }
}

Type read_type(Reader &reader, std::size_t depth, std::size_t &nodes,
               std::size_t maximum_depth, std::size_t maximum_nodes) {
    if (depth > maximum_depth ||
        ++nodes > std::min(max_type_nodes, maximum_nodes))
        throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                         "type wire traversal limit exhausted");
    const auto kind_value = reader.u32();
    if (kind_value > ON1X_SDK_TYPE_OWN)
        reader.malformed("unknown canonical type kind");
    Type::Kind kind = Type::Invalid;
    if (!canonical_type_kind(
            static_cast<On1xSDK_TypeKind>(kind_value), kind))
        reader.malformed("unsupported legacy canonical type kind");
    const auto variable = reader.u64();
    const auto extent = reader.u64();
    if (variable > std::numeric_limits<std::size_t>::max() ||
        extent > std::numeric_limits<std::size_t>::max())
        throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                        "canonical type scalar exceeds host size limit");
    auto nominal = reader.text(4096);
    const auto argument_count = reader.u32();
    if (argument_count > max_type_nodes)
        throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                        "type argument limit exhausted");
    std::vector<Type> arguments;
    arguments.reserve(argument_count);
    for (std::size_t i = 0; i < argument_count; ++i)
        arguments.push_back(read_type(
            reader, depth + 1, nodes, maximum_depth, maximum_nodes));
    const auto label_count = reader.u32();
    if (label_count > 256)
        reader.malformed("type label count exceeds canonical limit");
    std::vector<std::string> labels;
    labels.reserve(label_count);
    for (std::size_t i = 0; i < label_count; ++i)
        labels.push_back(reader.text(4096));
    const auto child_count = reader.u32();
    if (child_count > max_type_nodes)
        throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                        "type child limit exhausted");

    auto no_metadata = [&] {
        if (variable || extent || !nominal.empty() || !arguments.empty() ||
            !labels.empty() || child_count)
            reader.malformed("unexpected metadata on atomic type");
    };
    try {
        switch (kind) {
        case Type::Function: {
            if (variable || extent || !nominal.empty() ||
                !arguments.empty() || !labels.empty())
                reader.malformed("malformed function type metadata");
            std::vector<Type> parameters;
            parameters.reserve(child_count);
            for (std::size_t i = 0; i < child_count; ++i)
                parameters.push_back(read_type(
                    reader, depth + 1, nodes,
                    maximum_depth, maximum_nodes));
            auto result = read_type(
                reader, depth + 1, nodes, maximum_depth, maximum_nodes);
            return Type::function(std::move(parameters), std::move(result));
        }
        case Type::Tuple: {
            if (variable || extent || !nominal.empty() ||
                !arguments.empty() || !labels.empty())
                reader.malformed("malformed tuple type metadata");
            std::vector<Type> elements;
            elements.reserve(child_count);
            for (std::size_t i = 0; i < child_count; ++i)
                elements.push_back(read_type(
                    reader, depth + 1, nodes,
                    maximum_depth, maximum_nodes));
            return Type::tuple(std::move(elements));
        }
        case Type::Record:
        case Type::Sum:
        case Type::Array:
        case Type::Slice: {
            if (variable ||
                ((kind == Type::Record || kind == Type::Sum) && extent) ||
                ((kind == Type::Array || kind == Type::Slice) &&
                 (!nominal.empty() || !arguments.empty())))
                reader.malformed("malformed data type metadata");
            std::vector<Type> elements;
            elements.reserve(child_count);
            for (std::size_t i = 0; i < child_count; ++i)
                elements.push_back(read_type(
                    reader, depth + 1, nodes,
                    maximum_depth, maximum_nodes));
            auto result = Type::data(kind, std::move(elements),
                                     std::move(labels), extent);
            if (!nominal.empty())
                result = result.with_nominal(std::move(nominal),
                                             std::move(arguments));
            else if (!arguments.empty())
                reader.malformed("anonymous data type has arguments");
            return result;
        }
        case Type::Named:
            if (variable || extent || nominal.empty() || !labels.empty() ||
                child_count)
                reader.malformed("malformed named type");
            return Type::named(std::move(nominal), std::move(arguments));
        case Type::Forall:
            if (!variable || extent || !nominal.empty() ||
                !arguments.empty() || !labels.empty() || child_count != 1)
                reader.malformed("malformed forall type");
            return Type::for_all(variable,
                                 read_type(reader, depth + 1, nodes,
                                           maximum_depth, maximum_nodes));
        case Type::Code:
            if (variable || extent || !nominal.empty() ||
                !arguments.empty() || !labels.empty() || child_count != 1)
                reader.malformed("malformed code type");
            return Type::code(read_type(
                reader, depth + 1, nodes,
                maximum_depth, maximum_nodes));
        case Type::Own:
            if (variable || extent || !nominal.empty() ||
                !arguments.empty() || !labels.empty() || child_count != 1)
                reader.malformed("malformed owning type");
            return Type::own(read_type(
                reader, depth + 1, nodes,
                maximum_depth, maximum_nodes));
        case Type::Channel: {
            if (!nominal.empty() || !arguments.empty() || !labels.empty() ||
                child_count != 1)
                reader.malformed("malformed channel type");
            auto payload = read_type(
                reader, depth + 1, nodes,
                maximum_depth, maximum_nodes);
            return Type::channel(
                std::move(payload),
                static_cast<std::size_t>(variable),
                static_cast<std::size_t>(extent));
        }
        case Type::Sender:
        case Type::Receiver: {
            if (variable || extent || !nominal.empty() ||
                !arguments.empty() || !labels.empty() || child_count != 1)
                reader.malformed("malformed channel endpoint type");
            auto payload = read_type(
                reader, depth + 1, nodes,
                maximum_depth, maximum_nodes);
            return kind == Type::Sender
                ? Type::sender(std::move(payload))
                : Type::receiver(std::move(payload));
        }
        case Type::Generic:
            if (extent || !nominal.empty() || !arguments.empty() ||
                !labels.empty() || child_count)
                reader.malformed("malformed generic type");
            return Type::generic(variable);
        case Type::Bound:
            if (extent || !nominal.empty() || !arguments.empty() ||
                !labels.empty() || child_count)
                reader.malformed("malformed bound type");
            return Type::bound(variable);
        default:
            no_metadata();
            return Type(kind);
        }
    } catch (const WireError &) {
        throw;
    } catch (const std::length_error &) {
        throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                        "canonical type construction limit");
    } catch (const std::logic_error &) {
        reader.malformed("canonical type construction rejected wire shape");
    }
}

struct Decode {
    Identity identity;
    std::string filename;
    std::size_t max_bytes = ON1X_SDK_IR_MAX_WIRE_BYTES;
    std::size_t max_nodes = ON1X_SDK_IR_MAX_NODES;
    std::size_t max_work = ON1X_SDK_IR_MAX_NODES * 16u;
    std::size_t max_depth = ON1X_SDK_IR_MAX_DEPTH;
};

On1xSDK_Status decode_options(const On1xSDK_IRDecodeOptions *source,
                               Decode &out) noexcept {
    if (!source) return ON1X_SDK_INVALID_ARGUMENT;
    if (const auto status =
            copy_text({"<sdk-wire>", 10}, 4096, false, out.filename);
        status != ON1X_SDK_OK)
        return status;
    struct Prefix { std::uint32_t abi_version, struct_size; } prefix{};
    std::memcpy(&prefix, source, sizeof prefix);
    constexpr auto required =
        offsetof(On1xSDK_IRDecodeOptions, identity) +
        sizeof(On1xSDK_IRDecodeOptions::identity);
    if (prefix.abi_version != ON1X_SDK_SEMANTICS_IR_ABI_VERSION ||
        prefix.struct_size < required)
        return ON1X_SDK_ABI_MISMATCH;
    On1xSDK_IRDecodeOptions copy{};
    std::memcpy(&copy, source,
                std::min<std::size_t>(prefix.struct_size, sizeof copy));
    if (const auto status = read_identity(
            &copy.identity, ON1X_SDK_IR_CANDIDATE, out.identity);
        status != ON1X_SDK_OK)
        return status;
#define ON1X_DECODE_HAS(field) \
    (prefix.struct_size >= offsetof(On1xSDK_IRDecodeOptions, field) + \
                           sizeof copy.field)
    if (ON1X_DECODE_HAS(filename) && copy.filename.size) {
        if (const auto status = copy_text(
                copy.filename, 4096, false, out.filename);
            status != ON1X_SDK_OK)
            return status;
    }
    if (ON1X_DECODE_HAS(max_bytes) && copy.max_bytes)
        out.max_bytes = copy.max_bytes;
    if (ON1X_DECODE_HAS(max_nodes) && copy.max_nodes)
        out.max_nodes = copy.max_nodes;
    if (ON1X_DECODE_HAS(max_work) && copy.max_work)
        out.max_work = copy.max_work;
    if (ON1X_DECODE_HAS(max_depth) && copy.max_depth)
        out.max_depth = copy.max_depth;
#undef ON1X_DECODE_HAS
    if (!out.max_bytes || out.max_bytes > ON1X_SDK_IR_MAX_WIRE_BYTES ||
        !out.max_nodes || out.max_nodes > ON1X_SDK_IR_MAX_NODES ||
        !out.max_work || !out.max_depth ||
        out.max_depth > ON1X_SDK_IR_MAX_DEPTH)
        return ON1X_SDK_INVALID_ARGUMENT;
    return ON1X_SDK_OK;
}

void write_location(Writer &writer, const Location &where) {
    writer.text(where.file);
    writer.u64(where.offset);
    writer.u64(where.line);
    writer.u64(where.column);
}

Location read_location(Reader &reader) {
    Location result;
    result.file = reader.text();
    const auto offset = reader.u64();
    const auto line = reader.u64();
    const auto column = reader.u64();
    if (offset > std::numeric_limits<std::size_t>::max() ||
        line > std::numeric_limits<std::size_t>::max() ||
        column > std::numeric_limits<std::size_t>::max())
        throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                        "source point exceeds host size limit");
    result.offset = static_cast<std::size_t>(offset);
    result.line = static_cast<std::size_t>(line);
    result.column = static_cast<std::size_t>(column);
    if (!result.line || !result.column)
        reader.malformed("invalid source point in HIR wire");
    return result;
}

void report_wire_error(const WireError &error,
                       On1xSDK_IRFailure *failure,
                       const On1xSDK_DiagnosticSink *diagnostics,
                       const char *code) noexcept {
    set_failure(failure, error.failure);
    emit_diagnostic(diagnostics, code, error.what());
}

void preflight_mir(std::string_view text, std::size_t maximum_work,
                   std::size_t maximum_depth) {
    std::size_t position = 0;
    std::size_t depth = 0;
    std::size_t nodes = 0;
    auto tick = [&] {
        if (++nodes > maximum_work)
            throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                            "MIR decode work budget exhausted");
        if (depth > maximum_depth)
            throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                            "MIR decode depth budget exhausted");
    };
    while (position < text.size()) {
        const auto byte = static_cast<unsigned char>(text[position]);
        if (byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n') {
            ++position;
            continue;
        }
        if (byte == ')') {
            if (!depth) {
                throw WireError(ON1X_SDK_ERROR,
                                ON1X_SDK_IR_FAILURE_MALFORMED,
                                "unexpected ')' in MIR payload");
            }
            --depth;
            ++position;
            continue;
        }
        tick();
        if (byte == '(') {
            ++depth;
            if (depth > maximum_depth)
                throw WireError(ON1X_SDK_LIMIT,
                                ON1X_SDK_IR_FAILURE_LIMIT,
                                "MIR decode depth budget exhausted");
            ++position;
            continue;
        }
        if (byte == '"') {
            ++position;
            bool closed = false;
            while (position < text.size()) {
                if (text[position] == '\\' && position + 1 < text.size()) {
                    position += 2;
                    continue;
                }
                if (text[position++] == '"') {
                    closed = true;
                    break;
                }
            }
            if (!closed)
                throw WireError(ON1X_SDK_ERROR,
                                ON1X_SDK_IR_FAILURE_MALFORMED,
                                "unterminated string in MIR payload");
            continue;
        }
        do {
            ++position;
        } while (position < text.size() &&
                 text[position] != '(' && text[position] != ')' &&
                 text[position] != ' ' && text[position] != '\t' &&
                  text[position] != '\r' && text[position] != '\n');
    }
    if (depth)
        throw WireError(ON1X_SDK_ERROR,
                        ON1X_SDK_IR_FAILURE_MALFORMED,
                        "unterminated list in MIR payload");
}

} // namespace
} // namespace on1x::sdk::ir

namespace mir = on1x::mir;
using on1x::Error;

extern "C" {

void on1x_sdk_ir_blob_retain(On1xSDK_IRBlob *blob) {
    if (!blob) return;
    auto current = blob->references.load(std::memory_order_relaxed);
    while (current != std::numeric_limits<std::size_t>::max() &&
           !blob->references.compare_exchange_weak(
               current, current + 1, std::memory_order_relaxed)) {}
}

void on1x_sdk_ir_blob_release(On1xSDK_IRBlob *blob) {
    if (blob && blob->references.fetch_sub(
                    1, std::memory_order_acq_rel) == 1)
        delete blob;
}

On1xSDK_Status on1x_sdk_ir_blob_bytes(
    const On1xSDK_IRBlob *blob,
    On1xSDK_IRBytesView *out_bytes) {
    if (!blob || !out_bytes) return ON1X_SDK_INVALID_ARGUMENT;
    *out_bytes = {blob->bytes.data(), blob->bytes.size()};
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_hir_serialize(
    const On1xSDK_HIRSnapshot *snapshot,
    On1xSDK_IRBlob **out_blob) {
    using namespace on1x::sdk::ir;
    if (!out_blob) return ON1X_SDK_INVALID_ARGUMENT;
    *out_blob = nullptr;
    if (!snapshot) return ON1X_SDK_INVALID_ARGUMENT;
    if (!snapshot->wire_complete) return ON1X_SDK_UNSUPPORTED;
    On1xSDK_IRFailure failure = ON1X_SDK_IR_FAILURE_NONE;
    if (const auto status =
            on1x_sdk_hir_verify_structure(snapshot, nullptr, &failure, nullptr);
        status != ON1X_SDK_OK)
        return status;
    try {
        Writer writer;
        writer.raw(hir_magic.data(), hir_magic.size());
        writer.u32(ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION);
        writer.u32(0);
        writer.u64(snapshot->identity.generation);
        writer.u64(snapshot->identity.revision);
        writer.text(snapshot->provenance);
        writer.u64(snapshot->nodes.size());
        writer.u64(snapshot->functions.size());
        for (const auto id : snapshot->functions) writer.u64(id);
        std::size_t type_nodes = 0;
        for (const auto &node : snapshot->nodes) {
            writer.u64(node.id);
            writer.u32(static_cast<std::uint32_t>(node.node_class));
            writer.u32(node.kind);
            writer.u64(node.flags);
            writer.text(node.name);
            write_location(writer, node.where);
            write_type(writer, node.type, 0, type_nodes);
            writer.u32(static_cast<std::uint32_t>(node.labels.size()));
            for (const auto &label : node.labels) writer.text(label);
            for (const auto value : node.auxiliary) writer.u64(value);
            writer.u32(static_cast<std::uint32_t>(node.children.size()));
            for (const auto &edge : node.children) {
                writer.u32(static_cast<std::uint32_t>(edge.kind));
                writer.u64(edge.target);
            }
        }
        auto blob = std::make_unique<On1xSDK_IRBlob>();
        blob->bytes = writer.take();
        *out_blob = blob.release();
        return ON1X_SDK_OK;
    } catch (const WireError &error) {
        return error.status;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_sdk_hir_deserialize(
    const void *bytes,
    size_t size,
    const On1xSDK_IRDecodeOptions *options,
    On1xSDK_HIRSnapshot **out_snapshot,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics) {
    using namespace on1x::sdk::ir;
    if (!out_snapshot) return ON1X_SDK_INVALID_ARGUMENT;
    *out_snapshot = nullptr;
    set_failure(out_failure, ON1X_SDK_IR_FAILURE_NONE);
    Decode decode;
    if (const auto status = decode_options(options, decode);
        status != ON1X_SDK_OK) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return status;
    }
    try {
        Reader reader(bytes, size, decode.max_bytes, decode.max_work);
        reader.expect(hir_magic);
        if (reader.u32() != ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION ||
            reader.u32() != 0)
            reader.malformed("unsupported HIR wire schema");
        const auto generation = reader.u64();
        const auto revision = reader.u64();
        if (generation != decode.identity.generation) {
            set_failure(out_failure, ON1X_SDK_IR_FAILURE_CROSS_CONTEXT);
            return ON1X_SDK_BUSY;
        }
        if (revision != decode.identity.revision) {
            set_failure(out_failure, ON1X_SDK_IR_FAILURE_STALE_REVISION);
            return ON1X_SDK_BUSY;
        }
        auto provenance = reader.text(ON1X_SDK_IR_MAX_PROVENANCE_BYTES);
        const auto node_count = reader.u64();
        const auto function_count = reader.u64();
        if (node_count > decode.max_nodes ||
            function_count > node_count)
            throw WireError(ON1X_SDK_LIMIT, ON1X_SDK_IR_FAILURE_LIMIT,
                            "HIR wire node limit exhausted");
        std::vector<std::uint64_t> functions;
        functions.reserve(static_cast<std::size_t>(function_count));
        for (std::uint64_t i = 0; i < function_count; ++i)
            functions.push_back(reader.u64());
        std::vector<HIRNode> nodes;
        nodes.reserve(static_cast<std::size_t>(node_count));
        std::size_t type_nodes = 0;
        for (std::uint64_t i = 0; i < node_count; ++i) {
            HIRNode node;
            node.id = reader.u64();
            const auto node_class = reader.u32();
            if (node_class < ON1X_SDK_HIR_NODE_FUNCTION ||
                node_class > ON1X_SDK_HIR_NODE_CAPTURE)
                reader.malformed("unknown HIR node class");
            node.node_class =
                static_cast<On1xSDK_HIRNodeClass>(node_class);
            node.kind = reader.u32();
            node.flags = reader.u64();
            node.name = reader.text();
            node.where = read_location(reader);
            node.type = read_type(
                reader, 0, type_nodes, decode.max_depth, decode.max_nodes);
            const auto label_count = reader.u32();
            if (label_count > 256)
                reader.malformed("HIR label limit exceeded");
            node.labels.reserve(label_count);
            for (std::size_t label = 0; label < label_count; ++label)
                node.labels.push_back(reader.text());
            for (auto &value : node.auxiliary) value = reader.u64();
            const auto child_count = reader.u32();
            if (child_count > decode.max_nodes)
                throw WireError(ON1X_SDK_LIMIT,
                                ON1X_SDK_IR_FAILURE_LIMIT,
                                "HIR edge limit exhausted");
            node.children.reserve(child_count);
            for (std::size_t child = 0; child < child_count; ++child) {
                const auto edge = reader.u32();
                if (edge < ON1X_SDK_HIR_EDGE_PARAMETER ||
                    edge > ON1X_SDK_HIR_EDGE_CAPTURE)
                    reader.malformed("unknown HIR edge kind");
                node.children.push_back({
                    static_cast<On1xSDK_HIREdgeKind>(edge),
                    reader.u64()
                });
            }
            nodes.push_back(std::move(node));
        }
        if (!reader.done()) reader.malformed("trailing HIR wire bytes");
        auto *snapshot = make_hir_snapshot(
            decode.identity, std::move(provenance), std::move(nodes),
            std::move(functions), true, {});
        const On1xSDK_IRLimits limits{
            ON1X_SDK_SEMANTICS_IR_ABI_VERSION, sizeof(On1xSDK_IRLimits),
            decode.max_nodes, decode.max_work, decode.max_depth
        };
        const auto status = on1x_sdk_hir_verify_structure(
            snapshot, &limits, out_failure, diagnostics);
        if (status != ON1X_SDK_OK) {
            on1x_sdk_hir_snapshot_release(snapshot);
            return status;
        }
        *out_snapshot = snapshot;
        return ON1X_SDK_OK;
    } catch (const WireError &error) {
        report_wire_error(error, out_failure, diagnostics,
                          "ON1X-SDK-HIR-DECODE");
        return error.status;
    } catch (const std::bad_alloc &) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
        return ON1X_SDK_LIMIT;
    } catch (...) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_MALFORMED);
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_sdk_mir_serialize(
    const On1xSDK_MIRSnapshot *snapshot,
    On1xSDK_IRWireFormat format,
    On1xSDK_IRBlob **out_blob) {
    using namespace on1x::sdk::ir;
    if (!out_blob) return ON1X_SDK_INVALID_ARGUMENT;
    *out_blob = nullptr;
    if (!snapshot) return ON1X_SDK_INVALID_ARGUMENT;
    if (format != ON1X_SDK_IR_WIRE_MIR_CANONICAL &&
        format != ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1)
        return ON1X_SDK_UNSUPPORTED;
    try {
        const auto canonical = mir::serialize(snapshot->module);
        if (canonical.size() > ON1X_SDK_IR_MAX_WIRE_BYTES)
            return ON1X_SDK_LIMIT;
        auto blob = std::make_unique<On1xSDK_IRBlob>();
        if (format == ON1X_SDK_IR_WIRE_MIR_CANONICAL) {
            blob->bytes.assign(canonical.begin(), canonical.end());
        } else {
            Writer writer;
            writer.raw(mir_magic.data(), mir_magic.size());
            writer.u32(ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION);
            writer.u32(snapshot->module.version);
            writer.u64(snapshot->identity.generation);
            writer.u64(snapshot->identity.revision);
            writer.text(snapshot->provenance);
            writer.u64(canonical.size());
            writer.raw(canonical.data(), canonical.size());
            blob->bytes = writer.take();
        }
        *out_blob = blob.release();
        return ON1X_SDK_OK;
    } catch (const WireError &error) {
        return error.status;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (const Error &error) {
        return error_status(error);
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

On1xSDK_Status on1x_sdk_mir_deserialize(
    const void *bytes,
    size_t size,
    On1xSDK_IRWireFormat format,
    const On1xSDK_IRDecodeOptions *options,
    On1xSDK_MIRSnapshot **out_snapshot,
    On1xSDK_IRFailure *out_failure,
    const On1xSDK_DiagnosticSink *diagnostics) {
    using namespace on1x::sdk::ir;
    if (!out_snapshot) return ON1X_SDK_INVALID_ARGUMENT;
    *out_snapshot = nullptr;
    set_failure(out_failure, ON1X_SDK_IR_FAILURE_NONE);
    if (format != ON1X_SDK_IR_WIRE_MIR_CANONICAL &&
        format != ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_UNSUPPORTED);
        return ON1X_SDK_UNSUPPORTED;
    }
    Decode decode;
    if (const auto status = decode_options(options, decode);
        status != ON1X_SDK_OK) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION);
        return status;
    }
    try {
        if ((size && !bytes) || size > decode.max_bytes)
            throw WireError(size > decode.max_bytes
                                ? ON1X_SDK_LIMIT
                                : ON1X_SDK_INVALID_ARGUMENT,
                            size > decode.max_bytes
                                ? ON1X_SDK_IR_FAILURE_LIMIT
                                : ON1X_SDK_IR_FAILURE_INVALID_CONFIGURATION,
                            "invalid or oversized MIR wire");
        std::string_view canonical(
            bytes ? static_cast<const char *>(bytes) : "", size);
        std::string provenance = decode.filename;
        std::uint32_t claimed_version = 0;
        if (format == ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1) {
            Reader reader(bytes, size, decode.max_bytes, decode.max_work);
            reader.expect(mir_magic);
            if (reader.u32() != ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION)
                reader.malformed("unsupported MIR envelope schema");
            claimed_version = reader.u32();
            const auto generation = reader.u64();
            const auto revision = reader.u64();
            if (generation != decode.identity.generation) {
                set_failure(out_failure,
                            ON1X_SDK_IR_FAILURE_CROSS_CONTEXT);
                return ON1X_SDK_BUSY;
            }
            if (revision != decode.identity.revision) {
                set_failure(out_failure,
                            ON1X_SDK_IR_FAILURE_STALE_REVISION);
                return ON1X_SDK_BUSY;
            }
            provenance =
                reader.text(ON1X_SDK_IR_MAX_PROVENANCE_BYTES);
            const auto payload_size = reader.u64();
            if (payload_size > decode.max_bytes)
                throw WireError(ON1X_SDK_LIMIT,
                                ON1X_SDK_IR_FAILURE_LIMIT,
                                "MIR payload limit exhausted");
            const auto *payload =
                reader.raw(static_cast<std::size_t>(payload_size));
            if (!reader.done())
                reader.malformed("trailing MIR envelope bytes");
            canonical = {
                reinterpret_cast<const char *>(payload),
                static_cast<std::size_t>(payload_size)
            };
        }
        preflight_mir(canonical, decode.max_work, decode.max_depth);
        auto module = mir::deserialize(canonical, decode.filename);
        if (format == ON1X_SDK_IR_WIRE_MIR_ENVELOPE_V1 &&
            module.version != claimed_version)
            throw WireError(ON1X_SDK_ERROR,
                            ON1X_SDK_IR_FAILURE_MALFORMED,
                            "MIR envelope version/payload mismatch");
        const On1xSDK_IRLimits limits{
            ON1X_SDK_SEMANTICS_IR_ABI_VERSION, sizeof(On1xSDK_IRLimits),
            decode.max_nodes, decode.max_work, decode.max_depth
        };
        if (const auto status =
                verify_module(module, &limits, out_failure, diagnostics);
            status != ON1X_SDK_OK)
            return status;
        *out_snapshot = make_mir_snapshot(
            decode.identity, std::move(provenance), std::move(module));
        return ON1X_SDK_OK;
    } catch (const WireError &error) {
        report_wire_error(error, out_failure, diagnostics,
                          "ON1X-SDK-MIR-DECODE");
        return error.status;
    } catch (const Error &error) {
        const auto status = error_status(error);
        set_failure(out_failure, status == ON1X_SDK_LIMIT
                             ? ON1X_SDK_IR_FAILURE_LIMIT
                             : ON1X_SDK_IR_FAILURE_MALFORMED);
        emit_diagnostic(diagnostics, "ON1X-SDK-MIR-DECODE",
                        error.what(), &error.where);
        return status;
    } catch (const std::bad_alloc &) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_LIMIT);
        return ON1X_SDK_LIMIT;
    } catch (...) {
        set_failure(out_failure, ON1X_SDK_IR_FAILURE_MALFORMED);
        return ON1X_SDK_ERROR;
    }
}

} // extern "C"
