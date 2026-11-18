#pragma once
#include <on1x/type.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
namespace on1x {
inline constexpr std::size_t max_source_bytes = 2 * 1024 * 1024;
inline constexpr std::size_t max_syntax_depth = 256;
struct Location { std::string file; std::size_t offset = 0, line = 1, column = 1; };
struct Error : std::runtime_error {
 Location where;
 // Reporting metadata only: never used to recover or accept an invalid program.
 std::string diagnostic_id,expected,found;
 Error(Location at, std::string message, std::string id="ON1X-SOURCE", std::string want={}, std::string got={})
  : std::runtime_error(std::move(message)), where(std::move(at)), diagnostic_id(std::move(id)),
    expected(std::move(want)), found(std::move(got)) {}
};
struct Token { std::string text; Location where; };
struct Capture { std::string name; Type type; };
struct Pattern {
 enum class Kind { Name, Tuple, Wildcard, Rest, Literal, Variant, Array, Record, Or, As } kind=Kind::Name;
  Location where;
 std::string name;
 std::size_t context=0; // Syntax context, resolved before inference.
 std::vector<Pattern> elements; // Empty for a name; at least two for a tuple.
  std::vector<std::string> labels;
  bool open=false;
  Type type=Type::Invalid; // Inferred only; never transported as pattern evidence.
  bool is_tuple() const { return kind==Kind::Tuple||(kind==Kind::Name&&!elements.empty()); }
  bool binds() const { return (kind==Kind::Name&&elements.empty())||kind==Kind::As; }
};
// Bindings of an or-pattern share one lexical identity. Its right alternative
// is checked separately for equal names/types, not introduced a second time.
template<class F> void pattern_bindings(Pattern& p,F&& f){
 if(p.binds())f(p);
 for(std::size_t i=0;i<p.elements.size();++i){if(p.kind==Pattern::Kind::Or&&i)break;pattern_bindings(p.elements[i],f);}
}
template<class F> void pattern_bindings(const Pattern& p,F&& f){
 if(p.binds())f(p);
 for(std::size_t i=0;i<p.elements.size();++i){if(p.kind==Pattern::Kind::Or&&i)break;pattern_bindings(p.elements[i],f);}
}
struct Param {
 std::string name; Type type; Location where;
 std::size_t context=0;
 // Pattern parameters have unlexable internal names, never emitted as C names.
 std::optional<Pattern> pattern;
};
struct Stmt;
struct Expr;
struct Macro;
using ExprPtr = std::unique_ptr<Expr>;
struct Expr {
  enum class Kind { Integer, Float, Boolean, String, Variable, Unary, Binary, Call, Lambda, Tuple, PolyPack, PolyProjection,
                    MacroCall, SyntaxHole, SyntaxBlock, Extension, SyntaxList, SyntaxVector, SyntaxKeyword,
                     Quote, Splice, Lift, Run, SyntaxLet, Record, Variant, Match, Array, Field, Index, Slice, Length,
                       PatternMatch, Case, Unit, Region, TagTest, Payload, Project, Loop, Propagate, SourceCsp, Csp } kind;
 enum class Reference { Unresolved, Local, Function, Extern, Intrinsic, UnsafeForeign } reference = Reference::Unresolved;
 Location where;
 std::string text;
  std::vector<std::string> labels;
  std::vector<Pattern> patterns;
  bool statement_match=false;
  bool noreturn=false; // Checked control result, never source/PPX authority.
  std::size_t context=0;
  std::size_t control_target=0; // Source Loop only; recomputed after transforms.
 std::vector<ExprPtr> children;
 std::vector<ExprPtr> proofs; // Universality checks only; never runtime operands.
 std::size_t projection = 0;
 bool prototype = false;
 std::size_t syntax_repeat = 0;
 ExprPtr syntax_tail; // Dotted syntax, never an ordinary expression operand.
 std::vector<Macro> syntax_definitions; // Lexical let_syntax/letrec_syntax; erased before PPX.
 ExprPtr callee; // Call operands: callee first, followed by children (arguments).
 Type type = Type::Invalid;
 std::vector<Param> parameters;
 Type result = Type::Void;
 std::vector<Stmt> body;
 std::vector<Capture> captures; // Checked immutable value captures, in first-use order.
 std::size_t syntax_depth = 1; // Parser bound includes anonymous-function bodies.
 // The current frontend does not admit escaping json/json_view values.
};
struct Stmt {
  enum class Kind { Let, Assign, Return, If, While, Expr, Block, Template, Range, Break, Continue,
                     Storage, Initialize, Region, Leave, Trap, Defer, PushDefer, TaskGroup, CspScope } kind;
 Location where;
 std::string name;
  std::size_t context=0;
  std::string label; // Separate lexical namespace, including macro syntax contexts.
  std::size_t label_context=0;
  std::size_t control_target=0; // Recomputed after PPX; never accepted as proof.
  bool inclusive=false; // Source Range only; value holds the two bound expressions.
  bool unsafe_block=false; // Source-only annotation on a lexical Block.
 std::optional<Pattern> pattern; // Local tuple destructuring only; names use name.
 Type declared = Type::Invalid;
 bool mutable_binding = false;
 bool prototype = false; // Type-checked generic instance, never emitted.
 std::vector<std::unique_ptr<Stmt>> instances; // Stable storage during inference.
 ExprPtr value;
  std::vector<Stmt> body, otherwise; // While.otherwise is its latch, outside body scope.
};
struct Function {
 std::string name;
 Location where;
 std::vector<Param> params;
 Type result = Type::Void;
 bool external = false;
 // Internal typed foreign declaration. An ordinary source extern has no symbol here.
 std::string unsafe_kernel_symbol;
 bool prototype = false; // Checked generalized declaration, not emitted.
 std::vector<Stmt> body;
  std::vector<ExprPtr> attributes;
  bool extension_generated=false; // Host-only: explicit interface cannot be erased by a later PPX.
};
struct MacroParameter {
 enum class Kind { Capture, Literal, Wildcard, List, Vector, Tuple, Call, Unary, Binary } kind=Kind::Capture;
 Location where;std::string name,category;bool repeated=false;
 ExprPtr literal; // Non-null for a binding-sensitive literal pattern.
 std::vector<MacroParameter> elements;
 std::unique_ptr<MacroParameter> tail;
};
struct MacroRule {
 Location where;std::vector<MacroParameter> parameters;
 bool block=false;ExprPtr expression;std::vector<Stmt> body;
 std::unique_ptr<MacroParameter> tail;
};
struct Macro {Location where;std::string name;std::vector<MacroRule> rules;std::string ellipsis="...";std::vector<std::string> literals;};
struct PpxRegistration {Location where;std::string label,program;};
struct FeatureActivation {Location where,end;std::string name;};
struct ModuleExtension {
 Location where,end;
 std::string owner,source;
 std::vector<Token> tokens;
};
struct ImportName { std::string name,alias; };
struct Import {
 Location where;
 std::string module,alias;
 std::vector<ImportName> names;
 std::size_t relative=0;
 bool quoted=false,selective=false;
};
// A source request, never a filesystem path or an admission certificate.
// Deliberately not part of the PPX Unit wire format.
struct KernelImport {
 Location where;
 std::string module,alias;
 bool is_unsafe=false;
};
struct NominalDeclaration {
 Location where;
 std::string name;
 Type::Kind kind=Type::Record;
 std::vector<std::string> parameters,labels;
 std::vector<Type> fields; // Bound indices refer to parameters; no inferred layout.
};
// Host-retained declaration-extension requests, never accepted from ordinary
// Unit-v1 PPX output. They are proposals, not cached typing/ownership proofs.
struct ExtensionMember { std::string name; Type scheme; Location where; };
struct ExtensionContract {
 std::string name;
 std::vector<ExtensionMember> members;
 Location where;
};
struct ExtensionBinding {
 std::string member,function,selector;
 std::string function_id,selector_id; // Stable origins; loader names may change.
 Location where;
};
struct ExtensionImplementation {
 std::string name,contract;
 std::vector<Type> arguments;
 bool canonical=false;
 std::vector<ExtensionBinding> bindings;
 Location where;
};
struct ExtensionEvidence {
 std::string owner,origin,version,program,schema;
 std::string binary_hash,schema_hash,compiler_hash,registry_hash;
 unsigned timeout_ms=5000;
 std::vector<ExtensionContract> contracts;
 std::vector<ExtensionImplementation> implementations;
};
struct Unit {
  std::vector<std::string> imports; std::vector<Function> functions;
  std::vector<Import> module_imports;
  std::vector<KernelImport> kernel_imports;
  std::vector<FeatureActivation> features;
  std::vector<ModuleExtension> extensions;
  std::vector<NominalDeclaration> nominals;
  std::vector<ExtensionEvidence> extension_evidence;
  std::map<std::string,std::string> source_digests;
 std::map<Type,std::vector<Type>> forall_layouts; // Internal whole-unit demand ABI.
 bool forall_elaborated=false;
 bool stages_lowered=false;
 std::vector<Macro> macros;
 std::vector<PpxRegistration> ppx;
};
std::string read_source_file(const std::string& filename);
std::vector<Token> scan(const std::string& source, const std::string& filename);
Unit parse(const std::vector<Token>& tokens,const std::vector<std::string>& enabled={});
void require_no_kernel_imports(const Unit&);
bool is_intrinsic_name(const std::string& name);
std::vector<Type> extern_parameter_types(const Function& function);
void check(Unit& unit);
std::string emit_c(const Unit& unit, const std::string& source_name);
}
