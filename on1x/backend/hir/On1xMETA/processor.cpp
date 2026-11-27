#include <on1x/processor.hpp>
#include <on1x/clone.hpp>
#include <on1x/meta_schema.hpp>
#include <on1x_meta_rules.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <string_view>

namespace on1x::processor {
namespace {
using E=Expr::Kind;
using S=Stmt::Kind;
using N=meta::NodeKind;
namespace table=meta::generated;

[[noreturn]] void fail(const Location& at,const std::string& message) {
  throw Error(at,"On1xMETA: "+message,"ON1X-PROCESSOR");
}
[[noreturn]] void exhausted(const Location& at,const char* resource) {
  throw Error(at,std::string("On1xMETA ")+resource+" budget exhausted",
              "ON1X-PROCESSOR-LIMIT");
}
meta::Phase phase_of(Phase phase) {
  switch(phase) {
    case Phase::Desugar:return meta::Phase::Desugar;
    case Phase::Canonicalize:return meta::Phase::Canonicalize;
    case Phase::Semantic:return meta::Phase::Semantic;
  }
  fail({},"unknown processor phase");
}

class Engine {
  Limits limits;
  Stats stats;
  Stats* canonical=nullptr;
  bool canonicalized=false, folded_identity=false;
  std::size_t canonical_count=0, total_rewrites=0;
  std::size_t steps=0, nodes=0, serial=0;
  std::set<std::string> names;
  std::vector<const meta::Rule*> rules;
  // Every checked pattern has a structural root. Preserve authored order in
  // each bucket without trying every canonical/desugar rule on every leaf.
  std::map<std::pair<E,std::string_view>,std::vector<std::size_t>> by_root;
  using Captures=std::map<std::string,ExprPtr*>;

