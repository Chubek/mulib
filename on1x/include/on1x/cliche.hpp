#pragma once
#include <on1x/ast.hpp>

namespace on1x::cliches {

// Compiler-side proof search limits. Exhaustion is distinct from an
// unsatisfied or cyclic obligation.
struct ResolutionLimits {
 std::size_t work=4096;
 std::size_t depth=64;
};

struct Selection {
 const ClicheImplementation* implementation=nullptr;
 const ClicheImplementationMember* member=nullptr;
 std::vector<Type> implementation_arguments;
};

bool present(const Unit&);
const ClicheDeclaration& declaration(const Unit&,const std::string&,const Location&);
const ClicheMember& member(const Unit&,const std::string&,const std::string&,const Location&);

Type substitute(const Type&,const std::vector<Type>&,const Location&);
ClicheConstraint substitute(const ClicheConstraint&,const std::vector<Type>&,const Location&);

// Validate declarations/coherence and append ordinary, statically named member
// functions used by inference and C11 lowering. Idempotent on a checked Unit.
void prepare(Unit&);

// Resolve one concrete predicate and all recursive prerequisites. No runtime
// name lookup is produced; the returned member names a compiler-generated
// ordinary function.
Selection resolve(const Unit&,const std::string&,const std::vector<Type>&,
                  const std::string&,const std::vector<Type>&,
                  const Location&,const ResolutionLimits& limits = {});

void require(const Unit&,const ClicheConstraint&,const Location&,
             const ResolutionLimits& limits = {});

// Checked non-prototype HIR must contain no abstract member projections.
void verify_elaborated(const Unit&);

} // namespace on1x::cliches
