#include "Bridge.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace on1x::backends::archon {
namespace {
using V = ArchonValue;
using O = mir::Op;
constexpr V none = ARCHON_MIR_NONE;
constexpr std::size_t max_tuple_words = 4096; // bounded type expansion and frame budget
constexpr std::size_t max_frame_local_bytes = 32768;

[[noreturn]] void fail(const Location &where, std::string message,
                      ArchonBackendStatus status = ARCHON_BACKEND_INVALID) {
    throw Error(where, "ON1X-MIR-ARCHON: " + std::move(message), status);
}
void poll(const Options &options, const Location &at) {
    if (options.cancelled && options.cancelled(options.cancel_userdata))
        fail(at, "compilation cancelled", ARCHON_BACKEND_CANCELLED);
}
std::string source_symbol(mir::Id id) { return "on1x_mir_f" + std::to_string(id); }

std::size_t words(const Type &type, const Location &at, unsigned depth = 0) {
    if (depth > 32) fail(at, "tuple nesting exceeds 32", ARCHON_BACKEND_LIMIT);
    if (!type.is_tuple()) {
        if (type == Type::I64 || type == Type::Bool || type.is_numeric()) return 1;
        fail(at, "unrepresentable MachineIR type: " + type_name(type));
    }
    std::size_t count = 0;
    for (const auto &child : type.elements()) {
        auto n = words(child, at, depth + 1);
        if (n > max_tuple_words - count)
            fail(at, "tuple exceeds flat-word limit", ARCHON_BACKEND_LIMIT);
        count += n;
    }
    // The canonical Type API currently requires at least two tuple elements.
    return count;
}
ArchonMIRType abi_type(const Type &t, const Location &at) {
    (void)words(t, at);
    return t.is_tuple() ? ARCHON_MIR_PTR : ARCHON_MIR_I64;
}
bool contains_float(const Type &t) {
    if (t == Type::F64 || t == Type::F32) return true;
    if (t.is_tuple()) for (const auto &child : t.elements())
        if (contains_float(child)) return true;
    return false;
}
std::string runtime_type(const Type &t) {
    if (t == Type::I64) return "i64";
    if (t == Type::Bool) return "bool";
    if (t == Type::F64) return "f64";
    if (t == Type::F32) return "f32";
    if (t == Type::U8) return "u8";
    if (t == Type::U16) return "u16";
    if (t == Type::U32) return "u32";
    if (t == Type::U64) return "u64";
    return {};
}

struct InstructionData {
    ArchonMIRInst c{};
    std::string symbol;
    std::vector<V> arguments;
};
struct BlockData {
    std::vector<V> params;
    std::vector<InstructionData> insts;
    std::vector<ArchonMIRInst> c_insts;
    std::vector<V> yes_args;
    std::vector<V> no_args;
    ArchonMIRTerm term{};
};
struct FunctionData {
    std::string name;
    ArchonMIRType result = ARCHON_MIR_VOID;
    std::size_t param_count = 0;
    std::vector<ArchonMIRType> types;
    std::vector<ArchonMIRLocal> locals;
    std::vector<BlockData> blocks;
    std::vector<ArchonMIRBlock> c_blocks;
};
struct ImportData {
    ArchonMIRType result{};
    std::vector<ArchonMIRType> params;
};
} // namespace

struct MachineModule::Impl {
    std::size_t instruction_count = 0;
    std::vector<std::unique_ptr<FunctionData>> functions;
    std::map<std::string, ImportData> imports;
    std::vector<ArchonMIRFunction> c_functions;
    std::vector<ArchonMIRImport> c_imports;
    ArchonMIRModule c{};

    void import(std::string name, ArchonMIRType result,
                std::vector<ArchonMIRType> params, const Location &at) {
        const auto it = imports.find(name);
        if (it != imports.end()) {
            if (it->second.result != result || it->second.params != params)
                fail(at, "conflicting external function declaration");
            return;
        }
        imports.emplace(std::move(name), ImportData{result, std::move(params)});
    }

