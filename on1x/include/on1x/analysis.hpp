#pragma once
#include <on1x/backend/mir.hpp>
#include <on1x/diagnostic.hpp>
#include <cstdint>
#include <functional>
#include <optional>

namespace on1x::analysis {
struct Options {
 std::optional<std::string> command_scrutiny;
 std::optional<std::string> project_scrutiny;
 std::size_t work=100000;
 bool repairs=true;
};

struct Statistics {
 bool wetstone_available=false;
 bool complete=true;
 std::size_t functions=0,analyzed_functions=0,skipped_functions=0,work=0;
};

struct Context {
 const diagnostic::Catalog& catalog;
 diagnostic::Registry& diagnostics;
 Options options;
 Statistics statistics;
};

bool wetstone_available() noexcept;
void run(const Unit&,Context&);
}

namespace on1x {
// The ordinary overload remains source-compatible. This overload exposes
// bounded advisory diagnostics to compiler hosts without process-global state.
void check(Unit&,analysis::Context&);
void check_interfaces(Unit,const std::vector<InterfaceObligation>&,analysis::Context&);
}

/*
 * Deterministic analyses over the canonical, verified MIR.  This is not a
 * second IR and does not infer source-language ownership or foreign aliasing.
 * Each invocation verifies its input before deriving records.
 */
namespace on1x::mir::analysis {
inline constexpr std::uint32_t analysis_version=1;

enum class Kind : std::uint32_t {
 CFG=1,
 Dominators=2,
 Loops=3,
 LivenessUseDef=4,
 Effects=5
};
enum class Status : std::uint32_t {
 Complete,
 BudgetExhausted,
 Cancelled,
 DeadlineExceeded,
 CallbackFailed
};
enum class Control : std::uint32_t {
 Continue,
 Cancel,
 Deadline,
 Failure
};
enum class DefinitionKind : std::uint32_t { BlockParameter=1, Instruction=2 };
enum class UseKind : std::uint32_t {
 InstructionOperand=1,
 TerminatorValue=2,
 EdgeArgument=3
};

struct Options {
 Kind kind=Kind::CFG;
 std::size_t work=4194304;
 std::size_t records=262144;
 mir::Limits verification;
 // Called synchronously with deterministic consumed work. Throwing is Failure.
 std::function<Control(std::size_t)> control;
};
struct CFGBlock {
 Id function=0,block=0;
 std::size_t predecessors=0,successors=0;
 bool entry=false;
 friend auto operator<=>(const CFGBlock&,const CFGBlock&)=default;
};
struct CFGEdge {
 Id function=0,source=0,target=0,terminator=0;
 std::size_t ordinal=0;
 friend auto operator<=>(const CFGEdge&,const CFGEdge&)=default;
};
struct Dominator {
 Id function=0,block=0,immediate=0;
 std::size_t depth=0;
 friend auto operator<=>(const Dominator&,const Dominator&)=default;
};
struct Loop {
 Id function=0,header=0,latch=0;
 std::size_t ordinal=0,parent=SIZE_MAX;
 bool reducible=true;
 friend auto operator<=>(const Loop&,const Loop&)=default;
};
struct LoopMember {
 Id function=0,block=0;
 std::size_t loop=0;
 friend auto operator<=>(const LoopMember&,const LoopMember&)=default;
};
struct Definition {
 Id function=0,block=0,value=0,owner=0;
 DefinitionKind kind=DefinitionKind::Instruction;
 friend auto operator<=>(const Definition&,const Definition&)=default;
};
struct Use {
 Id function=0,block=0,value=0,user=0,incoming_parameter=0;
 UseKind kind=UseKind::InstructionOperand;
 std::size_t edge=SIZE_MAX,index=0;
 friend auto operator<=>(const Use&,const Use&)=default;
};
struct LiveValue {
 Id function=0,block=0,value=0;
 bool live_in=false;
 friend auto operator<=>(const LiveValue&,const LiveValue&)=default;
};
struct EffectStep {
 Id function=0,block=0,instruction=0;
 Effect effect=Effect::Pure;
 friend auto operator<=>(const EffectStep&,const EffectStep&)=default;
};
struct FunctionEffects {
 Id function=0;
 std::uint32_t mask=0;
 friend auto operator<=>(const FunctionEffects&,const FunctionEffects&)=default;
};

struct Result {
 Status status=Status::Complete;
 Kind kind=Kind::CFG;
 std::size_t work=0;
 std::string diagnostic;
 Location where{"<mir-analysis>"};
 std::vector<CFGBlock> blocks;
 std::vector<CFGEdge> edges;
 std::vector<Dominator> dominators;
 std::vector<Loop> loops;
 std::vector<LoopMember> loop_members;
 std::vector<Definition> definitions;
 std::vector<Use> uses;
 std::vector<LiveValue> liveness;
 std::vector<EffectStep> effects;
 std::vector<FunctionEffects> function_effects;

 std::size_t record_count() const noexcept;
};

// Non-complete results contain no records. Verification failures throw Error.
Result run(const Module&,Options={});
} // namespace on1x::mir::analysis
