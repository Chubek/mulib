#include <on1x/safety.hpp>
#include <deque>
#include <limits>
#include <sstream>

namespace on1x::safety {
namespace c=moonstone::chisel;
namespace {
Location location(const moonstone::SourceSpan& s){return {s.file,0,s.line,s.column};}
[[noreturn]] void fail(const Location& at,const std::string& message){throw Error(at,"ON1X-MEMORY: "+message);}
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
 Budget& budget;c::Function flow;c::Id current=0,next_place=1,next_loan=1;
 struct Scope {std::map<std::string,Binding> names;std::vector<Binding> cleanup;};
 std::vector<Scope> scopes;
 c::Id block(){budget.node({"<memory-hir>"});auto id=static_cast<c::Id>(flow.blocks.size());flow.blocks.push_back({id,{},{}});return id;}
 void event(c::Op op,Binding b,const Location& at){budget.node(at);flow.blocks[current].instructions.push_back({op,b.owner,b.loan,span(at)});}
 void cleanup(std::size_t index,const Location& at){for(auto it=scopes[index].cleanup.rbegin();it!=scopes[index].cleanup.rend();++it)event(it->loan?c::Op::EndBorrow:c::Op::Drop,*it,at);}
 Binding lookup(const std::string& name,const Location& at){for(auto s=scopes.rbegin();s!=scopes.rend();++s)if(auto b=s->names.find(name);b!=s->names.end())return b->second;fail(at,"unknown local: "+name);}
 void bind(const Pattern& p){if(p.is_tuple())fail(p.where,"tuple patterns not modeled");scopes.back().names[p.name]={};}
 Binding expression(const Expr& e){
  budget.node(e.where);
  switch(e.kind){
  case Expr::Kind::Integer:case Expr::Kind::Boolean:case Expr::Kind::String:return {};
  case Expr::Kind::Variable:{
   if(e.reference!=Expr::Reference::Local)fail(e.where,"first-class callable not modeled");
   auto b=lookup(e.text,e.where);if(b.owner)event(c::Op::Use,b,e.where);return b;
  }
  case Expr::Kind::Unary:case Expr::Kind::Binary:
   // Only scalar expressions here; constructors/borrows cannot have scalar type.
   // Both logical paths may use live resources, but cannot change ownership.
   for(const auto& child:e.children)if(expression(*child).owner)fail(e.where,"resource in scalar operator");
   return {};
  case Expr::Kind::Call:{
   if(!e.callee||e.callee->kind!=Expr::Kind::Variable)fail(e.where,"indirect call not modeled");
   const auto& callee=*e.callee;auto name=callee.text;
   if(callee.reference==Expr::Reference::Extern)fail(e.where,"raw external call lacks a checked memory model: "+name);
   if(callee.reference!=Expr::Reference::Function&&callee.reference!=Expr::Reference::Intrinsic)fail(e.where,"indirect call not modeled");
   std::vector<Binding> args;for(const auto& arg:e.children){auto b=expression(*arg);
    if(b.owner&&arg->kind!=Expr::Kind::Variable)fail(arg->where,"temporary resource argument not modeled");
    args.push_back(b);}
   if(callee.reference==Expr::Reference::Intrinsic){
    if(name=="json_parse_or_die")return {next_place++,0};
    if(name=="json_get_or_die"){
     if(args.empty()||!args[0].owner)fail(e.where,"borrow has no owner");
     return {args[0].owner,next_loan++};
    }
    if(name!="json_size"&&name!="json_number"&&name!="json_print"&&name!="println"&&name!="assert")fail(e.where,"intrinsic memory model unavailable: "+name);
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
   case Stmt::Kind::Let:{auto b=expression(*s.value);if(b.owner){
     if(s.value->kind!=Expr::Kind::Call||s.mutable_binding||s.pattern)fail(s.where,"resource copy/mutation not modeled");
     event(b.loan?c::Op::BorrowShared:c::Op::Create,b,s.where);scopes.back().cleanup.push_back(b);
    }
    if(s.pattern)bind(*s.pattern);else scopes.back().names[s.name]=b;break;}
   case Stmt::Kind::Assign:if(expression(*s.value).owner||lookup(s.name,s.where).owner)fail(s.where,"resource reassignment not modeled");break;
   case Stmt::Kind::Expr:if(expression(*s.value).owner)fail(s.where,"discarded resource");break;
   case Stmt::Kind::Return:
    if(s.value&&expression(*s.value).owner)fail(s.where,"resource escape");
    for(std::size_t n=scopes.size();n;--n)cleanup(n-1,s.where);
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
    auto loop=block(),done=block();flow.blocks[head].successors={loop,done};current=loop;
    if(!statements(s.body))flow.blocks[current].successors={head};
    current=done;break;
   }
   case Stmt::Kind::Template:break;
   }
  }
  if(scope){if(!returned)cleanup(scopes.size()-1,body.empty()?Location{"<empty-scope>"}:body.back().where);scopes.pop_back();}
  return returned;
 }
public:
 explicit Lower(Budget& b):budget(b){}
 c::Function run(const Function& f){
  current=block();flow.entry=current;scopes.push_back({});
  for(const auto& p:f.params){budget.node(p.where);if(p.type!=Type::I64&&p.type!=Type::Bool&&p.type!=Type::String)fail(p.where,"parameter memory model unavailable");if(p.pattern)bind(*p.pattern);else scopes.back().names[p.name]={};}
  if(!statements(f.body,false))cleanup(0,f.where);
  return std::move(flow);
 }
};
}
void verify(const c::Function& function,Limits limits){Budget budget{limits};verify_impl(function,budget);}
std::string check(const Unit& unit,Limits limits){
 Budget lowering{limits};std::ostringstream report;report<<"on1x.memory/v1 chisel JSON/scalar usage\n";
 for(const auto& f:unit.functions){
  lowering.tick(f.where,f.name.size());
  if(f.prototype)continue;
  if(f.external)fail(f.where,"raw external declaration lacks a checked memory model: "+f.name);
  auto flow=Lower(lowering).run(f);verify(flow,limits);
  std::size_t events=0;for(const auto& b:flow.blocks)events+=b.instructions.size();
  report<<f.name<<": "<<flow.blocks.size()<<" blocks, "<<events<<" resource events\n";
 }
 return report.str();
}
}