    void finalize(const Options &options) {
        // All input vectors and strings are frozen before borrowing pointers.
        c_functions.reserve(functions.size());
        for (auto &holder : functions) {
            poll(options, {"<mir>"});
            auto &f = *holder;
            f.c_blocks.reserve(f.blocks.size());
            for (auto &b : f.blocks) {
                poll(options, {"<mir>"});
                b.c_insts.reserve(b.insts.size());
                for (auto &i : b.insts) {
                    auto c = i.c;
                    c.symbol = i.symbol.empty() ? nullptr : i.symbol.c_str();
                    c.args = i.arguments.empty() ? nullptr : i.arguments.data();
                    c.arg_count = i.arguments.size();
                    b.c_insts.push_back(c);
                }
                b.term.yes.args = b.yes_args.empty() ? nullptr : b.yes_args.data();
                b.term.yes.arg_count = b.yes_args.size();
                b.term.no.args = b.no_args.empty() ? nullptr : b.no_args.data();
                b.term.no.arg_count = b.no_args.size();
                f.c_blocks.push_back({b.params.empty() ? nullptr : b.params.data(), b.params.size(),
                    b.c_insts.empty() ? nullptr : b.c_insts.data(), b.c_insts.size(), b.term});
            }
            c_functions.push_back({f.name.c_str(), f.result,
                f.types.empty() ? nullptr : f.types.data(), f.types.size(), f.param_count,
                f.c_blocks.data(), f.c_blocks.size(),
                f.locals.empty() ? nullptr : f.locals.data(), f.locals.size()});
        }
        c_imports.reserve(imports.size());
        for (const auto &[name, imp] : imports)
            c_imports.push_back({name.c_str(), imp.result,
                imp.params.empty() ? nullptr : imp.params.data(), imp.params.size()});
        c = {ARCHON_MIR_SCHEMA_VERSION, c_functions.data(), c_functions.size(),
            c_imports.empty() ? nullptr : c_imports.data(), c_imports.size(), nullptr, 0};
    }
};

namespace {
class Builder {
    MachineModule::Impl &owner_;
    const Options &options_;
    std::size_t instruction_count_ = 0;
    std::size_t local_bytes_ = 0;
    const mir::Function &source_;
    FunctionData &out_;
    std::map<mir::Id, V> values_;
    std::map<mir::Id, Type> types_;
    std::map<mir::Id, std::size_t> slot_indices_;
    std::map<mir::Id, Type> slot_types_;
    // Dedicated per-target parameter storage. Loop backedges snapshot all
    // aggregate sources before updating *any* destination (parallel phi).
    std::map<mir::Id, std::size_t> tuple_phi_locals_;
    std::map<mir::Id, std::uint32_t> block_ids_;
    V sret_ = none;
    BlockData *block_ = nullptr;
    Location at_;

