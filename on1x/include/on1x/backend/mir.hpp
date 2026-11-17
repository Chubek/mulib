#pragma once
#include <on1x/ast.hpp>
#include <functional>
#include <string_view>

namespace on1x::mir {
using Id = std::uint64_t;
inline constexpr unsigned format_version = 2;
inline constexpr unsigned legacy_format_version = 1;
inline constexpr unsigned numeric_format_version = 3;
enum class Op { Constant, Add, Sub, Mul, Div, Mod, Neg, Not, Eq, Ne, Lt, Le, Gt, Ge,
                Load, Store, Call, Print, Assert,
                FloatAdd, FloatSub, FloatMul, FloatDiv, FloatNeg,
                FloatEq, FloatNe, FloatLt, FloatLe, FloatGt, FloatGe,
                FloatFromI64, FloatToI64, FloatFromBits, FloatToBits,
                 FloatIsNaN, FloatIsFinite, FloatSignbit, Tuple, Extract, Numeric };
enum class Exit { Invalid, Return, Jump, Branch };
enum class Effect { Pure, Trap, Memory, Call, IO };
struct Value { Id id=0; Type type=Type::Invalid; Location where; };
struct Instruction {
 Id id=0; Type type=Type::Void; Op op=Op::Constant;
 std::vector<Id> operands;
 std::int64_t immediate=0;
 Id target=0, effect_in=0, effect_out=0;
 Location where;
 // v3-only allowlisted scalar numeric operation. Never a foreign symbol.
 std::string primitive={};
};
struct Edge { Id block=0; std::vector<Id> arguments; };
struct Terminator {
 Id id=0; Exit kind=Exit::Invalid; Id value=0, effect=0;
 std::vector<Edge> edges; Location where;
};
struct Block {
 Id id=0, effect=0; Location where;
 std::vector<Value> parameters;
 std::vector<Instruction> instructions;
 Terminator exit;
};
struct Function {
 Id id=0; std::string name; Type result=Type::Void; Location where;
 // Entry block parameters are the function arguments. Slots are function-local,
 // non-addressable value storage; they cannot escape or alias another frame.
 std::vector<Value> slots;
 std::vector<Block> blocks;
};
struct Module { unsigned version=legacy_format_version; Id id=1; std::vector<Function> functions; };
struct Limits { std::size_t nodes=65536, work=4194304; };
Effect effect(Op op);
std::string_view op_name(Op op);
void verify(const Module& module, Limits limits={});
// Requires checked, elaborated HIR; unsupported operations fail with source locations.
Module lower(const Unit& unit, Limits limits={});
std::string serialize(const Module& module);
Module deserialize(std::string_view text, const std::string& filename="<mir>");
bool structurally_equal(const Module& a, const Module& b);
std::string cfg_dot(const Module& module);
std::string emit_c(const Module& module);

enum class Status { Returned, Trapped, BudgetExhausted };
// Scalar storage is an i64 number, bool 0/1, or exact typed unsigned/IEEE bit
// payload (u64 uses all 64 bits, f32 only the low 32). No signed arithmetic is
// performed on unsigned payloads. Decoders validate widths and canonical NaNs.
// Tuple storage has only elements, in type order. This is not a foreign ABI.
struct RuntimeValue { std::int64_t scalar=0; std::vector<RuntimeValue> elements; };
struct Evaluation {
 Status status=Status::Returned; Type type=Type::Void; std::int64_t value=0;
 std::string output, diagnostic; Location where; std::size_t steps=0;
 RuntimeValue data;
};
// Reference interpreter only: no host FFI, no native/VM backend claim.
Evaluation evaluate(const Module& module, std::string_view function="main",
                    const std::vector<std::int64_t>& arguments={},
                    std::size_t steps=1000000, std::size_t call_depth=256);
Evaluation evaluate_values(const Module& module, std::string_view function,
                          const std::vector<RuntimeValue>& arguments,
                          std::size_t steps=1000000, std::size_t call_depth=256);
}
