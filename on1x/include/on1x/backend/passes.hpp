#pragma once
#include <on1x/backend/mir.hpp>

namespace on1x::mir::passes {
inline constexpr unsigned pipeline_version=1;
enum class Pass { ConstantFold, SimplifyCFG, DeadPure, LocalSlots };
enum class Analysis { Signatures, CFG, Calls, Effects, Values, Liveness };
struct Descriptor {
 Pass pass;
 std::string_view name;
 unsigned version;
 std::vector<std::string_view> requirements;
 std::vector<Analysis> preserves, invalidates;
 std::string_view effects, target, budget, verifier;
};
const Descriptor& descriptor(Pass pass);
std::string_view analysis_name(Analysis analysis);
enum class Status { Complete, BudgetExhausted, Cancelled, DeadlineExceeded, CallbackFailed };
enum class Outcome {
 Unchanged,
 Committed,
 BudgetExhausted,
 Cancelled,
 DeadlineExceeded,
 CallbackFailed
};
enum class Control { Continue, Cancel, Deadline, Failure };
struct Event {
 Pass pass;
 std::size_t round=0, input_revision=0, output_revision=0;
 Outcome outcome=Outcome::Unchanged;
 std::size_t work=0, attempted_rewrites=0;
 Location where;
 std::string diagnostic;
};
struct Options {
 std::vector<Pass> pipeline={Pass::ConstantFold,Pass::SimplifyCFG,Pass::LocalSlots,Pass::DeadPure};
 std::string target="portable-scalar-v1";
 std::size_t work=4194304, rewrites=65536, rounds=8;
 mir::Limits verification;
 // Synchronous cooperative control. Throwing is reported as CallbackFailed.
 std::function<Control(std::size_t)> control;
 // Called before and after each pass attempt. `before` is true for the first call.
  std::function<Control(const Event&,bool before)> instrumentation;
  // An explicitly disabled driver profile can still request a verified report.
  bool allow_empty=false;
};
class Result {
public:
 const Module& module() const { return module_; }
 const std::string& source() const { return source_; }
 const std::string& output() const { return output_; }
 const Options& options() const { return options_; }
 const std::vector<Event>& events() const { return events_; }
 Status status() const { return status_; }
 const std::string& diagnostic() const { return diagnostic_; }
 std::size_t work() const { return work_; }
 std::size_t attempted_rewrites() const { return rewrites_; }
 bool changed() const { return source_!=output_; }
private:
 Result(Module module,std::string source,Options options)
  : module_(std::move(module)),source_(std::move(source)),output_(source_),options_(std::move(options)) {
   // Control/instrumentation and their captured userdata are call-scoped.
   options_.control={};options_.instrumentation={};
  }
 Module module_;
 std::string source_,output_;
 Options options_;
 std::vector<Event> events_;
 Status status_=Status::Complete;
 std::string diagnostic_;
 std::size_t work_=0,rewrites_=0;
 friend Result run(const Module&,Options);
};
// Built-in passes only. Each candidate commits after structural verification and
// checked preservation obligations. Exhaustion/cancellation/deadline/callback
// failure returns the last verified prefix with a distinct status; a malformed
// source or failed verifier throws without mutating the input.
Result run(const Module& module,Options options={});
// Checks structural/preservation obligations, NOT arbitrary rewrite equivalence.
// This does not manufacture a publishable Result from an external candidate.
void verify_transition(const Module& before,const Module& after,Pass pass,mir::Limits limits={});
std::string report(const Result& result);
}
