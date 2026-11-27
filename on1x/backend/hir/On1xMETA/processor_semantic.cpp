#include <on1x/processor_semantic.hpp>
#include <on1x/meta_schema.hpp>
#include <on1x_meta_rules.hpp>
#include <algorithm>
#include <charconv>
#include <map>
#include <sstream>
#include <string_view>
#include <type_traits>

namespace on1x::processor {
namespace {
using E=Expr::Kind;
using S=Stmt::Kind;
using Constraint=meta::Constraint;
std::string_view name(E kind){
#define KIND(k) case E::k:return #k;
 switch(kind){
  KIND(Integer) KIND(Float) KIND(Boolean) KIND(String) KIND(Variable)
  KIND(ClicheMember) KIND(InherentMember) KIND(Unary) KIND(Binary) KIND(Call)
  KIND(Lambda) KIND(Tuple) KIND(PolyPack) KIND(PolyProjection)
  KIND(MacroCall) KIND(SyntaxHole) KIND(SyntaxBlock) KIND(Extension)
  KIND(SyntaxList) KIND(SyntaxVector) KIND(SyntaxKeyword) KIND(Quote)
  KIND(Splice) KIND(Lift) KIND(Run) KIND(SyntaxLet) KIND(Record)
  KIND(Variant) KIND(Match) KIND(Array) KIND(Field) KIND(Index)
  KIND(Slice) KIND(Length) KIND(PatternMatch) KIND(Case) KIND(Unit)
  KIND(Region) KIND(TagTest) KIND(Payload) KIND(Project) KIND(Loop)
  KIND(Propagate) KIND(SourceCsp) KIND(Csp) KIND(UnsafeBlock)
  KIND(Character) KIND(ByteBuffer) KIND(Interpolated) KIND(RegexLiteral)
  KIND(RegexMatch) KIND(RegexCapture) KIND(Borrow)
 }
#undef KIND
 return {};
}
std::string_view name(S kind){
#define KIND(k) case S::k:return "Stmt" #k;
 switch(kind){
  KIND(Let) KIND(Assign) KIND(Return) KIND(If) KIND(While) KIND(Expr)
  KIND(Block) KIND(Template) KIND(Range) KIND(Break) KIND(Continue)
  KIND(Storage) KIND(Initialize) KIND(Region) KIND(Leave) KIND(Trap)
  KIND(Defer) KIND(PushDefer) KIND(TaskGroup) KIND(CspScope)
 }
#undef KIND
 return {};
}
[[noreturn]] void malformed(const Location& at,const std::string& message){
 throw Error(at,message,"ON1X-PROCESSOR-SEMANTIC");
}
[[noreturn]] void reject(const Constraint& rule,const Location& at){
 malformed(at,std::string(rule.message)+" [semantic rule "+rule.name+" at "+
           rule.file+":"+std::to_string(rule.line)+":"+std::to_string(rule.column)+"]");
}
class Validator {
 SemanticStage stage;
 Limits limits;
 SemanticStats stats;
 std::size_t steps=0;
 std::map<std::string_view,std::vector<std::size_t>> by_kind;
 std::vector<std::size_t> hits;
 void work(const Location& at){
  if(steps==limits.steps)
   throw Error(at,"semantic processor work budget exhausted","ON1X-PROCESSOR-LIMIT");
  ++steps;
 }
 void tick(const Location& at,std::size_t depth){
  work(at);
  if(depth>limits.depth||stats.visited==limits.nodes)
   throw Error(at,"semantic processor depth/node budget exhausted","ON1X-PROCESSOR-LIMIT");
  ++stats.visited;
  if(at.file.empty()||at.file.size()>4096||!at.line||!at.column||
     at.offset>max_source_bytes||at.line>max_source_bytes+1||at.column>max_source_bytes+1)
   malformed(at,"invalid semantic processor source location");
 }
 bool typed(const Constraint& rule)const{
  const std::string_view property=rule.property;
  return property=="type"||property=="resolved"||property=="operands"||
         property=="result";
 }
 std::size_t count(const Constraint& rule,const Location& at)const{
  std::size_t value=0;const std::string_view text=rule.value;
  const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
  if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size())
   malformed(at,"invalid compiled semantic count");
  return value;
 }
 bool expression_rule(const Constraint& rule,const Expr& e)const{
  const std::string_view property=rule.property,value=rule.value;
  if(property=="arity")return e.children.size()==count(rule,e.where);
  if(property=="min_arity")return e.children.size()>=count(rule,e.where);
  if(property=="callee")return bool(e.callee)==(value=="present");
  if(property=="body")return (!e.body.empty())==(value=="present");
  if(property=="forbidden")return false;
  if(property=="resolved")return e.reference!=Expr::Reference::Unresolved;
  if(property=="type")
   return e.type==(value=="bool"?Type::Bool:value=="i64"?Type::I64:Type::Void);
  if(property=="operands"){
   if(e.children.empty())return false;
   for(const auto& child:e.children){
    if(!child||child->type==Type::Invalid)return false;
    if(value=="same"&&child->type!=e.children[0]->type)return false;
    if(value=="boolean"&&child->type!=Type::Bool)return false;
    if(value=="numeric"&&!child->type.is_numeric())return false;
   }
   return true;
  }
  if(property=="result")
   return !e.children.empty()&&e.children[0]&&e.type!=Type::Invalid&&
          e.type==e.children[0]->type;
  malformed(e.where,"unknown compiled expression semantic property: "+std::string(property));
 }
 bool statement_rule(const Constraint& rule,const Stmt& s)const{
  const std::string_view property=rule.property,value=rule.value;
  if(property=="value"){
   if(value=="boolean")
    return s.value&&(stage==SemanticStage::Structural||s.value->type==Type::Bool);
   return bool(s.value)==(value=="present");
  }
  if(property=="body")return (!s.body.empty())==(value=="present");
  if(property=="forbidden")return false;
  malformed(s.where,"unknown compiled statement semantic property: "+std::string(property));
 }
 template<class Node> void constraints(const Node& node){
  const auto kind=name(node.kind);
  if(kind.empty())malformed(node.where,"unknown semantic processor node kind");
  const auto found=by_kind.find(kind);
  if(found==by_kind.end())return;
  for(auto index:found->second){
   const auto& rule=meta::generated::constraints[index];
   if(stage==SemanticStage::Structural&&typed(rule))continue;
   work(node.where);++stats.checks;++hits[index];
   bool valid;
   if constexpr(std::is_same_v<Node,Expr>)valid=expression_rule(rule,node);
   else valid=statement_rule(rule,node);
   if(!valid)reject(rule,node.where);
  }
 }
 void pattern(const Pattern& p,std::size_t depth){
  tick(p.where,depth);
  for(const auto& child:p.elements)pattern(child,depth+1);
 }
 void parameters(const std::vector<Param>& ps,std::size_t depth){
  for(const auto& p:ps){tick(p.where,depth);if(p.pattern)pattern(*p.pattern,depth+1);}
 }
 void expression(const Expr& e,std::size_t depth){
  tick(e.where,depth);
  // Retained inference proofs/prototypes have their own authoritative checker.
  // Structural checks still inspect them, but they are not executable typed HIR.
  if(stage==SemanticStage::Typed&&e.prototype)return;
  for(const auto& child:e.children)if(!child)malformed(e.where,"missing semantic expression operand");
  for(const auto& proof:e.proofs)if(!proof)malformed(e.where,"missing semantic universal proof");
  constraints(e);
  if(e.callee)expression(*e.callee,depth+1);
  for(const auto& child:e.children)expression(*child,depth+1);
  if(stage==SemanticStage::Structural)
   for(const auto& proof:e.proofs)expression(*proof,depth+1);
  for(const auto& p:e.patterns)pattern(p,depth+1);
  parameters(e.parameters,depth+1);
  body(e.body,depth+1);
 }
 void statement(const Stmt& s,std::size_t depth){
  tick(s.where,depth);
  if(stage==SemanticStage::Typed&&(s.prototype||s.kind==S::Template))return;
  constraints(s);
  if(s.pattern)pattern(*s.pattern,depth+1);
  if(s.value)expression(*s.value,depth+1);
  body(s.body,depth+1);body(s.otherwise,depth+1);
  for(const auto& instance:s.instances){
   if(!instance)malformed(s.where,"missing semantic statement instance");
   statement(*instance,depth+1);
  }
 }
 void body(const std::vector<Stmt>& statements,std::size_t depth){
  for(const auto& s:statements)statement(s,depth);
 }
public:
 Validator(SemanticStage selected,const Limits& requested):stage(selected),limits(requested),
  hits(meta::generated::constraints.size()){
  limits.nodes=std::min<std::size_t>(limits.nodes,meta::generated::budgets.nodes);
  limits.steps=std::min<std::size_t>(limits.steps,meta::generated::budgets.steps);
  limits.depth=std::min<std::size_t>(limits.depth,meta::generated::budgets.depth);
  for(std::size_t i=0;i<meta::generated::constraints.size();++i)
   by_kind[meta::generated::constraints[i].kind].push_back(i);
 }
 SemanticStats run(const Unit& unit){
  for(const auto& f:unit.functions){
   tick(f.where,0);
   if(stage==SemanticStage::Typed&&f.prototype)continue;
   parameters(f.params,1);body(f.body,1);
  }
  for(std::size_t i=0;i<hits.size();++i)
   if(hits[i])stats.rules.emplace_back(meta::generated::constraints[i].name,hits[i]);
  return std::move(stats);
 }
};
std::string quoted(std::string_view text){
 std::string out="\"";constexpr char hex[]="0123456789abcdef";
 for(unsigned char c:text){
  if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}
  else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
  else out+=static_cast<char>(c);
 }
 return out+'"';
}
void provenance(std::ostream& out,std::string_view name,bool semantic){
 if(semantic){
  for(const auto& rule:meta::generated::constraints)if(name==rule.name){
   out<<",\"file\":"<<quoted(rule.file)<<",\"line\":"<<rule.line<<",\"column\":"<<rule.column;
   return;
  }
 }else for(const auto& rule:meta::generated::rules)if(name==rule.name){
  out<<",\"file\":"<<quoted(rule.file)<<",\"line\":"<<rule.line<<",\"column\":"<<rule.column;
  return;
 }
}
template<class Statistics> void statistics(std::ostream& out,const Statistics& stats){
 constexpr bool semantic=std::is_same_v<Statistics,SemanticStats>;
 out<<"{\"visited\":"<<stats.visited;
 if constexpr(semantic)out<<",\"checks\":"<<stats.checks;
 else out<<",\"rewrites\":"<<stats.rewrites;
 out<<",\"rules\":[";
 bool first=true;
 for(const auto& [rule,hits]:stats.rules){
  if(!first)out<<',';
  first=false;
  out<<"{\"name\":"<<quoted(rule)<<",\"count\":"<<hits;
  provenance(out,rule,semantic);out<<'}';
 }
 out<<"]}";
}
void source_statistics(std::ostream& out,const meta::SourceDesugarReport& stats){
 out<<"{\"program_sha256\":"<<quoted(stats.program_sha256)
    <<",\"verified\":"<<(stats.verified?"true":"false")
    <<",\"rewrites\":"<<stats.rewrites<<",\"definitions\":"<<stats.definitions
    <<",\"fixities\":"<<stats.fixities<<",\"rules\":[";
 bool first=true;
 for(const auto& rule:stats.rules){
  if(!first)out<<',';
  first=false;
  out<<"{\"name\":"<<quoted(rule.name)<<",\"count\":"<<rule.count
     <<",\"file\":"<<quoted(rule.file)<<",\"line\":"<<rule.line<<",\"column\":"<<rule.column<<'}';
 }
 out<<"]}";
}
}
SemanticStats validate(const Unit& unit,SemanticStage stage,const Limits& limits){
 return Validator(stage,limits).run(unit);
}
std::string report(const Report& result){
 std::ostringstream out;
 out<<"{\"format\":\"on1x.processor\",\"version\":"<<meta::generated::schema_version
    <<",\"program_sha256\":"<<quoted(meta::generated::program_sha256)
    <<",\"parser_sha256\":"<<quoted(meta::generated::parser_sha256)
    <<",\"verified\":"<<(result.verified?"true":"false")<<",\"source_desugar\":";
  source_statistics(out,result.source_desugar);out<<",\"structural\":";
 statistics(out,result.structural);out<<",\"typed_before\":";
 statistics(out,result.typed_before);out<<",\"desugar\":";
 statistics(out,result.desugar);out<<",\"canonicalize\":";
 statistics(out,result.canonicalize);out<<",\"typed_after\":";
 statistics(out,result.typed_after);
 const auto& pe=result.partial_eval;
 out<<",\"partial_eval\":{\"enabled\":"<<(pe.enabled?"true":"false")
    <<",\"limited\":"<<(pe.limited?"true":"false")<<",\"work\":"<<pe.work
    <<",\"folds\":"<<pe.folds<<",\"static_calls\":"<<pe.static_calls
     <<",\"residual_calls\":"<<pe.residual_calls<<'}';
  const auto& novo=result.novostone_hir;
  if(novo.enabled)
   out<<",\"novostone_hir\":{\"enabled\":true,\"limited\":"<<(novo.limited?"true":"false")
      <<",\"attempted\":"<<novo.attempted<<",\"applied\":"<<novo.applied
      <<",\"specialized\":"<<novo.specialized<<",\"obligations\":"<<novo.obligations
      <<",\"smt_proved\":"<<novo.smt_proved<<'}';
  const auto& fire=result.firestone_hir;
  if(fire.enabled)
   out<<",\"firestone_hir\":{\"enabled\":true,\"limited\":"<<(fire.limited?"true":"false")
      <<",\"visited\":"<<fire.visited<<",\"folds\":"<<fire.folds
      <<",\"divide_one\":"<<fire.divide_one<<",\"applied\":"<<fire.folds+fire.divide_one<<'}';
  out<<"}\n";
 return out.str();
}
}