    V allocate(ArchonMIRType type) {
        poll(options_, at_);
        if (out_.types.size() >= options_.max_values_per_function ||
            out_.types.size() >= static_cast<std::size_t>(none))
            fail(at_, "MachineIR value budget exceeded", ARCHON_BACKEND_LIMIT);
        const V value = static_cast<V>(out_.types.size());
        out_.types.push_back(type);
        return value;
    }
    V temporary(ArchonMIRType type = ARCHON_MIR_I64) { return allocate(type); }
    V lookup(mir::Id id) const {
        const auto it = values_.find(id);
        if (it == values_.end()) fail(at_, "missing verified SSA value");
        return it->second;
    }
    const Type &value_type(mir::Id id) const {
        const auto it = types_.find(id);
        if (it == types_.end()) fail(at_, "missing verified SSA type");
        return it->second;
    }
    std::size_t local(const Type &type) {
        poll(options_, at_);
        const auto n = words(type, at_);
        if (n > max_frame_local_bytes / 8 || out_.locals.size() >= UINT32_MAX)
            fail(at_, "frame local exceeds bounded native representation", ARCHON_BACKEND_LIMIT);
        const auto size = static_cast<std::uint32_t>(std::max<std::size_t>(1, n) * 8);
        if (size > options_.max_local_bytes_per_function - local_bytes_)
            fail(at_, "MachineIR local storage budget exceeded", ARCHON_BACKEND_LIMIT);
        local_bytes_ += size;
        const auto index = out_.locals.size();
        out_.locals.push_back({size, 8});
        return index;
    }
    void emit(ArchonMIROpcode op, V dst = none, V a = none, V b = none,
              std::uint64_t imm = 0, std::string symbol = {}, std::vector<V> args = {}) {
        poll(options_, at_);
        if (instruction_count_ >= options_.max_instructions_per_function)
            fail(at_, "MachineIR instruction budget exceeded", ARCHON_BACKEND_LIMIT);
        if (owner_.instruction_count >= options_.max_instructions_per_module)
            fail(at_, "MachineIR module instruction budget exceeded", ARCHON_BACKEND_LIMIT);
        ++instruction_count_;
        ++owner_.instruction_count;
        InstructionData d;
        d.c = {op, dst, a, b, imm, nullptr, nullptr, 0};
        d.symbol = std::move(symbol);
        d.arguments = std::move(args);
        block_->insts.push_back(std::move(d));
    }
    V constant(std::uint64_t value) {
        const auto dst = temporary();
        emit(ARCHON_MIR_CONST, dst, none, none, value);
        return dst;
    }
    V offset_ptr(V base, std::size_t word_offset) {
        if (!word_offset) return base;
        if (word_offset > UINT64_MAX / 8) fail(at_, "tuple field offset overflow");
        const auto offset = constant(static_cast<std::uint64_t>(word_offset) * 8);
        const auto result = temporary(ARCHON_MIR_PTR);
        emit(ARCHON_MIR_PTR_ADD, result, base, offset);
        return result;
    }
    V frame_address(std::size_t index, V result = none) {
        if (result == none) result = temporary(ARCHON_MIR_PTR);
        emit(ARCHON_MIR_FRAME_ADDR, result, none, none, index);
        return result;
    }
    void copy_words(V dest, V src, std::size_t count) {
        for (std::size_t n = 0; n < count; ++n) {
            // Large tuples do not rely on a displacement outside signed 12-bit.
            auto from = offset_ptr(src, n);
            auto to = offset_ptr(dest, n);
            auto bits = temporary();
            emit(ARCHON_MIR_LOAD, bits, from);
            emit(ARCHON_MIR_STORE, none, to, bits);
        }
    }
    void put_value(V destination, V source, const Type &type) {
        if (type.is_tuple()) copy_words(destination, source, words(type, at_));
        else emit(ARCHON_MIR_STORE, none, destination, source);
    }
    void extern_call(std::string symbol, ArchonMIRType result,
                     std::vector<V> operands, V destination = none) {
        std::vector<ArchonMIRType> types;
        types.reserve(operands.size());
        for (auto v : operands) types.push_back(out_.types.at(v));
        owner_.import(symbol, result, std::move(types), at_);
        emit(ARCHON_MIR_CALL, destination, none, none, 0, std::move(symbol), std::move(operands));
    }
    void runtime(std::string symbol, const mir::Instruction &i,
                 bool raw_abi = false) {
        std::vector<V> args;
        for (auto id : i.operands) args.push_back(lookup(id));
        auto result = i.type == Type::Void ? ARCHON_MIR_VOID : abi_type(i.type, at_);
        extern_call((raw_abi ? "on1x_" : "on1x_archon_") + symbol, result,
                    std::move(args), i.type == Type::Void ? none : lookup(i.id));
    }
    void compare(const mir::Instruction &i, ArchonMIROpcode base,
                 bool swap, bool invert) {
        auto x = lookup(i.operands.at(swap ? 1 : 0));
        auto y = lookup(i.operands.at(swap ? 0 : 1));
        auto dst = invert ? temporary() : lookup(i.id);
        emit(base, dst, x, y);
        if (invert) emit(ARCHON_MIR_EQ, lookup(i.id), dst, constant(0));
    }
    void unsigned_compare(const mir::Instruction &i, std::string_view op) {
        if (op == "eq") compare(i, ARCHON_MIR_EQ, false, false);
        else if (op == "ne") compare(i, ARCHON_MIR_EQ, false, true);
        else if (op == "lt") compare(i, ARCHON_MIR_ULT, false, false);
        else if (op == "gt") compare(i, ARCHON_MIR_ULT, true, false);
        else if (op == "le") compare(i, ARCHON_MIR_ULT, true, true);
        else if (op == "ge") compare(i, ARCHON_MIR_ULT, false, true);
        else fail(at_, "invalid unsigned comparison primitive");
    }
    void numeric(const mir::Instruction &i) {
        // Validated by on1x::mir::verify. Runtime wrapper is exact spelling of
        // its allowlisted descriptor; no dynamic foreign symbol resolution.
        const auto underscore = i.primitive.find('_');
        if (underscore != std::string::npos) {
            auto op = std::string_view(i.primitive).substr(0, underscore);
            auto suffix = std::string_view(i.primitive).substr(underscore + 1);
            if (suffix == "u8" || suffix == "u16" || suffix == "u32" || suffix == "u64") {
                if (op == "eq" || op == "ne" || op == "lt" || op == "le" ||
                    op == "gt" || op == "ge") { unsigned_compare(i, op); return; }
            }
        }
        if (i.type.is_unsigned()) {
            const auto prefix = runtime_type(i.type) + "_";
            if (i.primitive.starts_with(prefix)) {
                const auto op = std::string_view(i.primitive).substr(prefix.size());
                ArchonMIROpcode native{};
                if (op == "wrapping_add") native = ARCHON_MIR_ADD;
                else if (op == "wrapping_sub") native = ARCHON_MIR_SUB;
                else if (op == "wrapping_mul") native = ARCHON_MIR_MUL;
                else if (op == "and") native = ARCHON_MIR_AND;
                else if (op == "or") native = ARCHON_MIR_OR;
                else if (op == "xor" || op == "not") native = ARCHON_MIR_XOR;
                if (native != ArchonMIROpcode{}) {
                    // Runtime unsigned operations use uint64_t arithmetic then
                    // cast to T. Modular arithmetic commutes with truncation,
                    // including multiplication and subtraction underflow.
                    const auto width = i.type == Type::U8 ? 8u :
                        i.type == Type::U16 ? 16u : i.type == Type::U32 ? 32u : 64u;
                    const auto dst = lookup(i.id);
                    const auto result = width == 64 ? dst : temporary();
                    const auto rhs = op == "not" ? constant(UINT64_MAX) :
                        lookup(i.operands.at(1));
                    emit(native, result, lookup(i.operands.at(0)), rhs);
                    if (width != 64) emit(ARCHON_MIR_TRUNC, dst, result, none, width);
                    return;
                }
            }
        }
        runtime(i.primitive, i);
    }
    void instruction(const mir::Instruction &i) {
        at_ = i.where;
        const auto dst = i.type == Type::Void ? none : lookup(i.id);
        const auto x = i.operands.empty() ? none : lookup(i.operands[0]);
        switch (i.op) {
        case O::Constant:
            if (i.type == Type::F64 || i.type == Type::F32) {
                // Match the C11 oracle's from-bits entrypoint: canonicalize
                // through the qualified runtime without altering their bits.
                const auto bits = constant(std::bit_cast<std::uint64_t>(i.immediate));
                extern_call(i.type == Type::F64 ? "on1x_archon_f64_from_bits"
                                                 : "on1x_archon_f32_from_bits",
                            ARCHON_MIR_I64, {bits}, dst);
            } else {
                emit(ARCHON_MIR_CONST, dst, none, none,
                     std::bit_cast<std::uint64_t>(i.immediate));
            }
            return;
        case O::Add: runtime("add_i64", i, true); return;
        case O::Sub: runtime("sub_i64", i, true); return;
        case O::Mul: runtime("mul_i64", i, true); return;
        case O::Div: runtime("div_i64", i, true); return;
        case O::Mod: runtime("mod_i64", i, true); return;
        case O::Neg: runtime("neg_i64", i, true); return;
        case O::Not: emit(ARCHON_MIR_EQ, dst, x, constant(0)); return;
        case O::Eq: compare(i, ARCHON_MIR_EQ, false, false); return;
        case O::Ne: compare(i, ARCHON_MIR_EQ, false, true); return;
        case O::Lt: compare(i, ARCHON_MIR_SLT, false, false); return;
        case O::Le: compare(i, ARCHON_MIR_SLT, true, true); return;
        case O::Gt: compare(i, ARCHON_MIR_SLT, true, false); return;
        case O::Ge: compare(i, ARCHON_MIR_SLT, false, true); return;
        case O::Load: {
            auto it = slot_indices_.find(i.target);
            if (it == slot_indices_.end()) fail(at_, "unknown slot during lowering");
            const auto ptr = frame_address(it->second);
            if (i.type.is_tuple()) {
                auto outptr = frame_address(local(i.type), dst);
                copy_words(outptr, ptr, words(i.type, at_));
            } else emit(ARCHON_MIR_LOAD, dst, ptr);
            return;
        }
        case O::Store: {
            auto it = slot_indices_.find(i.target);
            if (it == slot_indices_.end()) fail(at_, "unknown slot during lowering");
            put_value(frame_address(it->second), x, slot_types_.at(i.target));
            return;
        }
        case O::Call: {
            std::vector<V> args;
            if (i.type.is_tuple()) args.push_back(frame_address(local(i.type), dst));
            for (auto id : i.operands) args.push_back(lookup(id));
            const auto c_dst = (i.type == Type::Void || i.type.is_tuple()) ? none : dst;
            // The canonical verifier has already resolved the target/signature.
            emit(ARCHON_MIR_CALL, c_dst, none, none, 0, source_symbol(i.target), std::move(args));
            return;
        }
        case O::Print: {
            const auto type = runtime_type(value_type(i.operands[0]));
            if (type.empty()) fail(at_, "print of unrepresentable scalar");
            const bool float_type = type == "f32" || type == "f64";
            extern_call(std::string(float_type ? "on1x_archon_print_" : "on1x_print_") + type,
                        ARCHON_MIR_VOID, {x});
            return;
        }
        case O::Assert: extern_call("on1x_assert", ARCHON_MIR_VOID, {x}); return;
        case O::Tuple: {
            V dest = frame_address(local(i.type), dst);
            std::size_t offset = 0;
            for (auto id : i.operands) {
                const auto &t = value_type(id);
                put_value(offset_ptr(dest, offset), lookup(id), t);
                offset += words(t, at_);
            }
            return;
        }
        case O::Extract: {
            const auto &parent_type = value_type(i.operands.at(0));
            if (!parent_type.is_tuple()) fail(at_, "extract is not a tuple");
            const auto &items = parent_type.elements();
            if (i.immediate < 0 || static_cast<std::uint64_t>(i.immediate) >= items.size())
                fail(at_, "extract index out of range");
            std::size_t offset = 0;
            for (std::size_t n = 0; n < static_cast<std::size_t>(i.immediate); ++n)
                offset += words(items[n], at_);
            auto ptr = offset_ptr(x, offset);
            if (i.type.is_tuple()) emit(ARCHON_MIR_COPY, dst, ptr);
            else emit(ARCHON_MIR_LOAD, dst, ptr);
            return;
        }
        case O::FloatAdd: runtime("add_f64", i); return;
        case O::FloatSub: runtime("sub_f64", i); return;
        case O::FloatMul: runtime("mul_f64", i); return;
        case O::FloatDiv: runtime("div_f64", i); return;
        case O::FloatNeg: runtime("neg_f64", i); return;
        case O::FloatEq: runtime("eq_f64", i); return;
        case O::FloatNe: runtime("ne_f64", i); return;
        case O::FloatLt: runtime("lt_f64", i); return;
        case O::FloatLe: runtime("le_f64", i); return;
        case O::FloatGt: runtime("gt_f64", i); return;
        case O::FloatGe: runtime("ge_f64", i); return;
        case O::FloatFromI64: runtime("f64_from_i64", i); return;
        case O::FloatToI64: runtime("f64_to_i64", i); return;
        case O::FloatFromBits: runtime("f64_from_bits", i); return;
        case O::FloatToBits: runtime("f64_to_bits", i); return;
        case O::FloatIsNaN: runtime("f64_is_nan", i); return;
        case O::FloatIsFinite: runtime("f64_is_finite", i); return;
        case O::FloatSignbit: runtime("f64_signbit", i); return;
        case O::Numeric: numeric(i); return;
        }
        fail(at_, "unknown verified opcode");
    }
    struct LoweredEdge {
        std::uint32_t target;
        std::vector<V> args;
    };
    LoweredEdge edge(const mir::Edge &incoming) {
        const auto target_index = block_ids_.at(incoming.block);
        const auto &destination = source_.blocks.at(target_index);
        std::vector<V> original;
        original.reserve(incoming.arguments.size());
        bool needs_aggregate_copy = false;
        for (std::size_t n = 0; n < incoming.arguments.size(); ++n) {
            original.push_back(lookup(incoming.arguments[n]));
            needs_aggregate_copy |= destination.parameters[n].type.is_tuple();
        }
        if (!needs_aggregate_copy) return {target_index, std::move(original)};
        // Split *only* the taken edge. A conditional must not mutate the phi
        // storage of its untaken successor. Proxy blocks take exact edge args
        // as SSA parameters, preserving values until the transfer executes.
        if (out_.blocks.size() >= UINT32_MAX)
            fail(at_, "too many native CFG blocks", ARCHON_BACKEND_LIMIT);
        const auto proxy_index = static_cast<std::uint32_t>(out_.blocks.size());
        out_.blocks.emplace_back();
        auto *prior = block_;
        block_ = &out_.blocks.back();
        for (const auto &param : destination.parameters)
            block_->params.push_back(temporary(abi_type(param.type, param.where)));

        // Stage *all* source tuple payloads before writing any phi slot.
        // This avoids corruption for swaps (x,y) -> (y,x) and loop backedges.
        std::vector<V> staged(destination.parameters.size(), none);
        for (std::size_t n = 0; n < destination.parameters.size(); ++n) {
            const auto &param = destination.parameters[n];
            if (!param.type.is_tuple()) continue;
            auto stage = frame_address(local(param.type));
            copy_words(stage, block_->params[n], words(param.type, at_));
            staged[n] = stage;
        }
        for (std::size_t n = 0; n < destination.parameters.size(); ++n) {
            const auto &param = destination.parameters[n];
            if (param.type.is_tuple()) {
                const auto dest = frame_address(tuple_phi_locals_.at(param.id));
                copy_words(dest, staged[n], words(param.type, at_));
                block_->yes_args.push_back(dest);
            } else {
                block_->yes_args.push_back(block_->params[n]);
            }
        }
        block_->term = {ARCHON_MIR_BR, none, {target_index, nullptr, 0}, {0, nullptr, 0}};
        block_ = prior;
        return {proxy_index, std::move(original)};
    }
    void terminal(const mir::Terminator &t) {
        at_ = t.where;
        ArchonMIRTerm result{};
        result.value = none;
        result.yes = {0, nullptr, 0};
        result.no = {0, nullptr, 0};
        switch (t.kind) {
        case mir::Exit::Return:
            result.kind = ARCHON_MIR_RETURN;
            if (source_.result.is_tuple()) {
                copy_words(sret_, lookup(t.value), words(source_.result, at_));
            } else if (source_.result != Type::Void) {
                result.value = lookup(t.value);
            }
            break;
        case mir::Exit::Jump: {
            result.kind = ARCHON_MIR_BR;
            auto next = edge(t.edges.at(0));
            result.yes.target = next.target;
            block_->yes_args = std::move(next.args);
            break;
        }
        case mir::Exit::Branch: {
            result.kind = ARCHON_MIR_CBR;
            result.value = lookup(t.value);
            auto yes = edge(t.edges.at(0));
            auto no = edge(t.edges.at(1));
            result.yes.target = yes.target;
            result.no.target = no.target;
            block_->yes_args = std::move(yes.args);
            block_->no_args = std::move(no.args);
            break;
        }
        default: fail(at_, "invalid verified terminator");
        }
        block_->term = result;
    }

public:
    Builder(MachineModule::Impl &owner, const Options &options,
            const mir::Function &source, FunctionData &out)
        : owner_(owner), options_(options), source_(source),
          out_(out), at_(source.where) {}
    void run() {
        poll(options_, at_);
        if (source_.blocks.front().parameters.size() +
                (source_.result.is_tuple() ? 1u : 0u) > ARCHON_MIR_MAX_PARAMS)
            fail(at_, "native ABI parameter limit exceeded (including tuple result pointer)",
                 ARCHON_BACKEND_LIMIT);
        out_.name = source_symbol(source_.id);
        out_.result = source_.result.is_tuple() ? ARCHON_MIR_VOID :
            source_.result == Type::Void ? ARCHON_MIR_VOID : abi_type(source_.result, at_);
        if (source_.blocks.size() > UINT32_MAX / 3u)
            fail(at_, "native CFG block budget exhausted", ARCHON_BACKEND_LIMIT);
        out_.blocks.reserve(source_.blocks.size() * 3u);
        out_.blocks.resize(source_.blocks.size());
        if (source_.result.is_tuple()) sret_ = allocate(ARCHON_MIR_PTR);
        // MachineIR entry has zero block parameters: these are ABI parameters.
        for (const auto &p : source_.blocks.front().parameters) {
            at_ = p.where;
            values_.emplace(p.id, allocate(abi_type(p.type, at_)));
            types_.emplace(p.id, p.type);
        }
        out_.param_count = out_.types.size();
        for (std::size_t n = 0; n < source_.blocks.size(); ++n) {
            const auto &src = source_.blocks[n];
            at_ = src.where;
            block_ids_.emplace(src.id, static_cast<std::uint32_t>(n));
            if (n != 0) for (const auto &p : src.parameters) {
                values_.emplace(p.id, allocate(abi_type(p.type, p.where)));
                types_.emplace(p.id, p.type);
                out_.blocks[n].params.push_back(values_.at(p.id));
                if (p.type.is_tuple()) tuple_phi_locals_.emplace(p.id, local(p.type));
            }
            for (const auto &i : src.instructions) {
                if (i.type == Type::Void) continue;
                values_.emplace(i.id, allocate(abi_type(i.type, i.where)));
                types_.emplace(i.id, i.type);
            }
        }
        for (const auto &slot : source_.slots) {
            at_ = slot.where;
            slot_indices_.emplace(slot.id, local(slot.type));
            slot_types_.emplace(slot.id, slot.type);
        }
        for (std::size_t n = 0; n < source_.blocks.size(); ++n) {
            block_ = &out_.blocks[n];
            for (const auto &i : source_.blocks[n].instructions) instruction(i);
            terminal(source_.blocks[n].exit);
        }
    }
};

void build_entrypoint(MachineModule::Impl &m, const mir::Module &source, const Options &options) {
    const mir::Function *entry = nullptr;
    for (const auto &f : source.functions) if (f.name == "main") { entry = &f; break; }
    if (!entry) fail({"<mir>"}, "no main function for executable entrypoint");
    if (!entry->blocks.front().parameters.empty() ||
        (entry->result != Type::Void && entry->result != Type::I64))
        fail(entry->where, "main requires no arguments and i64 or void result");
    poll(options, entry->where);
    const std::size_t count = entry->result == Type::Void ? 2 : 1;
    if (count > options.max_instructions_per_function ||
        count > options.max_instructions_per_module - m.instruction_count)
        fail(entry->where, "MachineIR entrypoint instruction budget exceeded", ARCHON_BACKEND_LIMIT);
    m.instruction_count += count;
    auto f = std::make_unique<FunctionData>();
    f->name = "main";
    f->result = ARCHON_MIR_I64;
    f->param_count = 0;
    f->types.push_back(ARCHON_MIR_I64);
    f->blocks.resize(1);
    auto &b = f->blocks.front();
    InstructionData call;
    call.symbol = source_symbol(entry->id);
    call.c = {ARCHON_MIR_CALL, entry->result == Type::I64 ? 0u : none,
              none, none, 0, nullptr, nullptr, 0};
    b.insts.push_back(std::move(call));
    if (entry->result == Type::Void) {
        InstructionData zero;
        zero.c = {ARCHON_MIR_CONST, 0, none, none, 0, nullptr, nullptr, 0};
        b.insts.push_back(std::move(zero));
    }
    b.term = {ARCHON_MIR_RETURN, 0, {0, nullptr, 0}, {0, nullptr, 0}};
    m.functions.push_back(std::move(f));
}
} // namespace

