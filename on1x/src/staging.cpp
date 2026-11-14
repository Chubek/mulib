#include <on1x/staging.hpp>
#include <set>
namespace on1x {
namespace {
using K=Expr::Kind;
class Lowering {
 struct Binding {std::size_t level,world;};
 Unit& unit;std::vector<std::map<std::string,Binding>> scopes;
 struct QuoteFrame {std::size_t identity;std::vector<std::string> names;std::vector<ExprPtr> operands;};
  std::vector<QuoteFrame*> frames;std::set<std::string> used,globals;std::size_t serial=0,world=0,budget=262144;
 void tick(const Location& at){if(!budget--)throw Error(at,"staging AST limit exceeded");}
 std::string fresh(){std::string n;do{n="on1x_stage_"+std::to_string(++serial);}while(used.contains(n));used.insert(n);return n;}
 void bind(const std::string& name,std::size_t level){scopes.back()[name]={level,level?frames.at(level-1)->identity:0};}
 void bind(const Pattern& p,std::size_t level){if(p.is_tuple())for(const auto& c:p.elements)bind(c,level);else bind(p.name,level);}
 void reference(const Expr& e,std::size_t level){
  for(auto it=scopes.rbegin();it!=scopes.rend();++it){auto found=it->find(e.text);if(found==it->end())continue;
   const auto& binding=found->second;
   if(binding.level>level||(binding.level&&frames.at(binding.level-1)->identity!=binding.world))throw Error(e.where,"MSP: future-stage binding escapes into an earlier stage: "+e.text);
   if(binding.level<level)throw Error(e.where,"MSP: cross-stage reference requires lift(...) or splice(...): "+e.text);
   return;
  }
 }
 void body(std::vector<Stmt>& statements,std::size_t level,bool push=true){
  if(push)scopes.emplace_back();
   for(auto& s:statements){tick(s.where);if(s.kind==Stmt::Kind::Assign){Expr id;id.text=s.name;id.where=s.where;reference(id,level);}if(s.value)expression(*s.value,level);if(s.kind==Stmt::Kind::Let){if(s.pattern)bind(*s.pattern,level);else bind(s.name,level);}body(s.body,level);body(s.otherwise,level);}
  if(push)scopes.pop_back();
 }
  void expression(Expr& e,std::size_t level){
   tick(e.where);
   if(e.kind==K::Call&&e.callee->kind==K::Variable&&e.callee->text=="run"&&!globals.contains("run")){
    bool local=false;for(const auto& scope:scopes)local=local||scope.contains("run");
    if(!local){if(e.children.size()!=1)throw Error(e.where,"MSP: run requires one code<T> operand");e.kind=K::Run;e.callee.reset();}
   }
  if(e.kind==K::Variable){reference(e,level);return;}
  if(e.kind==K::Splice||e.kind==K::Lift){
   if(frames.empty()||!level)throw Error(e.where,"MSP: splice/lift requires an enclosing quotation");
   auto* frame=frames.back();frames.pop_back();expression(*e.children.at(0),level-1);frames.push_back(frame);
   auto name=fresh();frame->names.push_back(name);frame->operands.push_back(std::move(e.children[0]));
   auto id=std::make_unique<Expr>();id->kind=K::Variable;id->where=e.where;id->text=name;
   bool splice=e.kind==K::Splice;e.children.clear();
   if(splice){e.kind=K::Run;e.children.push_back(std::move(id));}else e=std::move(*id);
   return;
  }
  if(e.kind==K::Quote){
   if(level>=64)throw Error(e.where,"MSP: quotation stage limit exceeded");
   QuoteFrame frame{++world,{},{}};frames.push_back(&frame);auto quoted=std::move(e.children.at(0));expression(*quoted,level+1);frames.pop_back();
   auto thunk=std::make_unique<Expr>();thunk->kind=K::Lambda;thunk->where=e.where;thunk->result=Type::Hole;
   Stmt result;result.kind=Stmt::Kind::Return;result.where=e.where;result.value=std::move(quoted);thunk->body.push_back(std::move(result));
   auto factory=std::make_unique<Expr>();factory->kind=K::Lambda;factory->where=e.where;factory->result=Type::Hole;
   for(const auto& name:frame.names){Param parameter;parameter.name=name;parameter.type=Type::Hole;parameter.where=e.where;factory->parameters.push_back(std::move(parameter));}
   Stmt returns;returns.kind=Stmt::Kind::Return;returns.where=e.where;returns.value=std::move(thunk);factory->body.push_back(std::move(returns));
   auto call=std::make_unique<Expr>();call->kind=K::Call;call->where=e.where;call->callee=std::move(factory);call->children=std::move(frame.operands);
   e.children.clear();e.children.push_back(std::move(call));return;
  }
  if(e.callee)expression(*e.callee,level);
  for(auto& c:e.children)expression(*c,level);
  if(e.kind==K::Lambda){scopes.emplace_back();for(const auto& p:e.parameters){if(p.pattern)bind(*p.pattern,level);else bind(p.name,level);}body(e.body,level,false);scopes.pop_back();}
 }
public:
 explicit Lowering(Unit& u):unit(u){}
 void run(){
  auto pattern=[&](auto&& self,const Pattern& p)->void{used.insert(p.name);for(const auto& c:p.elements)self(self,c);};
  auto collect=[&](auto&& self,const Expr& e)->void{if(e.kind==K::Variable)used.insert(e.text);for(const auto& p:e.parameters){used.insert(p.name);if(p.pattern)pattern(pattern,*p.pattern);}if(e.callee)self(self,*e.callee);for(const auto& c:e.children)self(self,*c);auto b=[&](auto&& visit,const std::vector<Stmt>& body)->void{for(const auto& s:body){used.insert(s.name);if(s.pattern)pattern(pattern,*s.pattern);if(s.value)self(self,*s.value);visit(visit,s.body);visit(visit,s.otherwise);}};b(b,e.body);};
  for(const auto& f:unit.functions){used.insert(f.name);for(const auto& p:f.params){used.insert(p.name);if(p.pattern)pattern(pattern,*p.pattern);}auto b=[&](auto&& self,const std::vector<Stmt>& body)->void{for(const auto& s:body){used.insert(s.name);if(s.pattern)pattern(pattern,*s.pattern);if(s.value)collect(collect,*s.value);self(self,s.body);self(self,s.otherwise);}};b(b,f.body);}
  for(const auto& f:unit.functions)globals.insert(f.name);
  for(auto& f:unit.functions){scopes.emplace_back();for(const auto& p:f.params){if(p.pattern)bind(*p.pattern,0);else bind(p.name,0);}body(f.body,0,false);scopes.pop_back();}
  // Factories add real HIR nodes; bound the lowered tree too, not just source.
  std::size_t lowered=262144;
  auto validate=[&](auto&& self,const Expr& e,std::size_t depth)->void{
   if(depth>max_syntax_depth||!lowered--)throw Error(e.where,"MSP: lowered staging AST depth/node limit exceeded");
   if(e.callee)self(self,*e.callee,depth+1);
   for(const auto& child:e.children)self(self,*child,depth+1);
   auto visit=[&](auto&& walk,const std::vector<Stmt>& statements)->void{for(const auto& s:statements){if(!lowered--)throw Error(s.where,"MSP: lowered staging AST node limit exceeded");if(s.value)self(self,*s.value,depth+1);walk(walk,s.body);walk(walk,s.otherwise);}};visit(visit,e.body);
  };
  for(const auto& f:unit.functions){auto visit=[&](auto&& self,const std::vector<Stmt>& statements)->void{for(const auto& s:statements){if(!lowered--)throw Error(s.where,"MSP: lowered staging AST node limit exceeded");if(s.value)validate(validate,*s.value,1);self(self,s.body);self(self,s.otherwise);}};visit(visit,f.body);}
  unit.stages_lowered=true;
 }
};
}
bool needs_staging(const Unit& unit){
 if(unit.stages_lowered)return false;
 auto expression=[&](auto&& self,const Expr& e)->bool{if(e.kind==K::Quote||e.kind==K::Splice||e.kind==K::Lift||e.kind==K::Run||(e.kind==K::Call&&e.callee->kind==K::Variable&&e.callee->text=="run"))return true;if(e.callee&&self(self,*e.callee))return true;for(const auto& c:e.children)if(self(self,*c))return true;auto body=[&](auto&& visit,const std::vector<Stmt>& b)->bool{for(const auto& s:b)if((s.value&&self(self,*s.value))||visit(visit,s.body)||visit(visit,s.otherwise))return true;return false;};return body(body,e.body);};
 for(const auto& f:unit.functions){auto body=[&](auto&& self,const std::vector<Stmt>& b)->bool{for(const auto& s:b)if((s.value&&expression(expression,*s.value))||self(self,s.body)||self(self,s.otherwise))return true;return false;};if(body(body,f.body))return true;}
 return false;
}
void lower_stages(Unit& unit){if(needs_staging(unit))Lowering(unit).run();}
}
