#include <on1x/safety.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/concurrency.hpp>
#include <on1x/primitives.hpp>
#include <algorithm>
#include <deque>
#include <limits>
#include <sstream>

namespace on1x::safety {
namespace c=moonstone::chisel;
namespace {
Location location(const moonstone::SourceSpan& s){return {s.file,0,s.line,s.column};}
[[noreturn]] void fail(const Location& at,const std::string& message){throw Error(at,"ON1X-MEMORY: "+message);}
// Strict reader for the versioned line-based admitted-extern companion artifact.
// Structure:
//   on1x.admitted-extern/v1|v2|v3|v4 (v4 adds directional immutable strings)
//   identity <64 lowercase hex digits>
//   extern <name> (<admitted-scalar>,...) -> <admitted-scalar>|void
// Everything unknown, oversized, duplicated or out of profile is rejected.
std::map<std::string,AdmittedExtern> parse_admission_text(const std::string& text){
 constexpr std::size_t max_externs=4096,max_line=512,max_name=256,max_params=256;
 auto reject=[&](std::size_t line,const std::string& why)->Error{
  return Error({"<admitted-extern>",0,line,0},"ON1X-ADMISSION: "+why);
 };
 auto expect=[&](bool condition,std::size_t line,const std::string& why){
  if(!condition)throw reject(line,why);
 };
 if(text.size()>4*1024*1024)throw reject(1,"admission artifact exceeds 4 MiB");
 expect(!text.empty()&&text.back()=='\n',1,"admission artifact must end with a newline");
 std::map<std::string,AdmittedExtern> out;std::string identity;unsigned version=1;
 for(std::size_t begin=0,line=1;begin<text.size();++line){
  auto end=text.find('\n',begin);
  expect(end!=std::string::npos,line,"unterminated admission line");
  auto body=std::string_view(text).substr(begin,end-begin);
  expect(body.size()<=max_line&&body.find('\0')==std::string_view::npos,line,"admission line too long or contains NUL");
  begin=end+1;
  if(line==1){
   expect(body=="on1x.admitted-extern/v1"||body=="on1x.admitted-extern/v2"||body=="on1x.admitted-extern/v3"||body=="on1x.admitted-extern/v4",line,"unknown admission schema");
   version=static_cast<unsigned>(body.back()-'0');
  }else if(line==2){
   expect(body.starts_with("identity "),line,"missing admission identity");
   auto digest=body.substr(9);
   expect(digest.size()==64&&std::all_of(digest.begin(),digest.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),line,"admission identity must be 64 lowercase hex digits");
   identity=std::string(digest);
  }else{
   expect(out.size()<max_externs,line,"too many admitted externs");
   expect(body.starts_with("extern "),line,"unexpected admission line");
   auto rest=body.substr(7);
   auto open=rest.find(" (");
   expect(open!=std::string_view::npos&&open>0&&open<=max_name,line,"malformed extern name");
   auto name=rest.substr(0,open);
   auto first=[](char c){return c=='_'||(c>='a'&&c<='z')||(c>='A'&&c<='Z');};
   expect(first(name.front()),line,"invalid extern name");
   expect(std::all_of(name.begin(),name.end(),[&](char c){return first(c)||(c>='0'&&c<='9');}),line,"invalid extern name");
   expect(name.starts_with("on1x_ffi_"+identity+"_"),line,"extern name is not scoped to the admission identity");
   rest=rest.substr(open+1);
   expect(rest.starts_with("("),line,"malformed extern parameter list");
   auto arrow=rest.find(") -> ");
   expect(arrow!=std::string_view::npos,line,"malformed extern parameter list");
   auto inside=rest.substr(1,arrow-1);
   auto result=rest.substr(arrow+5);
   auto admitted_type=[&](std::string_view token,bool result_position){
    if(token=="i64")return Type{Type::I64};
    if(version>=2&&token=="bool")return Type{Type::Bool};
    if(version>=2&&token=="f64")return Type{Type::F64};
    if(version>=3)for(auto kind:{Type::F32,Type::U8,Type::U16,Type::U32,Type::U64})
     if(token==type_name(kind))return Type{kind};
    if(version>=4&&token==(result_position?"static_string":"borrowed_string"))return Type{Type::String};
    if(result_position&&token=="void")return Type{Type::Void};
    throw reject(line,"extern "+std::string(result_position?"result":"parameter")+" has unsupported scalar type");
   };
   AdmittedExtern admitted;admitted.result=admitted_type(result,true);admitted.immutable_strings=version>=4;
   std::vector<std::string_view> params;
   for(std::size_t at=0;at<inside.size();){
    auto comma=inside.find(',',at);
    auto token=inside.substr(at,comma==std::string_view::npos?inside.size():comma-at);
    params.push_back(token);
    if(comma==std::string_view::npos)break;
    at=comma+1;
    expect(at<inside.size(),line,"trailing comma in extern parameters");
   }
   if(inside.empty())params.clear();
   expect(params.size()<=max_params,line,"too many extern parameters");
   for(const auto& token:params)admitted.parameters.push_back(admitted_type(token,false));
   expect(out.emplace(std::string(name),std::move(admitted)).second,line,"duplicate admitted extern: "+std::string(name));
  }
 }
 expect(!identity.empty(),2,"missing admission identity");
 expect(!out.empty(),3,"admission artifact contains no externs");
 return out;
}

struct Budget {
 Limits left;
 void tick(const Location& at,std::size_t n=1){if(n>left.work)throw BudgetExceeded(at,"ON1X-MEMORY-BUDGET: work exhausted");left.work-=n;}
 void node(const Location& at){tick(at);if(!left.nodes)throw BudgetExceeded(at,"ON1X-MEMORY-BUDGET: nodes exhausted");--left.nodes;}
};
moonstone::SourceSpan span(const Location& at){
 if(at.line>UINT32_MAX||at.column>UINT32_MAX)fail(at,"source coordinate exceeds Chisel range");
 return {at.file,static_cast<std::uint32_t>(at.line),static_cast<std::uint32_t>(at.column),static_cast<std::uint32_t>(at.line),static_cast<std::uint32_t>(at.column)};
}
void verify_impl(const c::Function& input,Budget& budget){
 const Location at{"<memory-cfg>"};
 // Split each event into its own block so stronger admission checks can inspect
 // Chisel's converged input state at every operation, without copying its solver.
 c::Function f;std::map<c::Id,c::Id> starts;std::set<c::Id> places,tokens;
 for(const auto& b:input.blocks){
  budget.node(at);if(b.successors.size()>2)fail(at,"more than two successors");
  if(!starts.emplace(b.id,static_cast<c::Id>(f.blocks.size())).second)fail(at,"duplicate block");
  f.blocks.push_back({static_cast<c::Id>(f.blocks.size()),{}, {}});
  for(const auto& i:b.instructions){
   auto loc=location(i.source);budget.node(loc);budget.tick(loc,i.source.file.size());
   if(i.source.file.empty()||i.source.file.size()>4096||i.source.file.find('\0')!=std::string::npos||!i.source.line||!i.source.column||!i.place)fail(loc,"invalid event identity/source");
   if(static_cast<unsigned>(i.op)>static_cast<unsigned>(c::Op::Escape)||i.op==c::Op::Escape)fail(loc,"unsupported escape/unknown operation");
   bool loan=i.op==c::Op::BorrowShared||i.op==c::Op::BorrowUnique||i.op==c::Op::EndBorrow;
   if((loan&&!i.token)||(!loan&&i.op!=c::Op::Use&&i.token))fail(loc,"invalid loan token");
   if(i.op==c::Op::BorrowShared||i.op==c::Op::BorrowUnique)
    if(!tokens.insert(i.token).second)fail(loc,"loan token has multiple definitions");
   places.insert(i.place);
   f.blocks.back().successors={static_cast<c::Id>(f.blocks.size())};
   f.blocks.push_back({static_cast<c::Id>(f.blocks.size()),{i},{}});
  }
 }
 if(!starts.contains(input.entry))fail(at,"missing entry");
 f.entry=starts.at(input.entry);
 std::size_t end=0;
 for(const auto& b:input.blocks){
  end+=b.instructions.size();
  for(auto next:b.successors){
   budget.tick(at);if(!starts.contains(next)||next==input.entry)fail(at,"invalid successor or entry backedge");
   f.blocks[end].successors.push_back(starts.at(next));
  }
  ++end;
 }
 // Chisel joins loan maps by union. Reject unequal incoming loan states rather
 // than treating a possibly-active token as definitely active on every path.
 std::vector<std::vector<c::Id>> preds(f.blocks.size());std::set<c::Id> reached;
 std::vector<c::Id> pending{f.entry};
 while(!pending.empty()){auto id=pending.back();pending.pop_back();budget.tick(at);if(!reached.insert(id).second)continue;
  for(auto next:f.blocks[id].successors){preds[next].push_back(id);pending.push_back(next);}}
 if(reached.size()!=f.blocks.size())fail(at,"unreachable resource block");
 // Bound the upstream block-visit budget by state size (including join copies).
 auto cost=8*(places.size()+tokens.size()+1);auto iterations=budget.left.work/cost;
 if(!iterations)throw BudgetExceeded(at,"ON1X-MEMORY-BUDGET: Chisel admission exhausted");
 c::Policy policy;policy.iteration_limit=iterations;
 auto result=c::analyze(f,policy);
 // Reserve the complete allowed solver envelope, not an invented measured count.
 budget.tick(at,iterations*cost);
 if(!result.converged)throw BudgetExceeded(at,"ON1X-MEMORY-BUDGET: Chisel did not converge");
 if(!result.diagnostics.empty()){const auto& d=result.diagnostics.front();fail(location(d.primary),d.id+": "+d.message);}
 for(const auto& b:f.blocks){
  if(preds[b.id].size()>1){const auto& first=result.block_out.at(preds[b.id][0]).tokens;
   for(auto p:preds[b.id])if(result.block_out.at(p).tokens!=first)fail(at,"path-dependent loan lifetime at join");}
  if(b.instructions.empty())continue;
  const auto& i=b.instructions.front();const auto& state=result.block_in.at(b.id);
  if(i.op==c::Op::Use){
   if(i.token){auto loan=state.tokens.find(i.token);if(loan==state.tokens.end()||loan->second.place!=i.place)fail(location(i.source),"use of inactive or mismatched borrow");}
   else for(auto token:c::at(state,i.place).loans)if(state.tokens.at(token).unique)fail(location(i.source),"owner access during exclusive borrow");
  }
 }
 // Independent may-live flow closes Chisel's permissive MaybeInvalid reinit and
 // checks normal-exit leaks. This small set analysis has a finite monotone domain.
 std::vector<std::set<c::Id>> live(f.blocks.size());std::deque<c::Id> queue{f.entry};std::set<c::Id> seen{f.entry};
 std::size_t remaining=iterations*cost/2;
 while(!queue.empty()){
  auto id=queue.front();queue.pop_front();
  if(remaining<cost/2)throw BudgetExceeded(at,"ON1X-MEMORY-BUDGET: exit analysis exhausted");
  remaining-=cost/2;
  auto state=live[id];const auto& b=f.blocks[id];
  for(const auto& i:b.instructions){
   if(i.op==c::Op::Create||i.op==c::Op::Reinitialize){if(state.contains(i.place))fail(location(i.source),"possibly-live resource reinitialization");state.insert(i.place);}
   if(i.op==c::Op::Move||i.op==c::Op::Drop)state.erase(i.place);
  }
  if(b.successors.empty()&&(!state.empty()||!result.block_out.at(id).tokens.empty()))fail(at,"resource or borrow remains live at normal exit");
  for(auto next:b.successors){auto old=live[next].size();live[next].insert(state.begin(),state.end());if(seen.insert(next).second||old!=live[next].size())queue.push_back(next);}
 }
}
struct Binding {c::Id owner=0,loan=0;};
class Lower {
 struct LoopTarget {std::size_t id; c::Id done,latch;std::size_t keep;};
 std::vector<LoopTarget> loops;
 std::vector<LoopTarget> regions;
 std::set<std::size_t> region_targets,continue_targets;
 Budget& budget;const std::map<std::string,AdmittedExtern>& admitted;c::Function flow;c::Id current=0,next_place=1,next_loan=1;
 struct Scope {std::map<std::string,Binding> names;std::vector<Binding> cleanup;};
 std::vector<Scope> scopes;
 c::Id block(){budget.node({"<memory-hir>"});auto id=static_cast<c::Id>(flow.blocks.size());flow.blocks.push_back({id,{},{}});return id;}
 void event(c::Op op,Binding b,const Location& at){budget.node(at);flow.blocks[current].instructions.push_back({op,b.owner,b.loan,span(at)});}
 void cleanup(std::size_t index,const Location& at){for(auto it=scopes[index].cleanup.rbegin();it!=scopes[index].cleanup.rend();++it)event(it->loan?c::Op::EndBorrow:c::Op::Drop,*it,at);}
 Binding lookup(const std::string& name,const Location& at){for(auto s=scopes.rbegin();s!=scopes.rend();++s)if(auto b=s->names.find(name);b!=s->names.end())return b->second;fail(at,"unknown local: "+name);}
  bool trivial(const Type& type,const Location& at,std::size_t depth=0){
   budget.node(at);if(depth>64)fail(at,"scalar tuple memory model nesting limit exceeded");
   if(type.is_numeric()||type==Type::Bool)return true;
   if(!type.is_tuple())return false;
   for(const auto& field:type.elements())if(!trivial(field,at,depth+1))return false;
   return true;
  }
  void bind(const Pattern& p){
   budget.node(p.where);
   if(p.is_tuple()){for(const auto& child:p.elements)bind(child);return;}
   scopes.back().names[p.name]={};
  }
 Binding expression(const Expr& e){
  budget.node(e.where);
   switch(e.kind){
   case Expr::Kind::Unit:return {};
   case Expr::Kind::Region:{
    scopes.push_back({});bool done=statements(e.body,false);Binding result;if(!done){result=expression(*e.children.at(0));if(result.owner)fail(e.where,"resource result from value region");cleanup(scopes.size()-1,e.where);}scopes.pop_back();return result;
   }
   case Expr::Kind::Project:if(!trivial(e.type,e.where)||expression(*e.children.at(0)).owner)fail(e.where,"nontrivial tuple projection memory model");return {};
   case Expr::Kind::TagTest:case Expr::Kind::Payload:fail(e.where,"closed-sum memory model unavailable");
    case Expr::Kind::PatternMatch:case Expr::Kind::Case:case Expr::Kind::Loop:fail(e.where,"unlowered source value construct");
   case Expr::Kind::Integer:case Expr::Kind::Boolean:case Expr::Kind::Float:case Expr::Kind::String:return {};
   case Expr::Kind::Tuple:
    if(!trivial(e.type,e.where))fail(e.where,"tuple memory model requires trivially copied numeric/bool fields");
    for(const auto& child:e.children)if(expression(*child).owner)fail(child->where,"resource in scalar tuple");
    return {};
  case Expr::Kind::Variable:{
   if(e.reference!=Expr::Reference::Local)fail(e.where,"first-class callable not modeled");
   auto b=lookup(e.text,e.where);if(b.owner)event(c::Op::Use,b,e.where);return b;
  }
   case Expr::Kind::Unary:case Expr::Kind::Binary:
    if(e.kind==Expr::Kind::Binary&&(e.text=="&&"||e.text=="||")){
     if(expression(*e.children.at(0)).owner)fail(e.where,"resource logical operand");
     auto origin=current,right=block(),done=block();flow.blocks[origin].successors={right,done};current=right;
     if(expression(*e.children.at(1)).owner)fail(e.where,"resource logical operand");
     flow.blocks[current].successors={done};current=done;return {};
    }
   for(const auto& child:e.children)if(expression(*child).owner)fail(e.where,"resource in scalar operator");
   return {};
  case Expr::Kind::Call:{
   if(!e.callee||e.callee->kind!=Expr::Kind::Variable)fail(e.where,"indirect call not modeled");
   const auto& callee=*e.callee;auto name=callee.text;
   if(callee.reference==Expr::Reference::Extern){
    // Admitted externs carry scalars or immutable strings only. Strings have
    // process-lifetime storage in the admitted HIR; parameters cannot escape
    // the foreign call, and results cannot alias caller-owned storage.
    auto found=admitted.find(name);
    if(found==admitted.end())fail(e.where,"raw external call lacks a checked memory model: "+name);
    if(e.children.size()!=found->second.parameters.size())fail(e.where,"admitted extern call arity drift: "+name);
    for(std::size_t i=0;i<e.children.size();++i)
     if(e.children[i]->type!=found->second.parameters[i])fail(e.children[i]->where,"admitted extern call parameter type drift: "+name);
    if(e.type!=found->second.result)fail(e.where,"admitted extern call result drift: "+name);
   }
   if(callee.reference!=Expr::Reference::Function&&callee.reference!=Expr::Reference::Intrinsic&&callee.reference!=Expr::Reference::Extern)fail(e.where,"indirect call not modeled");
   std::vector<Binding> args;for(const auto& arg:e.children){auto b=expression(*arg);
    if(b.owner&&arg->kind!=Expr::Kind::Variable)fail(arg->where,"temporary resource argument not modeled");
    args.push_back(b);}
   if(callee.reference==Expr::Reference::Intrinsic){
    if(name=="json_parse_or_die")return {next_place++,0};
    if(name=="json_get_or_die"){
     if(args.empty()||!args[0].owner)fail(e.where,"borrow has no owner");
     return {args[0].owner,next_loan++};
    }
    if(name!="json_size"&&name!="json_number"&&name!="json_print"&&name!="println"&&name!="assert"&&
       !numeric_primitive(name))fail(e.where,"intrinsic memory model unavailable: "+name);
   }else for(auto b:args)if(b.owner)fail(e.where,"ordinary resource parameter not modeled");
   return {};
  }
  default:fail(e.where,"expression memory model unavailable");
  }
 }
 bool statements(const std::vector<Stmt>& body,bool scope=true){
  if(scope)scopes.push_back({});
  bool returned=false;
  for(const auto& s:body){
   budget.node(s.where);if(s.prototype)continue;if(returned)fail(s.where,"unreachable HIR");
   if(s.kind==Stmt::Kind::Template){returned=statements(s.body,false);continue;}
    switch(s.kind){
    case Stmt::Kind::Defer:case Stmt::Kind::PushDefer:fail(s.where,"deferred capture memory model unavailable");
   case Stmt::Kind::Let:{auto b=expression(*s.value);if(b.owner){
     if(s.value->kind!=Expr::Kind::Call||s.mutable_binding||s.pattern)fail(s.where,"resource copy/mutation not modeled");
     event(b.loan?c::Op::BorrowShared:c::Op::Create,b,s.where);scopes.back().cleanup.push_back(b);
    }
    if(s.pattern)bind(*s.pattern);else scopes.back().names[s.name]=b;break;}
    case Stmt::Kind::Assign:if(expression(*s.value).owner||lookup(s.name,s.where).owner)fail(s.where,"resource reassignment not modeled");break;
    case Stmt::Kind::Storage:if(s.declared!=Type::String&&!trivial(s.declared,s.where))fail(s.where,"nontrivial canonical storage memory model");scopes.back().names[s.name]={};break;
    case Stmt::Kind::Initialize:if(expression(*s.value).owner||lookup(s.name,s.where).owner)fail(s.where,"resource canonical storage not modeled");break;
    case Stmt::Kind::Expr:if(expression(*s.value).owner)fail(s.where,"discarded resource");returned=s.value->noreturn;break;
    case Stmt::Kind::Return:
     if(s.value&&expression(*s.value).owner)fail(s.where,"resource escape");
     if(!s.value||!s.value->noreturn)for(std::size_t n=scopes.size();n;--n)cleanup(n-1,s.where);
    returned=true;break;
   case Stmt::Kind::Block:returned=statements(s.body);break;
   case Stmt::Kind::If:{
    expression(*s.value);auto origin=current,yes=block(),no=block();flow.blocks[origin].successors={yes,no};
    current=yes;bool a=statements(s.body);auto a_end=current;
    current=no;bool b=statements(s.otherwise);auto b_end=current;
    if(a&&b){returned=true;break;}
    current=block();if(!a)flow.blocks[a_end].successors={current};if(!b)flow.blocks[b_end].successors={current};break;
   }
    case Stmt::Kind::While:{
     auto origin=current,head=block();flow.blocks[origin].successors={head};current=head;expression(*s.value);
     auto condition_tail=current,loop=block(),done=block();flow.blocks[condition_tail].successors={loop,done};current=loop;
     auto latch=!s.otherwise.empty()&&continue_targets.contains(s.control_target)?block():head;
     loops.push_back({s.control_target,done,latch,scopes.size()});bool terminated=statements(s.body);loops.pop_back();
     if(!s.otherwise.empty()&&(!terminated||latch!=head)){
      if(latch==head)latch=block();
      if(!terminated)flow.blocks[current].successors={latch};
      current=latch;
      if(!statements(s.otherwise))flow.blocks[current].successors={head};
     }else if(!terminated)flow.blocks[current].successors={head};
     current=done;break;
    }
    case Stmt::Kind::Break:case Stmt::Kind::Continue:{
     auto target=std::find_if(loops.rbegin(),loops.rend(),[&](const auto& t){return t.id==s.control_target;});
     if(target==loops.rend())fail(s.where,"missing canonical loop target");
     for(std::size_t n=scopes.size();n>target->keep;--n)cleanup(n-1,s.where);
     flow.blocks[current].successors={s.kind==Stmt::Kind::Break?target->done:target->latch};returned=true;break;
    }
    case Stmt::Kind::Range:fail(s.where,"unlowered range");
    case Stmt::Kind::Region:{
     auto done=region_targets.contains(s.control_target)?block():std::numeric_limits<c::Id>::max();regions.push_back({s.control_target,done,0,scopes.size()});bool terminal=statements(s.body);regions.pop_back();
     if(done!=std::numeric_limits<c::Id>::max()){if(!terminal)flow.blocks[current].successors={done};current=done;}else returned=terminal;break;
    }
    case Stmt::Kind::Leave:{auto target=std::find_if(regions.rbegin(),regions.rend(),[&](const auto& r){return r.id==s.control_target;});if(target==regions.rend())fail(s.where,"missing region join");for(std::size_t n=scopes.size();n>target->keep;--n)cleanup(n-1,s.where);flow.blocks[current].successors={target->done};returned=true;break;}
    case Stmt::Kind::Trap:{
     // The public CFG has no fatal terminator. A non-returning sink does not
     // pretend that terminal failures unwind or clean up live owners.
     auto origin=current,sink=block();flow.blocks[origin].successors={sink};flow.blocks[sink].successors={sink};current=sink;returned=true;break;
    }
   case Stmt::Kind::Template:break;
   }
  }
  if(scope){if(!returned)cleanup(scopes.size()-1,body.empty()?Location{"<empty-scope>"}:body.back().where);scopes.pop_back();}
  return returned;
 }
public:
 explicit Lower(Budget& b,const std::map<std::string,AdmittedExtern>& a):budget(b),admitted(a){}
  c::Function run(const Function& f){
   region_targets=canonical::exit_targets(f,Stmt::Kind::Leave);continue_targets=canonical::exit_targets(f,Stmt::Kind::Continue);
  current=block();flow.entry=current;scopes.push_back({});
   for(const auto& p:f.params){budget.node(p.where);if(p.type!=Type::String&&!trivial(p.type,p.where))fail(p.where,"parameter memory model unavailable");if(p.pattern)bind(*p.pattern);else scopes.back().names[p.name]={};}
  if(!statements(f.body,false))cleanup(0,f.where);
  return std::move(flow);
 }
};
}
void verify(const c::Function& function,Limits limits){Budget budget{limits};verify_impl(function,budget);}
std::string check(const Unit& unit,Limits limits,const std::map<std::string,AdmittedExtern>& admitted){
 csp::verify(unit,true);
 if(csp::present(unit))fail(unit.functions.front().where,"ON1X-CSP-UNSUPPORTED: concurrency is outside the Chisel resource adapter");
 canonical::verify_control(unit);
 canonical::verify_values(unit);
 Budget lowering{limits};std::ostringstream report;report<<"on1x.memory/v1 chisel JSON/scalar usage\n";
 std::set<std::string> external_names;
 for(const auto& f:unit.functions)if(f.external)external_names.insert(f.name);
 for(const auto& [name,entry]:admitted){
  auto scalar=[&](const Type& t){return t.is_numeric()||t==Type::Bool||(entry.immutable_strings&&t==Type::String);};
  for(const auto& type:entry.parameters)
   if(!scalar(type))fail({"<admitted-extern>"},"admitted extern parameter outside the scalar profile: "+name);
  if(entry.result!=Type::Void&&!scalar(entry.result))fail({"<admitted-extern>"},"admitted extern result outside the scalar profile: "+name);
  if(!external_names.contains(name))fail({"<admitted-extern>"},"admission evidence names an absent external declaration: "+name);
 }
 for(const auto& f:unit.functions){
  lowering.tick(f.where,f.name.size());
  if(f.prototype)continue;
  if(f.external){
   auto found=admitted.find(f.name);
   if(found==admitted.end())fail(f.where,"raw external declaration lacks a checked memory model: "+f.name+" (not covered by admitted-extern evidence)");
   const auto& entry=found->second;
   if(f.params.size()!=entry.parameters.size())fail(f.where,"admitted extern arity drift: "+f.name);
   for(std::size_t i=0;i<f.params.size();++i)
    if(f.params[i].type!=entry.parameters[i])fail(f.params[i].where,"admitted extern parameter type drift: "+f.name);
   if(f.result!=entry.result)fail(f.where,"admitted extern result drift: "+f.name);
   report<<f.name<<": admitted raw extern (reviewed "<<(entry.immutable_strings?"scalar/immutable-string":"scalar-only")<<" evidence)\n";
   continue;
  }
  auto flow=Lower(lowering,admitted).run(f);verify(flow,limits);
  std::size_t events=0;for(const auto& b:flow.blocks)events+=b.instructions.size();
  report<<f.name<<": "<<flow.blocks.size()<<" blocks, "<<events<<" resource events\n";
 }
 return report.str();
}
std::map<std::string,AdmittedExtern> parse_admission(const std::string& text){return parse_admission_text(text);}
}