MachineModule::MachineModule(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
MachineModule::~MachineModule() = default;
MachineModule::MachineModule(MachineModule &&) noexcept = default;
MachineModule &MachineModule::operator=(MachineModule &&) noexcept = default;
const ArchonMIRModule &MachineModule::view() const noexcept {
    static const ArchonMIRModule empty{};
    return impl_ ? impl_->c : empty;
}

MachineModule lower(const mir::Module &source, const Options &options) {
    poll(options, {"<mir>"});
    if (!options.max_values_per_function || !options.max_instructions_per_function ||
        !options.max_local_bytes_per_function || !options.max_instructions_per_module ||
        options.max_values_per_function > UINT32_MAX ||
        options.max_local_bytes_per_function > max_frame_local_bytes)
        fail({"<mir>"}, "invalid generated MachineIR resource budgets");
    mir::verify(source, options.verification_limits);
    poll(options, {"<mir>"});
    auto owned = std::make_unique<MachineModule::Impl>();
    owned->functions.reserve(source.functions.size() + (options.emit_c_entrypoint ? 1 : 0));
    for (const auto &func : source.functions) {
        auto f = std::make_unique<FunctionData>();
        Builder(*owned, options, func, *f).run();
        owned->functions.push_back(std::move(f));
    }
    if (options.emit_c_entrypoint) build_entrypoint(*owned, source, options);
    owned->finalize(options);
    poll(options, {"<mir>"});
    return MachineModule(std::move(owned));
}

Product compile(const mir::Module &source, const Options &options) {
    if (!options.max_assembly_bytes || options.max_assembly_bytes > 256u * 1024u * 1024u)
        fail({"<mir>"}, "invalid assembly output budget");
    const auto target_name = archon_backend_target();
    if (!target_name) fail({"<archon>"}, "generated native backend has no target");
    const std::string_view target(target_name);
    if (target != "AMD64/SysVAMD64" && target != "AArch64/AAPCS64" &&
        target != "RISCV64I/LP64" && target != "RISCV64IM/LP64")
        fail({"<archon>"}, "unqualified generated target/ABI: " + std::string(target),
             ARCHON_BACKEND_UNSUPPORTED);
    // Verify before traversing recursive types, even on a rejected FP target.
    auto module = lower(source, options);
    // On1x currently qualifies binary32/64 runtime semantics only on its
    // Linux AMD64/SSE2 target; other Archon scalar targets cannot claim them.
    if (target != "AMD64/SysVAMD64") {
        for (const auto &f : source.functions) {
            poll(options, f.where);
            if (contains_float(f.result))
                fail(f.where, "floating-point On1x runtime requires Linux AMD64/SSE2",
                     ARCHON_BACKEND_UNSUPPORTED);
            for (const auto &slot : f.slots) if (contains_float(slot.type))
                fail(slot.where, "floating-point On1x runtime requires Linux AMD64/SSE2",
                     ARCHON_BACKEND_UNSUPPORTED);
            for (const auto &b : f.blocks) {
                poll(options, b.where);
                for (const auto &p : b.parameters) if (contains_float(p.type))
                    fail(p.where, "floating-point On1x runtime requires Linux AMD64/SSE2",
                         ARCHON_BACKEND_UNSUPPORTED);
                for (const auto &i : b.instructions) if (contains_float(i.type))
                    fail(i.where, "floating-point On1x runtime requires Linux AMD64/SSE2",
                         ARCHON_BACKEND_UNSUPPORTED);
            }
        }
    }
    const ArchonBackendOptions limits{options.max_assembly_bytes, options.cancelled,
                                      options.cancel_userdata};
    ArchonBackendArtifact output{};
    ArchonBackendDiagnostic diagnostic{};
    const auto status = archon_backend_compile_ex(&module.view(), &limits,
        options.pic ? static_cast<unsigned>(ARCHON_BACKEND_PIC) : 0u, &output, &diagnostic);
    struct ArtifactCleanup {
        ArchonBackendArtifact *artifact;
        ~ArtifactCleanup() { archon_backend_artifact_destroy(artifact); }
    } cleanup{&output};
    if (status != ARCHON_BACKEND_OK) {
        std::ostringstream message;
        message << "native backend status " << static_cast<unsigned>(status)
                << ", stage " << static_cast<unsigned>(diagnostic.stage);
        if (diagnostic.function != SIZE_MAX) message << ", function " << diagnostic.function;
        if (diagnostic.block != SIZE_MAX) message << ", block " << diagnostic.block;
        if (diagnostic.instruction != SIZE_MAX) message << ", instruction " << diagnostic.instruction;
        const auto *begin = diagnostic.message;
        const auto *end = std::find(begin, begin + sizeof(diagnostic.message), '\0');
        message << ": ";
        message.write(begin, static_cast<std::streamsize>(end - begin));
        fail({"<archon>"}, message.str(), status);
    }
    if ((!output.data && output.size) || output.size > options.max_assembly_bytes)
        fail({"<archon>"}, "native backend published invalid or excessive artifact",
             ARCHON_BACKEND_INTERNAL);
    poll(options, {"<archon>"});
    return Product{output.size ? std::string(output.data, output.size) : std::string{},
                   std::string(target)};
}
} // namespace on1x::backends::archon