  void step(const Location& at,std::size_t depth) {
    if(depth>limits.depth)exhausted(at,"depth");
    if(steps>=limits.steps)exhausted(at,"work");
    ++steps;
  }
  void allocate(const Location& at) {
    if(nodes>=limits.nodes)exhausted(at,"node");
    ++nodes;
  }
  const meta::Node& node(std::uint32_t index,const Location& at) {
    if(index>=table::nodes.size())fail(at,"invalid compiled node index");
    const auto& result=table::nodes[index];
    if(result.arity>result.children.size())fail(at,"invalid compiled node arity");
    return result;
  }
  void pattern_names(const Pattern& pattern,std::size_t depth) {
    step(pattern.where,depth);allocate(pattern.where);
    if(!pattern.name.empty())names.insert(pattern.name);
    for(const auto& child:pattern.elements)pattern_names(child,depth+1);
  }
  void parameters(const std::vector<Param>& params,std::size_t depth) {
    for(const auto& param:params) {
      step(param.where,depth);
      names.insert(param.name);
      if(param.pattern)pattern_names(*param.pattern,depth+1);
    }
  }
  void inventory(const MacroParameter& p,std::size_t depth) {
    step(p.where,depth);allocate(p.where);
    if(p.literal)inventory(*p.literal,depth+1);
    for(const auto& child:p.elements)inventory(child,depth+1);
    if(p.tail)inventory(*p.tail,depth+1);
  }
  void inventory(const Macro& macro,std::size_t depth) {
    step(macro.where,depth);allocate(macro.where);
    for(const auto& rule:macro.rules) {
      step(rule.where,depth+1);
      for(const auto& parameter:rule.parameters)inventory(parameter,depth+2);
      if(rule.tail)inventory(*rule.tail,depth+2);
      if(rule.expression)inventory(*rule.expression,depth+2);
      inventory(rule.body,depth+2);
    }
  }
  // Before cloning, bound executable/proof tree depth and reserve every lexical
  // spelling, including private names introduced by earlier canonical passes.
  void inventory(const Expr& e,std::size_t depth) {
    step(e.where,depth);allocate(e.where);
    names.insert(e.text);
    parameters(e.parameters,depth+1);
    for(const auto& capture:e.captures)names.insert(capture.name);
    for(const auto& pattern:e.patterns)pattern_names(pattern,depth+1);
    for(const auto& macro:e.syntax_definitions)inventory(macro,depth+1);
    if(e.callee)inventory(*e.callee,depth+1);
    if(e.syntax_tail)inventory(*e.syntax_tail,depth+1);
    for(const auto& child:e.children) {
      if(!child)fail(e.where,"missing expression child");
      inventory(*child,depth+1);
    }
    for(const auto& proof:e.proofs) {
      if(!proof)fail(e.where,"missing universal proof");
      inventory(*proof,depth+1);
    }
    inventory(e.body,depth+1);
  }
  void inventory(const Stmt& s,std::size_t depth) {
      step(s.where,depth);allocate(s.where);names.insert(s.name);
      if(s.pattern)pattern_names(*s.pattern,depth+1);
      if(s.value)inventory(*s.value,depth+1);
      inventory(s.body,depth+1);inventory(s.otherwise,depth+1);
      for(const auto& instance:s.instances) {
        if(!instance)fail(s.where,"missing generic instance");
        // Instances are checked evidence, not rewrite targets.
        inventory(*instance,depth+1);
      }
  }
  void inventory(const std::vector<Stmt>& body,std::size_t depth) {
    for(const auto& s:body)inventory(s,depth);
  }
  void inventory(const Function& f) {
    names.insert(f.name);parameters(f.params,0);
    inventory(f.body,0);
    for(const auto& attribute:f.attributes) {
      if(!attribute)fail(f.where,"missing function attribute");
      inventory(*attribute,0);
    }
  }
  void inventory(const Unit& unit) {
    for(const auto& f:unit.functions)inventory(f);
    for(const auto& macro:unit.macros)inventory(macro,0);
    for(const auto& implementation:unit.cliche_implementations)
      for(const auto& member:implementation.members) {
        inventory(member.function);
        if(member.value)inventory(*member.value,0);
      }
    for(const auto& implementation:unit.inherent_implementations)
      for(const auto& member:implementation.members) {
        inventory(member.function);
        if(member.value)inventory(*member.value,0);
      }
  }
  std::string fresh() {
    std::string name;
    do {name="@processor_"+std::to_string(++serial);} while(names.contains(name));
    names.insert(name);return name;
  }
  bool match(std::uint32_t index,ExprPtr& expression,Captures& captures,
             std::size_t depth) {
    if(!expression)return false;
    auto& e=*expression;
    step(e.where,depth);
    const auto& n=node(index,e.where);
    if(e.prototype||e.type==Type::Invalid||e.type.has_holes()||e.type.has_generics())
      return false;
    if(n.kind==N::Capture) {
      if(n.arity||!captures.emplace(n.text,&expression).second)
        fail(e.where,"nonlinear compiled match capture");
      return true;
    }
    E kind;
    switch(n.kind) {
      case N::Unary:kind=E::Unary;break;
      case N::Binary:kind=E::Binary;break;
      case N::Boolean:kind=E::Boolean;break;
      case N::Integer:kind=E::Integer;break;
      case N::Unit:kind=E::Unit;break;
      case N::Select:fail(e.where,"Select is a replacement-only constructor");
      case N::Capture:fail(e.where,"invalid capture");
    }
    if(e.kind!=kind||e.text!=n.text||e.children.size()!=n.arity)return false;
    // Literals are exact typed values, never float/unsigned textual aliases.
    if((kind==E::Boolean&&e.type!=Type::Bool)||
       (kind==E::Integer&&e.type!=Type::I64)||
       (kind==E::Unit&&e.type!=Type::Void))return false;
    for(std::size_t i=0;i<n.arity;++i)
      if(!match(n.children[i],e.children[i],captures,depth+1))return false;
    return true;
  }
  bool guard(meta::Guard guard,const Captures& captures) {
    for(const auto& [name,slot]:captures) {
      (void)name;
      const auto& e=**slot;
      switch(guard) {
        case meta::Guard::Boolean:if(e.type!=Type::Bool)return false;break;
        case meta::Guard::Integer:if(e.type!=Type::I64)return false;break;
        case meta::Guard::Always:break;
        // Pure deliberately means literal here. Checked arithmetic may trap;
        // even a local read may transfer an owner or access a borrowed place.
        case meta::Guard::Pure:
          if(e.kind!=E::Boolean&&e.kind!=E::Integer&&e.kind!=E::Unit)return false;
          break;
      }
    }
    return true;
  }
  ExprPtr expression(E kind,Type type,const Location& at,std::string text={}) {
    allocate(at);
    auto result=std::make_unique<Expr>();
    result->kind=kind;result->type=type;result->where=at;result->text=std::move(text);
    return result;
  }
  Stmt statement(S kind,const Location& at) {
    allocate(at);Stmt result;result.kind=kind;result.where=at;return result;
  }
  ExprPtr construct(std::uint32_t index,Captures& captures,const Location& at,
                    std::size_t depth) {
    step(at,depth);
    const auto& n=node(index,at);
    if(n.kind==N::Capture) {
      auto found=captures.find(n.text);
      if(found==captures.end()||!found->second||!*found->second)
        fail(at,"missing or duplicated replacement capture");
      auto result=std::move(*found->second);
      found->second=nullptr;
      return result;
    }
    std::vector<ExprPtr> children;
    for(std::size_t i=0;i<n.arity;++i)
      children.push_back(construct(n.children[i],captures,at,depth+1));
    if(n.kind==N::Select) {
      if(children.size()!=3||std::any_of(children.begin(),children.end(),
          [](const auto& e){return e->type!=Type::Bool;}))
        fail(at,"Select requires three checked booleans");
      // An ordinary typed value region, not a new HIR node or a runtime helper.
      // No operand is copied, hoisted or evaluated in the unselected branch.
      auto result=expression(E::Region,Type::Bool,at);
      auto name=fresh();
      auto storage=statement(S::Storage,at);
      storage.name=name;storage.declared=Type::Bool;
      result->body.push_back(std::move(storage));
      auto branch=statement(S::If,at);branch.value=std::move(children[0]);
      auto yes=statement(S::Initialize,at);yes.name=name;
      yes.value=std::move(children[1]);branch.body.push_back(std::move(yes));
      auto no=statement(S::Initialize,at);no.name=name;
      no.value=std::move(children[2]);branch.otherwise.push_back(std::move(no));
      result->body.push_back(std::move(branch));
      auto tail=expression(E::Variable,Type::Bool,at,name);
      tail->reference=Expr::Reference::Local;
      result->children.push_back(std::move(tail));
      return result;
    }
    E kind;Type type;
    const std::string_view text=n.text;
    switch(n.kind) {
      case N::Boolean:
        if(n.arity||(text!="true"&&text!="false"))fail(at,"invalid boolean constructor");
        kind=E::Boolean;type=Type::Bool;break;
      case N::Integer:
        if(n.arity)fail(at,"invalid integer constructor");
        kind=E::Integer;type=Type::I64;break;
      case N::Unit:
        if(n.arity)fail(at,"invalid unit constructor");
        kind=E::Unit;type=Type::Void;break;
      case N::Unary:
        if(n.arity!=1)fail(at,"invalid unary constructor");
        kind=E::Unary;type=children[0]->type;
        if(text=="!") {
          if(type!=Type::Bool)fail(at,"logical negation requires bool");
        } else if(text!="-"||type!=Type::I64)fail(at,"unsupported unary constructor");
        break;
      case N::Binary: {
        if(n.arity!=2||children[0]->type!=children[1]->type)
          fail(at,"binary constructor operand type mismatch");
        kind=E::Binary;type=children[0]->type;
        const bool equality=text=="=="||text=="!=";
        const bool order=text=="<"||text==">"||text=="<="||text==">=";
        const bool logical=text=="&&"||text=="||";
        const bool arithmetic=text=="+"||text=="-"||text=="*"||text=="/"||text=="%";
        if(equality) {
          if(type!=Type::Bool&&type!=Type::I64)fail(at,"unsupported equality constructor");
          type=Type::Bool;
        } else if(order) {
          if(type!=Type::I64)fail(at,"order constructor requires i64");
          type=Type::Bool;
        } else if(logical) {
          if(type!=Type::Bool)fail(at,"logical constructor requires bool");
        } else if(!arithmetic||type!=Type::I64)
          fail(at,"unsupported arithmetic constructor");
        break;
      }
      case N::Capture:case N::Select:fail(at,"invalid compiled constructor");
    }
    auto result=expression(kind,type,at,std::string(text));
    result->children=std::move(children);
    return result;
  }
  void children(Expr& e,std::size_t depth) {
    if(e.callee)rewrite(e.callee,depth+1);
    for(auto& child:e.children)rewrite(child,depth+1);
    rewrite(e.body,depth+1);
    // Universality proofs, syntax and prototype instances retain their original
    // checked evidence; they are never runtime operands or rewrite targets.
  }
  void rewrite(ExprPtr& e,std::size_t depth) {
    if(!e)return;
    step(e->where,depth);++stats.visited;
    if(canonical)++canonical->visited;
    if(e->prototype)return;
    children(*e,depth);
    for(;;) {
      const auto bucket=by_root.find({e->kind,e->text});
      if(bucket==by_root.end())break;
      bool changed=false;
      for(const auto i:bucket->second) {
        const auto& rule=*rules[i];Captures captures;
        if(!match(rule.match,e,captures,depth)||!guard(rule.guard,captures))continue;
        if(total_rewrites>=limits.rewrites)exhausted(e->where,"rewrite");
        const auto at=e->where;const auto type=e->type;
        const bool identity=node(rule.replacement,at).kind==N::Capture;
        // A redex whose operands all share its source location can be
        // lowering scaffolding (e.g. a generated range-loop guard). Still
        // rewrite it, but require distinct provenance for an exported fact.
        const bool located=e->children.empty()||std::any_of(e->children.begin(),e->children.end(),[&](const auto& child) {
          const auto& where=child->where;
          return where.file!=at.file||where.offset!=at.offset;
        });
        auto replacement=construct(rule.replacement,captures,at,depth);
        for(const auto& [name,slot]:captures)
          if(slot)fail(at,"replacement dropped capture "+name);
        if(replacement->type!=type)fail(at,"replacement changes checked result type");
        e=std::move(replacement);
        canonicalized|=located&&(rule.phase==meta::Phase::Canonicalize||identity);
        folded_identity|=located&&identity;
        ++total_rewrites;
        auto& hits=i<canonical_count?*canonical:stats;
        ++hits.rewrites;++hits.rules[i<canonical_count?i:i-canonical_count].second;
        changed=true;
        // Normalize newly constructed children before trying the root again.
        children(*e,depth);
        break;
      }
      if(!changed)break;
    }
  }
  void rewrite(std::vector<Stmt>& body,std::size_t depth) {
    for(auto& s:body) {
      step(s.where,depth);++stats.visited;
      if(canonical)++canonical->visited;
      if(s.prototype||s.kind==S::Template)continue;
      if(s.value)rewrite(s.value,depth+1);
      rewrite(s.body,depth+1);rewrite(s.otherwise,depth+1);
    }
  }
public:
  Engine(Phase phase,const Limits& requested,const RuleSelection& selection,Stats* normalize=nullptr)
      :limits(requested),canonical(normalize) {
    limits.nodes=std::min<std::size_t>(limits.nodes,table::budgets.nodes);
    limits.steps=std::min<std::size_t>(limits.steps,table::budgets.steps);
    limits.depth=std::min<std::size_t>(limits.depth,table::budgets.depth);
    auto add=[&](Phase family,Stats& hits){
      for(const auto& rule:table::rules)if(rule.phase==phase_of(family)&&selection.enabled(rule.name)){
        const auto& root=node(rule.match,{});
        E kind;
        switch(root.kind){
          case N::Unary:kind=E::Unary;break;
          case N::Binary:kind=E::Binary;break;
          case N::Boolean:kind=E::Boolean;break;
          case N::Integer:kind=E::Integer;break;
          case N::Unit:kind=E::Unit;break;
          default:fail({},"invalid compiled match root");
        }
        by_root[{kind,root.text}].push_back(rules.size());
        rules.push_back(&rule);hits.rules.emplace_back(rule.name,0);
      }
    };
    if(canonical){add(Phase::Canonicalize,*canonical);canonical_count=rules.size();}
    add(phase,stats);
  }
  Stats run(Unit& unit) {
    if(rules.empty())return std::move(stats);
    inventory(unit);
    auto clone_budget=limits.nodes;
    Unit working;
    try {working=clone_unit(unit,clone_budget);}
    catch(const Error& error) {
      if(error.diagnostic_id=="ON1X-AST-LIMIT")exhausted(error.where,"clone node");
      throw;
    }
    for(auto& f:working.functions)if(!f.prototype&&!f.external) {
      canonicalized=folded_identity=false;
      rewrite(f.body,0);
      f.meta_canonicalized|=canonicalized;
      f.meta_folded_identity|=folded_identity;
    }
    // Re-count the completed tree with the same depth limit, before publishing.
    nodes=0;
    inventory(working);
    unit=std::move(working);
    return std::move(stats);
  }
};
}
Stats apply(Unit& unit,Phase phase,const Limits& limits) {
  return apply(unit,phase,limits,RuleSelection{});
}
Stats apply(Unit& unit,Phase phase,const Limits& limits,const RuleSelection& selection) {
  return Engine(phase,limits,selection).run(unit);
}
std::vector<std::string_view> rule_names() {
  std::vector<std::string_view> result;
  for(const auto& rule:table::rules)result.emplace_back(rule.name);
  return result;
}
NormalizationStats desugar_normalized(Unit& unit,const Limits& limits,const RuleSelection& selection) {
  NormalizationStats result;
  result.desugar=Engine(Phase::Desugar,limits,selection,&result.canonicalize).run(unit);
  return result;
}
}
