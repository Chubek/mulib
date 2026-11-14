#include <on1x/backend/mir.hpp>
#include <on1x/meta.hpp>
#include <charconv>
#include <map>
#include <limits>

namespace on1x::mir {
namespace {
class Lowering {
 const Unit& unit;Limits limits;Module module;Id serial=1;
 std::size_t function=0,current=0;
 static constexpr auto ended=std::numeric_limits<std::size_t>::max();
 struct Binding {Id id;Type type;bool slot;};
 std::vector<std::map<std::string,Binding>> scopes;
 std::map<std::string,Id> functions;
 [[noreturn]] void unsupported(const Location& at,const std::string& why){throw Error(at,"ON1X-MIR-UNSUPPORTED: "+why);}
 Id fresh(const Location& at){if(serial>=limits.nodes)throw Error(at,"ON1X-MIR-LOWER: node budget exhausted");return ++serial;}
 void scalar(const Type& t,const Location& at,bool allow_void=false){
  if(t!=Type::I64&&t!=Type::Bool&&!(allow_void&&t==Type::Void))unsupported(at,"scalar MIR cannot lower "+type_name(t));
 }
 Function& fn(){return module.functions.at(function);}
 Block& block(){return fn().blocks.at(current);}
 std::size_t make_block(const Location& at){
  Block b;b.id=fresh(at);b.effect=fresh(at);b.where=at;
  fn().blocks.push_back(std::move(b));return fn().blocks.size()-1;
 }
 Id token(){
  for(auto it=block().instructions.rbegin();it!=block().instructions.rend();++it)if(it->effect_out)return it->effect_out;
  return block().effect;
 }
 void terminate(Exit kind,Id value,std::vector<Edge> edges,const Location& at){
  auto effect=token();block().exit={fresh(at),kind,value,effect,std::move(edges),at};current=ended;
 }
 Edge edge(std::size_t index,std::vector<Id> arguments={}){return {fn().blocks.at(index).id,std::move(arguments)};}
 void jump(std::size_t index,const Location& at,std::vector<Id> args={}){terminate(Exit::Jump,0,{edge(index,std::move(args))},at);}
 Id emit(Op op,Type type,std::vector<Id> args,const Location& at,Id target=0,std::int64_t immediate=0){
  scalar(type,at,true);Instruction i;i.id=fresh(at);i.type=type;i.op=op;i.operands=std::move(args);i.where=at;i.target=target;i.immediate=immediate;
  if(effect(op)!=Effect::Pure){i.effect_in=token();i.effect_out=fresh(at);}
  auto result=type==Type::Void?0:i.id;block().instructions.push_back(std::move(i));return result;
 }
 Binding binding(const std::string& name,const Location& at){
  for(auto it=scopes.rbegin();it!=scopes.rend();++it)if(auto found=it->find(name);found!=it->end())return found->second;
  unsupported(at,"unresolved local "+name);
 }
 Id expression(const Expr& e){
  scalar(e.type,e.where,true);
  switch(e.kind){
  case Expr::Kind::Integer:{
   std::int64_t value=0;auto [p,ec]=std::from_chars(e.text.data(),e.text.data()+e.text.size(),value);
   if(ec!=std::errc{}||p!=e.text.data()+e.text.size())unsupported(e.where,"integer requires checked decimal HIR");
   return emit(Op::Constant,Type::I64,{},e.where,0,value);
  }
  case Expr::Kind::Boolean:return emit(Op::Constant,Type::Bool,{},e.where,0,e.text=="true");
  case Expr::Kind::Variable:{
   if(e.reference!=Expr::Reference::Local)unsupported(e.where,"first-class function values");
   auto b=binding(e.text,e.where);return b.slot?emit(Op::Load,b.type,{},e.where,b.id):b.id;
  }
  case Expr::Kind::Unary:{auto operand=expression(*e.children.at(0));return emit(e.text=="-"?Op::Neg:Op::Not,e.type,{operand},e.where);}
  case Expr::Kind::Binary:{
   auto left=expression(*e.children.at(0));
   if(e.text=="&&"||e.text=="||"){
    auto rhs=make_block(e.where),merge=make_block(e.where);Id result=fresh(e.where);
    fn().blocks[merge].parameters.push_back({result,Type::Bool,e.where});
    auto yes=e.text=="&&"?edge(rhs):edge(merge,{left});
    auto no=e.text=="&&"?edge(merge,{left}):edge(rhs);
    terminate(Exit::Branch,left,{yes,no},e.where);
    current=rhs;auto right=expression(*e.children.at(1));jump(merge,e.where,{right});current=merge;return result;
   }
   auto right=expression(*e.children.at(1));
   static const std::map<std::string,Op> ops={{"+",Op::Add},{"-",Op::Sub},{"*",Op::Mul},{"/",Op::Div},{"%",Op::Mod},
    {"==",Op::Eq},{"!=",Op::Ne},{"<",Op::Lt},{"<=",Op::Le},{">",Op::Gt},{">=",Op::Ge}};
   auto found=ops.find(e.text);if(found==ops.end())unsupported(e.where,"unknown binary operator");
   return emit(found->second,e.type,{left,right},e.where);
  }
  case Expr::Kind::Call:{
   const auto& c=*e.callee;
   if(c.kind!=Expr::Kind::Variable||(c.reference!=Expr::Reference::Function&&c.reference!=Expr::Reference::Intrinsic))
    unsupported(e.where,"indirect or raw foreign call requires closure/foreign MIR descriptors");
   std::vector<Id> args;for(const auto& a:e.children)args.push_back(expression(*a));
   if(c.reference==Expr::Reference::Function){
    if(!functions.contains(c.text))unsupported(e.where,"unresolved direct call");
    return emit(Op::Call,e.type,std::move(args),e.where,functions.at(c.text));
   }
   if(c.text=="println")return emit(Op::Print,Type::Void,std::move(args),e.where);
   if(c.text=="assert")return emit(Op::Assert,Type::Void,std::move(args),e.where);
   unsupported(e.where,"intrinsic needs an explicit effect/resource descriptor: "+c.text);
  }
  default:unsupported(e.where,"expression requires aggregate, closure, resource or stage lowering");
  }
 }
 void body(const std::vector<Stmt>& statements,bool scope=true){
  if(scope)scopes.emplace_back();
  for(const auto& s:statements){
   if(s.prototype)continue;
   if(current==ended)unsupported(s.where,"unreachable HIR statement");
   switch(s.kind){
   case Stmt::Kind::Let:{
    if(s.pattern)unsupported(s.where,"tuple destructuring");
    auto value=expression(*s.value);scalar(s.value->type,s.where);
    Binding b{value,s.value->type,false};
    if(s.mutable_binding){b.id=fresh(s.where);b.slot=true;fn().slots.push_back({b.id,b.type,s.where});emit(Op::Store,Type::Void,{value},s.where,b.id);}
    scopes.back().emplace(s.name,b);break;
   }
   case Stmt::Kind::Assign:{
    auto b=binding(s.name,s.where);if(!b.slot)unsupported(s.where,"assignment to immutable HIR binding");
    auto value=expression(*s.value);emit(Op::Store,Type::Void,{value},s.where,b.id);break;
   }
   case Stmt::Kind::Return:{auto value=s.value?expression(*s.value):0;terminate(Exit::Return,value,{},s.where);break;}
   case Stmt::Kind::Expr:(void)expression(*s.value);break;
   case Stmt::Kind::Block:body(s.body);break;
   case Stmt::Kind::Template:body(s.body,false);break;
   case Stmt::Kind::If:{
    auto condition=expression(*s.value);auto yes=make_block(s.where),no=make_block(s.where);
    terminate(Exit::Branch,condition,{edge(yes),edge(no)},s.where);
    current=yes;body(s.body);auto yes_tail=current;
    current=no;body(s.otherwise);auto no_tail=current;
    if(yes_tail==ended&&no_tail==ended){current=ended;break;}
    auto merge=make_block(s.where);
    if(yes_tail!=ended){current=yes_tail;jump(merge,s.where);}
    if(no_tail!=ended){current=no_tail;jump(merge,s.where);}
    current=merge;break;
   }
   case Stmt::Kind::While:{
    auto header=make_block(s.where),loop=make_block(s.where),done=make_block(s.where);jump(header,s.where);
    current=header;auto condition=expression(*s.value);terminate(Exit::Branch,condition,{edge(loop),edge(done)},s.where);
    current=loop;body(s.body);if(current!=ended)jump(header,s.where);current=done;break;
   }
   }
  }
  if(scope)scopes.pop_back();
 }
public:
 Lowering(const Unit& u,Limits l):unit(u),limits(l){}
 Module run(){
  meta::require_core(unit);
  for(const auto& f:unit.functions){
   if(f.prototype)continue;
   if(f.external)unsupported(f.where,"raw extern declarations require typed foreign MIR descriptors");
   scalar(f.result,f.where,true);Function target;target.id=fresh(f.where);target.name=f.name;target.result=f.result;target.where=f.where;
   functions.emplace(f.name,target.id);module.functions.push_back(std::move(target));
  }
  for(const auto& f:unit.functions){
   if(f.prototype)continue;
   current=make_block(f.where);scopes.emplace_back();
   for(const auto& p:f.params){
    if(p.pattern)unsupported(p.where,"parameter destructuring");
    scalar(p.type,p.where);auto id=fresh(p.where);block().parameters.push_back({id,p.type,p.where});scopes.back().emplace(p.name,Binding{id,p.type,false});
   }
   body(f.body,false);
   if(current!=ended){if(f.result!=Type::Void)unsupported(f.where,"missing return in checked HIR");terminate(Exit::Return,0,{},f.where);}
   scopes.clear();++function;
  }
  verify(module,limits);return std::move(module);
 }
};
}
Module lower(const Unit& unit,Limits limits){return Lowering(unit,limits).run();}
}
