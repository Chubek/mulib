#pragma once
#include <on1x/meta.hpp>
#include <functional>
#include <memory>
namespace on1x::meta {
using RepetitionPath=std::vector<std::size_t>;
struct MacroVariable {
 const MacroParameter* declaration=nullptr;
 std::size_t rank=0;
 std::map<RepetitionPath,const Expr*> values;
 std::map<RepetitionPath,std::size_t> widths;
};
struct MacroCaptures {
 std::map<std::string,MacroVariable> variables;
 std::vector<std::shared_ptr<Expr>> owned;
};
class MacroRules {
 const Schema& schema;
 std::function<bool(const Expr&)> locally_bound;
 std::function<ExprPtr(const Expr&)> clone;
 std::function<std::vector<Stmt>(const std::vector<Stmt>&)> clone_body;
 std::map<const MacroRule*,MacroCaptures> declarations;
 std::size_t work=0,metadata=4*1024*1024,active=0;
 void tick(const Location&,std::size_t amount=1);
 void charge(const Location&,std::size_t amount);
 MacroCaptures duplicate(const MacroCaptures&,const Location&);
 bool literal(const Expr&,const Expr&);
 bool match_one(const MacroParameter&,const Expr&,MacroCaptures&,RepetitionPath&);
 bool match_sequence(const std::vector<MacroParameter>&,const MacroParameter*,const std::vector<const Expr*>&,const Expr*,std::size_t,std::size_t,MacroCaptures&,RepetitionPath&,const Location&);
 void widths(const MacroParameter&,MacroCaptures&,const RepetitionPath&,std::size_t);
 std::size_t extent(const Expr&,const MacroCaptures&,const RepetitionPath&);
 const Expr& value(const std::string&,const MacroCaptures&,const RepetitionPath&,const Location&);
 void name(std::string&,std::size_t&,const MacroCaptures&,const RepetitionPath&,std::size_t,const Location&);
 void pattern(Pattern&,const MacroCaptures&,const RepetitionPath&,std::size_t);
 void element(const Expr&,const MacroCaptures&,RepetitionPath&,std::size_t,std::size_t,std::vector<ExprPtr>&);
public:
 MacroRules(const Schema& s,std::function<bool(const Expr&)> bound,std::function<ExprPtr(const Expr&)> copy,std::function<std::vector<Stmt>(const std::vector<Stmt>&)> bodies)
  :schema(s),locally_bound(std::move(bound)),clone(std::move(copy)),clone_body(std::move(bodies)){}
 std::string category(const Expr&)const;
 void validate(const MacroRule&);
 bool match(const MacroRule&,const Expr&,MacroCaptures&);
 void substitute(Expr&,const MacroCaptures&,std::size_t,RepetitionPath path={});
 void substitute_body(std::vector<Stmt>&,const MacroCaptures&,std::size_t,RepetitionPath path={});
};
}
