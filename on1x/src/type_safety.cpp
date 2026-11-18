#include <on1x/type_safety.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/concurrency.hpp>
#include <on1x/primitives.hpp>
#include <on1x/numeric.hpp>
#include <nerotypical/StructuralTyping.hpp>
#include <set>
#include <sstream>

namespace on1x::type_safety {
namespace nt=moonstone::nerotypical;
namespace {
using Term=nt::structural::TermId;
[[noreturn]] void fail(const Location& at,const std::string& text){throw Error(at,"ON1X-TYPE-SAFETY: "+text);}
[[noreturn]] void exhausted(const Location& at,const std::string& text){throw Inconclusive(at,"ON1X-TYPE-UNKNOWN: "+text);}
struct Budget {
 Limits limits;std::size_t used=0;
 void tick(const Location& at){if(used==limits.work)exhausted(at,"constraint construction work exhausted");++used;}
};
struct Binding {Type type;bool mut=false;std::size_t scope=0;};
class Verify {
 const Unit& unit;Budget& budget;nt::structural::TypeGraph graph;
 std::map<std::pair<std::string,std::vector<Term>>,Term> terms;
 std::map<Term,std::pair<std::size_t,std::size_t>> sizes;
 std::map<std::string,const Function*> functions;
 std::vector<std::map<std::string,Binding>> scopes;
 struct Frame {std::size_t scope;std::map<std::string,Type> needed;};
 std::vector<Frame> frames;
  Type returns=Type::Void;std::size_t constraints=0;
  std::vector<std::pair<std::size_t,bool>> regions;
 std::size_t nesting=0;
 struct Depth {
  std::size_t& value;
  Depth(std::size_t& n,const Location& at):value(n){if(++value>256)exhausted(at,"HIR nesting limit");}
  ~Depth(){--value;}
 };
 moonstone::SourceSpan span(const Location& at){
  if(at.file.empty()||at.file.size()>4096||!at.line||!at.column||at.line>UINT32_MAX||at.column>UINT32_MAX)fail(at,"invalid source point");
  return {at.file,static_cast<std::uint32_t>(at.line),static_cast<std::uint32_t>(at.column),static_cast<std::uint32_t>(at.line),static_cast<std::uint32_t>(at.column)};
 }
 Term app(std::string name,std::vector<Term> args,const Location& at){
  budget.tick(at);std::size_t depth=1,size=1;
  for(auto arg:args){depth=std::max(depth,sizes.at(arg).first+1);size+=sizes.at(arg).second;if(size>4096)exhausted(at,"expanded structural type limit");}
  if(depth>32)exhausted(at,"structural type depth limit");
  auto key=std::pair{name,args};if(auto found=terms.find(key);found!=terms.end())return found->second;
  if(graph.term_count()>=budget.limits.terms)exhausted(at,"type term budget exhausted");
  auto ctor=graph.find_constructor(name);
  if(!ctor){if(auto error=graph.add_constructor(name,static_cast<int>(args.size())))fail(at,*error);ctor=graph.find_constructor(name);}
  auto id=graph.apply(*ctor,std::move(args),span(at));terms.emplace(std::move(key),id);sizes[id]={depth,size};return id;
 }
 Term type(const Type& t,const Location& at,std::size_t depth=0){
  budget.tick(at);if(depth>32)exhausted(at,"structural type depth limit");
  std::vector<Term> args;std::string name;
   switch(t.kind()){
     case Type::F64:case Type::F32:case Type::U8:case Type::U16:case Type::U32:case Type::U64:
      name=type_name(t);break;
  case Type::I64:name="i64";break;case Type::Bool:name="bool";break;case Type::String:name="string";break;case Type::Void:name="void";break;
  case Type::Json:name="json";break;case Type::JsonView:name="json_view";break;case Type::SExpr:name="sexpr";break;case Type::Buffer:name="buffer";break;
  case Type::Function:
   name="fn"+std::to_string(t.parameters().size());
   for(const auto& p:t.parameters()){if(p==Type::Void)fail(at,"void parameter type");args.push_back(type(p,at,depth+1));}
   args.push_back(type(t.result(),at,depth+1));break;
  case Type::Tuple:
   if(t.elements().size()<2)fail(at,"invalid tuple type");
   name="tuple"+std::to_string(t.elements().size());for(const auto& e:t.elements()){if(e==Type::Void)fail(at,"void tuple element");args.push_back(type(e,at,depth+1));}break;
  case Type::Code:name="code";args.push_back(type(t.code_result(),at,depth+1));break;
  default:fail(at,"unresolved or quantified type is outside the structural adapter");
  }
  return app(std::move(name),std::move(args),at);
 }
 void equal(Term a,Term b,const Location& at){
  budget.tick(at);if(constraints==budget.limits.constraints)exhausted(at,"equality budget exhausted");++constraints;
  if(auto error=graph.add_equality(a,b,span(at),"On1x HIR typing obligation"))fail(at,*error);
 }
 void equal(Term a,const Type& b,const Location& at){equal(a,type(b,at),at);}
 void equal(Term a,Type::Kind b,const Location& at){equal(a,Type(b),at);}
 const Binding* lookup(const std::string& name)const{for(auto s=scopes.rbegin();s!=scopes.rend();++s)if(auto b=s->find(name);b!=s->end())return &b->second;return nullptr;}
 void bind(const std::string& name,const Type& t,bool mut,const Location& at){
  if(t==Type::Void)fail(at,"void local type");
  (void)type(t,at);if(!scopes.back().emplace(name,Binding{t,mut,scopes.size()-1}).second)fail(at,"duplicate local");
 }
 void pattern(const Pattern& p,const Type& t){
  Depth guard(nesting,p.where);
  budget.tick(p.where);if(!p.is_tuple()){bind(p.name,t,false,p.where);return;}
  if(!t.is_tuple()||p.elements.size()!=t.elements().size())fail(p.where,"pattern/type shape mismatch");
  for(std::size_t n=0;n<p.elements.size();++n)pattern(p.elements[n],t.elements()[n]);
 }
 void params(const std::vector<Param>& parameters){for(const auto& p:parameters){if(p.pattern)pattern(*p.pattern,p.type);else bind(p.name,p.type,false,p.where);}}
 Type signature(const Function& f){std::vector<Type> ps;for(const auto& p:f.params)ps.push_back(p.type);return Type::function(std::move(ps),f.result);}
 Term variable(const Expr& e,bool call){
  auto actual=type(e.type,e.where);
  if(auto b=lookup(e.text)){
   if(e.reference!=Expr::Reference::Local)fail(e.where,"incorrect local reference classification");
   for(auto& frame:frames)if(b->scope<frame.scope){if(b->mut)fail(e.where,"mutable closure capture");frame.needed.emplace(e.text,b->type);}
   equal(actual,b->type,e.where);return actual;
  }
  auto f=functions.find(e.text);if(f==functions.end())fail(e.where,"unresolved name");
  auto expected=f->second->external?Expr::Reference::Extern:Expr::Reference::Function;
  if(e.reference!=expected||(!call&&f->second->external))fail(e.where,"incorrect global reference classification");
  equal(actual,signature(*f->second),e.where);return actual;
 }
 Term intrinsic(const Expr& e,const std::vector<Term>& args){
  const auto& n=e.callee->text;auto at=e.where;
  auto arity=[&](std::size_t count){if(args.size()!=count)fail(at,"intrinsic arity mismatch");};
  if(auto p=data_primitive(n)){
   arity(p->parameters().size());for(std::size_t i=0;i<args.size();++i)equal(args[i],p->parameters()[i],at);return type(p->result(),at);
  }
  if(n=="println"){
    arity(1);auto t=e.children[0]->type;if(!t.is_numeric()&&t!=Type::Bool&&t!=Type::String)fail(at,"invalid println operand");return type(Type::Void,at);
  }
  if(n=="assert"){arity(1);equal(args[0],Type::Bool,at);return type(Type::Void,at);}
  if(n=="json_parse_or_die"){arity(1);equal(args[0],Type::String,at);return type(Type::Json,at);}
  if(n=="json_get_or_die"||n=="json_size"||n=="json_number"||n=="json_print"){
   arity(n=="json_get_or_die"?2:1);auto t=e.children[0]->type;
   if(t!=Type::Json&&t!=Type::JsonView)fail(at,"invalid JSON operand type");
   if(args.size()==2)equal(args[1],Type::String,at);
   return type(n=="json_get_or_die"?Type::JsonView:n=="json_print"?Type::Void:Type::I64,at);
  }
  fail(at,"unknown intrinsic");
 }
 Term expr(const Expr& e,std::size_t depth=0){
  Depth guard(nesting,e.where);
  budget.tick(e.where);if(depth>256)exhausted(e.where,"HIR depth limit");
  auto result=type(e.type,e.where);
  auto child=[&](std::size_t n){if(n>=e.children.size()||!e.children[n])fail(e.where,"missing expression child");return expr(*e.children[n],depth+1);};
  auto arity=[&](std::size_t n){if(e.children.size()!=n)fail(e.where,"expression arity mismatch");};
   switch(e.kind){
   case Expr::Kind::Unit:arity(0);equal(result,Type::Void,e.where);break;
   case Expr::Kind::Region:{if(e.children.size()!=1)fail(e.where,"invalid value region");scopes.push_back({});bool done=body(e.body,false,depth+1);equal(child(0),result,e.where);scopes.pop_back();if(done!=e.noreturn)fail(e.where,"incorrect value-region control");break;}
   case Expr::Kind::Project:{arity(1);auto t=e.children[0]->type;if(!t.is_tuple()||e.projection>=t.elements().size())fail(e.where,"invalid tuple projection");(void)child(0);equal(result,t.elements()[e.projection],e.where);break;}
   case Expr::Kind::TagTest:case Expr::Kind::Payload:fail(e.where,"closed-sum model is outside the structural adapter");
     case Expr::Kind::PatternMatch:case Expr::Kind::Case:case Expr::Kind::Loop:case Expr::Kind::UnsafeBlock:fail(e.where,"unlowered source value construct");
    case Expr::Kind::Integer:arity(0);(void)integer_literal_bits(e.text,e.where);equal(result,numeric_literal_type(e.text),e.where);break;
    case Expr::Kind::Float:arity(0);(void)float_literal_bits(e.text,e.where);equal(result,numeric_literal_type(e.text),e.where);break;
  case Expr::Kind::Boolean:arity(0);equal(result,Type::Bool,e.where);break;
  case Expr::Kind::String:arity(0);equal(result,Type::String,e.where);break;
  case Expr::Kind::Variable:arity(0);return variable(e,false);
    case Expr::Kind::Unary:{
     arity(1);if(e.text!="!"&&e.text!="-")fail(e.where,"unknown unary operator");
     auto operand=child(0);
     auto t=e.text=="!"?Type{Type::Bool}:e.children[0]->type;
     if(e.text=="-"&&(!t.is_numeric()||t.is_unsigned()))fail(e.where,"invalid unary numeric operand");
     equal(operand,t,e.where);equal(result,t,e.where);break;
   }
  case Expr::Kind::Binary:{
   arity(2);auto a=child(0),b=child(1);auto op=e.text;
   if(op=="=="||op=="!="){
     auto t=e.children[0]->type;if(!t.is_numeric()&&t!=Type::Bool&&t!=Type::String)fail(e.where,"invalid equality operand");equal(a,b,e.where);equal(result,Type::Bool,e.where);
   }else if(op=="&&"||op=="||"){equal(a,Type::Bool,e.where);equal(b,Type::Bool,e.where);equal(result,Type::Bool,e.where);}
   else{
    bool compare=op=="<"||op==">"||op=="<="||op==">=";
    if(!compare&&op!="+"&&op!="-"&&op!="*"&&op!="/"&&op!="%")fail(e.where,"unknown binary operator");
     auto t=e.children[0]->type;
     if(!t.is_numeric()||(op=="%"&&t.is_float()))fail(e.where,"invalid numeric operand type");
     equal(a,t,e.where);equal(b,t,e.where);equal(result,compare?Type{Type::Bool}:t,e.where);
   }break;
  }
  case Expr::Kind::Tuple:{std::vector<Term> es;for(std::size_t n=0;n<e.children.size();++n)es.push_back(child(n));if(es.size()<2)fail(e.where,"invalid tuple arity");equal(result,app("tuple"+std::to_string(es.size()),es,e.where),e.where);break;}
  case Expr::Kind::Call:{
   if(!e.callee)fail(e.where,"missing callee");
   std::vector<Term> args;for(std::size_t n=0;n<e.children.size();++n)args.push_back(child(n));
   const auto& callee=*e.callee;
   if(callee.reference==Expr::Reference::Intrinsic){
    if(callee.kind!=Expr::Kind::Variable||lookup(callee.text)||functions.contains(callee.text))fail(callee.where,"invalid intrinsic resolution");
    equal(result,intrinsic(e,args),e.where);
   }else{
    auto function=callee.kind==Expr::Kind::Variable?variable(callee,true):expr(callee,depth+1);
    auto name="fn"+std::to_string(args.size());args.push_back(result);equal(function,app(name,args,e.where),e.where);
   }break;
  }
  case Expr::Kind::Lambda:{
   std::vector<Type> ps;for(const auto& p:e.parameters)ps.push_back(p.type);equal(result,Type::function(ps,e.result),e.where);
   auto previous=returns;returns=e.result;frames.push_back({scopes.size(),{}});scopes.push_back({});params(e.parameters);
   auto complete=body(e.body,false);scopes.pop_back();auto needed=std::move(frames.back().needed);frames.pop_back();returns=previous;
   if(!complete&&e.result!=Type::Void)fail(e.where,"lambda lacks a return path");
   std::set<std::string> captured;
   for(const auto& capture:e.captures){if(!captured.insert(capture.name).second||!needed.contains(capture.name))fail(e.where,"incorrect closure capture metadata");equal(type(capture.type,e.where),needed.at(capture.name),e.where);}
   if(captured.size()!=needed.size())fail(e.where,"missing closure capture metadata");
   break;
  }
  case Expr::Kind::Quote:{arity(1);if(!unit.stages_lowered||!e.type.is_code())fail(e.where,"quotation is not stage-lowered");equal(child(0),Type::function({},e.type.code_result()),e.where);break;}
  case Expr::Kind::Run:{arity(1);equal(child(0),app("code",{result},e.where),e.where);break;}
  default:fail(e.where,"unlowered or quantified expression is outside the structural adapter");
  }
  return result;
 }
 bool body(const std::vector<Stmt>& statements,bool scoped=true,std::size_t depth=0){
  Depth guard(nesting,{"<type-hir>"});
  if(depth>256)exhausted({"<type-hir>"},"statement depth limit");
  if(scoped)scopes.push_back({});
  bool complete=false;
  for(const auto& s:statements){
   budget.tick(s.where);if(s.prototype)continue;if(complete)fail(s.where,"unreachable HIR statement");
   auto value=[&](){if(!s.value)fail(s.where,"missing statement value");return expr(*s.value);};
   switch(s.kind){
    case Stmt::Kind::Defer:case Stmt::Kind::PushDefer:fail(s.where,"deferred capture/effect model is outside the structural adapter");
   case Stmt::Kind::Let:{auto t=value();if(s.declared!=Type::Invalid)equal(t,s.declared,s.where);if(s.pattern)pattern(*s.pattern,s.value->type);else bind(s.name,s.value->type,s.mutable_binding,s.where);break;}
   case Stmt::Kind::Assign:{auto b=lookup(s.name);if(!b||!b->mut)fail(s.where,"assignment to missing/immutable local");equal(value(),b->type,s.where);break;}
   case Stmt::Kind::Return:equal(s.value?value():type(Type::Void,s.where),returns,s.where);complete=true;break;
    case Stmt::Kind::Expr:(void)value();complete=s.value->noreturn;break;
    case Stmt::Kind::Storage:bind(s.name,s.declared,false,s.where);break;
    case Stmt::Kind::Initialize:{auto b=lookup(s.name);if(!b)fail(s.where,"missing canonical storage");equal(value(),b->type,s.where);break;}
    case Stmt::Kind::Region:{regions.push_back({s.control_target,false});bool done=body(s.body,true,depth+1),joins=regions.back().second;regions.pop_back();complete=done&&!joins;break;}
    case Stmt::Kind::Leave:{auto target=std::find_if(regions.rbegin(),regions.rend(),[&](const auto& r){return r.first==s.control_target;});if(target==regions.rend())fail(s.where,"missing region target");target->second=true;complete=true;break;}
    case Stmt::Kind::Trap:complete=true;break;
   case Stmt::Kind::Block:complete=body(s.body,true,depth+1);break;
   case Stmt::Kind::Template:complete=body(s.body,false,depth+1);break;
   case Stmt::Kind::If:{equal(value(),Type::Bool,s.where);bool a=body(s.body,true,depth+1),b=body(s.otherwise,true,depth+1);complete=a&&b;break;}
   case Stmt::Kind::While:equal(value(),Type::Bool,s.where);(void)body(s.body,true,depth+1);(void)body(s.otherwise,true,depth+1);break;
   case Stmt::Kind::Break:case Stmt::Kind::Continue:complete=true;break;
   default:fail(s.where,"unknown statement");
   }
  }
  if(scoped)scopes.pop_back();
  return complete;
 }
public:
 Verify(const Unit& u,Budget& b):unit(u),budget(b){for(const auto& f:u.functions){budget.tick(f.where);if(!f.prototype)if(!functions.emplace(f.name,&f).second)fail(f.where,"duplicate function");}}
  std::string run(){
   canonical::verify_control(unit);
  if(functions.empty())fail({"<type-hir>"},"empty elaborated unit");
  for(const auto& [name,f]:functions){(void)name;(void)type(signature(*f),f->where);}
  for(const auto& f:unit.functions){
   if(f.prototype||f.external)continue;
   scopes.push_back({});returns=f.result;params(f.params);bool complete=body(f.body,false);scopes.pop_back();
   if(!complete&&f.result!=Type::Void)fail(f.where,"function lacks a return path");
  }
  nt::AnalysisOptions options;options.max_type_terms=budget.limits.terms;options.max_unification_rounds=budget.limits.rounds;options.allow_recursive_types=false;
  auto answer=graph.solve(options);
  if(answer.base.status==nt::AnalysisStatus::Unknown)exhausted({"<type-hir>"},"Nerotypical unification did not decide");
   if(!answer.base.well_typed()){
   if(!answer.base.diagnostics.empty()){const auto& d=answer.base.diagnostics.front();fail({d.primary.file,0,d.primary.line,d.primary.column},d.id+": "+d.message);}
    fail({"<type-hir>"},"Nerotypical rejected constraints");
   }
   // Preserve the independent solver's existing type-obligation diagnostics.
   // Region/initialization/tag checks are a separate mandatory admission gate,
   // not a substitute for constructing and solving those obligations.
   canonical::verify_values(unit);
  std::ostringstream report;report<<"on1x.type-safety/v1 nerotypical-structural-euf\nwell_typed concrete HIR\n"<<functions.size()<<" functions, "<<graph.term_count()<<" terms, "<<constraints<<" equalities, "<<answer.base.stats.unification_rounds<<" rounds\n";return report.str();
 }
};
}
std::string verify(const Unit& unit,Limits limits){csp::verify(unit,true);if(csp::present(unit))fail(unit.functions.front().where,"ON1X-CSP-UNSUPPORTED: concurrency is outside the independent structural adapter");Budget budget{limits};return Verify(unit,budget).run();}
}
