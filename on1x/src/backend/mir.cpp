#include <on1x/backend/mir.hpp>
#include <algorithm>
#include <map>
#include <set>

namespace on1x::mir {
namespace {
constexpr std::string_view names[]={"const","add.checked","sub.checked","mul.checked",
 "div.checked","mod.checked","neg.checked","not","eq","ne","lt","le","gt","ge",
 "load","store","call","print","assert"};
[[noreturn]] void fail(const Location& at,const std::string& message){throw Error(at,"ON1X-MIR-VERIFY: "+message);}
bool scalar(const Type& t){return t==Type::I64||t==Type::Bool;}
struct Budget {
 Limits left;
 void work(const Location& at,std::size_t n=1){if(n>left.work)fail(at,"analysis budget exhausted");left.work-=n;}
 void node(const Location& at){if(!left.nodes)fail(at,"node budget exhausted");--left.nodes;work(at);}
};
struct Definition { Type type; Id block; std::size_t position; };
}
std::string_view op_name(Op op){
 auto n=static_cast<std::size_t>(op);
 if(n>=std::size(names))throw std::invalid_argument("unknown MIR opcode");
 return names[n];
}
Effect effect(Op op){
 switch(op){
 case Op::Constant:case Op::Not:case Op::Eq:case Op::Ne:case Op::Lt:case Op::Le:case Op::Gt:case Op::Ge:return Effect::Pure;
 case Op::Add:case Op::Sub:case Op::Mul:case Op::Div:case Op::Mod:case Op::Neg:case Op::Assert:return Effect::Trap;
 case Op::Load:case Op::Store:return Effect::Memory;
 case Op::Call:return Effect::Call;
 case Op::Print:return Effect::IO;
 }
 throw std::invalid_argument("unknown MIR opcode");
}
void verify(const Module& module,Limits limits){
 Budget budget{limits};std::set<Id> identities;
 auto identity=[&](Id id,const Location& at){
  budget.node(at);
  if(at.file.empty()||at.file.size()>4096||at.file.find('\0')!=std::string::npos||!at.line||!at.column)fail(at,"invalid source location");
  budget.work(at,at.file.size());
  if(!id||!identities.insert(id).second)fail(at,"zero/duplicate identity");
 };
 if(module.version!=format_version)fail({"<mir>"},"unsupported format version");
 identity(module.id,{"<mir>"});
 if(module.functions.empty())fail({"<mir>"},"empty module");
 std::map<Id,const Function*> functions;std::set<std::string> function_names;
 for(const auto& f:module.functions){
  identity(f.id,f.where);
  if(f.name.empty()||f.name.size()>4096||f.name.find('\0')!=std::string::npos||!function_names.insert(f.name).second)fail(f.where,"invalid/duplicate function name");
  if(!scalar(f.result)&&f.result!=Type::Void)fail(f.where,"unsupported function result type");
  if(f.blocks.empty())fail(f.where,"missing entry block");
  functions.emplace(f.id,&f);
 }
 for(const auto& f:module.functions){
  std::map<Id,const Block*> blocks;std::map<Id,Type> slots;std::map<Id,Definition> defs;
  std::map<Id,std::vector<Id>> predecessors;
  auto value=[&](const Value& v,Id block){
   identity(v.id,v.where);if(!scalar(v.type))fail(v.where,"unsupported value type");
   defs.emplace(v.id,Definition{v.type,block,0});
  };
  for(const auto& s:f.slots){identity(s.id,s.where);if(!scalar(s.type))fail(s.where,"unsupported slot type");slots.emplace(s.id,s.type);}
  for(const auto& b:f.blocks){
   identity(b.id,b.where);identity(b.effect,b.where);blocks.emplace(b.id,&b);
   for(const auto& p:b.parameters)value(p,b.id);
   std::size_t position=0;
   for(const auto& i:b.instructions){
    identity(i.id,i.where);++position;
    if(!scalar(i.type)&&i.type!=Type::Void)fail(i.where,"unsupported instruction type");
    if(static_cast<std::size_t>(i.op)>=std::size(names))fail(i.where,"unknown opcode");
    if(i.type!=Type::Void)defs.emplace(i.id,Definition{i.type,b.id,position});
    if(i.effect_out)identity(i.effect_out,i.where);
   }
   identity(b.exit.id,b.exit.where);
  }
  for(const auto& b:f.blocks){
   const auto& t=b.exit;
   if(t.kind!=Exit::Return&&t.kind!=Exit::Jump&&t.kind!=Exit::Branch)fail(t.where,"missing/invalid terminator");
   std::size_t edges=t.kind==Exit::Return?0:t.kind==Exit::Jump?1:2;
   if(t.edges.size()!=edges)fail(t.where,"terminator edge count mismatch");
   for(const auto& e:t.edges){
    budget.work(t.where);
    if(!blocks.contains(e.block))fail(t.where,"unknown CFG target");
    if(e.block==f.blocks.front().id)fail(t.where,"entry block cannot have predecessors");
    if(e.arguments.size()!=blocks.at(e.block)->parameters.size())fail(t.where,"block argument arity mismatch");
    predecessors[e.block].push_back(b.id);
   }
  }
  std::set<Id> reachable;std::vector<Id> pending{f.blocks.front().id};
  while(!pending.empty()){
   auto id=pending.back();pending.pop_back();budget.work(f.where);
   if(!reachable.insert(id).second)continue;
   for(const auto& e:blocks.at(id)->exit.edges)pending.push_back(e.block);
  }
  if(reachable.size()!=blocks.size())fail(f.where,"unreachable block");
  // A definition dominates a use iff removing its block disconnects the use
  // from entry. Cache queries, and charge every traversal to the work budget.
  std::map<std::pair<Id,Id>,bool> dominance;
  auto dominates=[&](Id definition,Id use){
   auto key=std::pair{definition,use};
   if(auto found=dominance.find(key);found!=dominance.end())return found->second;
   std::set<Id> seen;std::vector<Id> work{f.blocks.front().id};bool result=true;
   while(!work.empty()){
    auto id=work.back();work.pop_back();budget.work(f.where);
    if(id==definition||!seen.insert(id).second)continue;
    if(id==use){result=false;break;}
    for(const auto& e:blocks.at(id)->exit.edges)work.push_back(e.block);
   }
   dominance.emplace(key,result);return result;
  };
  auto use=[&](Id id,Id block,std::size_t position,const Location& at)->Type{
   budget.work(at);
   auto found=defs.find(id);if(found==defs.end())fail(at,"unknown/non-value operand");
   const auto& d=found->second;
   if(d.block==block){if(d.position>=position)fail(at,"use before definition");}
   else if(!dominates(d.block,block))fail(at,"definition does not dominate use");
   return d.type;
  };
  for(const auto& b:f.blocks){
   Id token=b.effect;std::size_t position=0;
   for(const auto& i:b.instructions){
    ++position;std::vector<Type> args;
    for(auto id:i.operands)args.push_back(use(id,b.id,position,i.where));
    auto require=[&](bool condition,const char* message){if(!condition)fail(i.where,message);};
    auto signature=[&](Type result,std::vector<Type> parameters){require(i.type==result&&args==parameters,"opcode type/arity mismatch");};
    if(effect(i.op)==Effect::Pure)require(!i.effect_in&&!i.effect_out,"pure operation has effect tokens");
    else{require(i.effect_in==token&&i.effect_out,"broken effect ordering");token=i.effect_out;}
    if(i.op!=Op::Constant)require(i.immediate==0,"unexpected immediate");
    if(i.op!=Op::Load&&i.op!=Op::Store&&i.op!=Op::Call)require(i.target==0,"unexpected target");
    switch(i.op){
    case Op::Constant:require(scalar(i.type)&&args.empty(),"invalid constant type/arity");require(i.type!=Type::Bool||i.immediate==0||i.immediate==1,"invalid bool constant");break;
    case Op::Add:case Op::Sub:case Op::Mul:case Op::Div:case Op::Mod:signature(Type::I64,{Type::I64,Type::I64});break;
    case Op::Neg:signature(Type::I64,{Type::I64});break;
    case Op::Not:signature(Type::Bool,{Type::Bool});break;
    case Op::Lt:case Op::Le:case Op::Gt:case Op::Ge:signature(Type::Bool,{Type::I64,Type::I64});break;
    case Op::Eq:case Op::Ne:require(i.type==Type::Bool&&args.size()==2&&args[0]==args[1],"invalid equality operands");break;
    case Op::Load:case Op::Store:
     require(slots.contains(i.target),"unknown local slot");
     if(i.op==Op::Load)signature(slots.at(i.target),{});else signature(Type::Void,{slots.at(i.target)});
     break;
    case Op::Call:{
     require(functions.contains(i.target),"unknown call target");
     const auto& callee=*functions.at(i.target);std::vector<Type> parameters;
     for(const auto& p:callee.blocks.front().parameters)parameters.push_back(p.type);
     signature(callee.result,std::move(parameters));break;
    }
    case Op::Print:require(i.type==Type::Void&&args.size()==1,"invalid print signature");break;
    case Op::Assert:signature(Type::Void,{Type::Bool});break;
    }
   }
   const auto& t=b.exit;++position;
   if(t.effect!=token)fail(t.where,"terminator drops/reorders effects");
   if(t.kind==Exit::Branch){if(use(t.value,b.id,position,t.where)!=Type::Bool)fail(t.where,"branch requires bool");}
   else if(t.kind==Exit::Return&&f.result!=Type::Void){if(use(t.value,b.id,position,t.where)!=f.result)fail(t.where,"return type mismatch");}
   else if(t.value)fail(t.where,"unexpected terminator value");
   for(const auto& e:t.edges)for(std::size_t n=0;n<e.arguments.size();++n)
    if(use(e.arguments[n],b.id,position,t.where)!=blocks.at(e.block)->parameters[n].type)fail(t.where,"block argument type mismatch");
  }
  // Forward must-initialization: greatest fixed point, with an empty entry.
  // A loop backedge must never justify an uninitialized first-iteration load.
  std::set<Id> all;for(const auto& [id,type]:slots){(void)type;all.insert(id);}
  std::map<Id,std::set<Id>> initialized;
  for(const auto& b:f.blocks){budget.work(b.where,all.size());initialized[b.id]=all;}
  auto incoming=[&](const Block& b){
   std::set<Id> state;
   if(b.id!=f.blocks.front().id){
    state=all;budget.work(b.where,all.size());
    for(auto p:predecessors[b.id]){
     budget.work(b.where,state.size());
     for(auto it=state.begin();it!=state.end();)if(!initialized.at(p).contains(*it))it=state.erase(it);else ++it;
    }
   }
   return state;
  };
  bool changed=true;
  while(changed){
   changed=false;
   for(const auto& b:f.blocks){
    budget.work(b.where);auto state=incoming(b);
    for(const auto& i:b.instructions){budget.work(i.where);if(i.op==Op::Store)state.insert(i.target);}
    if(state!=initialized.at(b.id)){initialized[b.id]=std::move(state);changed=true;}
   }
  }
  for(const auto& b:f.blocks){
   auto state=incoming(b);
   for(const auto& i:b.instructions){
    budget.work(i.where);
    if(i.op==Op::Load&&!state.contains(i.target))fail(i.where,"load from possibly uninitialized slot");
    if(i.op==Op::Store)state.insert(i.target);
   }
  }
 }
}
}
