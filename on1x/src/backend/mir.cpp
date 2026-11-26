#include <on1x/backend/mir_directives.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <bit>
#include <tuple>
#include "numeric.hpp"

namespace on1x::mir {
namespace {
constexpr std::string_view names[]={"const","add.checked","sub.checked","mul.checked",
 "div.checked","mod.checked","neg.checked","not","eq","ne","lt","le","gt","ge",
 "load","store","call","print","assert",
 "f64.add","f64.sub","f64.mul","f64.div","f64.neg",
 "f64.eq","f64.ne","f64.lt","f64.le","f64.gt","f64.ge",
 "f64.from-i64","f64.to-i64","f64.from-bits","f64.to-bits",
  "f64.is-nan","f64.is-finite","f64.signbit","tuple","extract","numeric",
  "owner.create","owner.move","owner.drop"};
[[noreturn]] void fail(const Location& at,const std::string& message){throw Error(at,"ON1X-MIR-VERIFY: "+message);}
bool scalar(const Type& t){return t==Type::I64||t==Type::Bool;}
struct Budget {
 Limits left;
 void work(const Location& at,std::size_t n=1){if(n>left.work)fail(at,"analysis budget exhausted");left.work-=n;}
 void node(const Location& at){if(!left.nodes)fail(at,"node budget exhausted");--left.nodes;work(at);}
};
struct Definition { Type type; Id block; std::size_t position; };
bool owner_op(Op op){return op==Op::OwnerCreate||op==Op::OwnerMove||op==Op::OwnerDrop;}
bool nominal_identifier(std::string_view name,bool qualified){
 if(name.empty()||name.size()>128)return false;
 bool start=true;
 for(unsigned char c:name){
  if(!start&&qualified&&c=='.'){start=true;continue;}
  if(c!='_'&&!(c>='A'&&c<='Z')&&!(c>='a'&&c<='z')&&(start||c<'0'||c>'9'))return false;
  start=false;
 }
 return !start;
}
bool directive_selector(std::string_view selector){
 if(selector.empty())return true;
 if(selector.size()>128)return false;
 auto alphanumeric=[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');};
 if(!alphanumeric(selector.front())&&selector.front()!='_')return false;
 return std::all_of(selector.begin()+1,selector.end(),[&](unsigned char c){return alphanumeric(c)||c=='_'||c=='-'||c=='.';});
}
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
  case Op::Load:case Op::Store:case Op::OwnerCreate:case Op::OwnerMove:case Op::OwnerDrop:return Effect::Memory;
 case Op::Call:return Effect::Call;
  case Op::Print:return Effect::IO;
  case Op::FloatFromBits:case Op::FloatToBits:case Op::FloatIsNaN:case Op::FloatIsFinite:case Op::FloatSignbit:
  case Op::Tuple:case Op::Extract:return Effect::Pure;
  case Op::FloatAdd:case Op::FloatSub:case Op::FloatMul:case Op::FloatDiv:case Op::FloatNeg:
  case Op::FloatEq:case Op::FloatNe:case Op::FloatLt:case Op::FloatLe:case Op::FloatGt:case Op::FloatGe:
   case Op::FloatFromI64:case Op::FloatToI64:return Effect::Trap;
   case Op::Numeric:return Effect::Trap; // Conservative, including pure bit operations.
 }
 throw std::invalid_argument("unknown MIR opcode");
}
namespace {
void verify_profile(const Module& module,Limits limits,bool nominal_owner){
 Budget budget{limits};std::set<Id> identities;
 auto identity=[&](Id id,const Location& at){
  budget.node(at);
  if(at.file.empty()||at.file.size()>4096||at.file.find('\0')!=std::string::npos||!at.line||!at.column)fail(at,"invalid source location");
  budget.work(at,at.file.size());
  if(!id||!identities.insert(id).second)fail(at,"zero/duplicate identity");
 };
 const Location module_at{"<mir>"};
 if(nominal_owner){
  if(module.version!=nominal_owner_format_version)fail(module_at,"nominal-owner admission requires MIR version 5");
  if(!module.directives.empty())fail(module_at,"directives are unsupported in nominal-owner MIR");
 }else{
  if(module.version<legacy_format_version||module.version>directive_format_version)fail(module_at,"unsupported format version");
  if(!module.nominal_owners.empty())fail(module_at,"nominal owner declarations require explicit admission");
  if(module.version<directive_format_version&&!module.directives.empty())fail(module_at,"directives require MIR version 4");
 }
  auto admitted=[&](const Type& type,const Location& at){
  auto visit=[&](auto&& self,const Type& t,std::size_t depth)->bool{
   budget.work(at);if(depth>32)fail(at,"type depth budget exhausted");
   if(scalar(t))return true;
   if(nominal_owner||module.version==legacy_format_version)return false;
    if(t==Type::F64)return true;
    if(module.version>=numeric_format_version&&(t==Type::F32||t.is_unsigned()))return true;
   if(!t.is_tuple())return false;
   for(const auto& element:t.elements())if(!self(self,element,depth+1))return false;
   return true;
  };
   return visit(visit,type,0);
  };
  // Structural equality must share the verification work budget too. Repeated
  // calls/edges can compare the same independently decoded aggregate shapes.
  using Shape=const std::vector<Type>*;
  std::map<std::pair<Shape,Shape>,bool> equal_types;
  auto same_type=[&](auto&& self,const Type& a,const Type& b,const Location& at)->bool{
   budget.work(at);
   if(a.kind()!=b.kind())return false;
   if(!a.is_tuple())return a==b;
   const auto& x=a.elements();const auto& y=b.elements();
   if(&x==&y)return true;
   auto key=std::pair{&x,&y};
   if(auto found=equal_types.find(key);found!=equal_types.end())return found->second;
   bool equal=x.size()==y.size();
   for(std::size_t n=0;equal&&n<x.size();++n)equal=self(self,x[n],y[n],at);
   equal_types.emplace(key,equal);return equal;
  };
  auto same=[&](const Type& a,const Type& b,const Location& at){return same_type(same_type,a,b,at);};
  auto same_list=[&](const std::vector<Type>& a,const std::vector<Type>& b,const Location& at){
   budget.work(at);if(a.size()!=b.size())return false;
   for(std::size_t n=0;n<a.size();++n)if(!same(a[n],b[n],at))return false;
   return true;
  };
 identity(module.id,module_at);
 std::set<Id> nominal_types;
 if(nominal_owner){
  if(module.nominal_owners.empty())fail(module_at,"missing nominal owner declarations");
  std::set<std::string_view> nominal_names;
  for(const auto& decl:module.nominal_owners){
   identity(decl.id,decl.where);
   budget.work(decl.where,decl.name.size());
   budget.work(decl.where,decl.field.size());
   if(!nominal_identifier(decl.name,true)||!nominal_names.insert(decl.name).second)
    fail(decl.where,"invalid/duplicate nominal owner name");
   if(!nominal_identifier(decl.field,false)||decl.payload!=Type::I64)
    fail(decl.where,"nominal owner layout requires one named i64 field");
   nominal_types.insert(decl.id);
  }
 }
 if(module.functions.empty())fail(module_at,"empty module");
 std::map<Id,const Function*> functions;std::set<std::string> function_names;
 for(const auto& f:module.functions){
  identity(f.id,f.where);
  if(f.name.empty()||f.name.size()>4096||f.name.find('\0')!=std::string::npos||!function_names.insert(f.name).second)fail(f.where,"invalid/duplicate function name");
  if(!admitted(f.result,f.where)&&f.result!=Type::Void)fail(f.where,"unsupported function result type");
  if(f.blocks.empty())fail(f.where,"missing entry block");
  if(nominal_owner&&(f.blocks.size()!=1||!f.slots.empty()))
   fail(f.where,"nominal-owner MIR requires one block and no slots per function");
  functions.emplace(f.id,&f);
 }
 using DirectiveKey=std::tuple<std::string_view,std::string_view,std::string_view,std::string_view,Id,bool>;
 std::set<DirectiveKey> directive_keys;
 for(const auto& d:module.directives){
  budget.node(module_at);
  budget.work(module_at,d.backend.size());
  budget.work(module_at,d.target.size());
  budget.work(module_at,d.kind.size());
  budget.work(module_at,d.action.size());
  if(!directive_selector(d.backend)||!directive_selector(d.target))fail(module_at,"invalid directive selector");
  if(d.kind=="hint"){
   const bool function_hint=d.action=="cold-code"||d.action=="no-inline";
   if(!function_hint&&d.action!="kernel-hot"&&d.action!="local-memory"&&d.action!="canonicalized"&&d.action!="private-code")
    fail(module_at,"unknown directive hint action");
   if(function_hint&&!d.function)fail(module_at,"directive hint "+d.action+" requires function scope");
   }else if(d.kind=="rewrite"){
    const bool scoped=d.action==memgraph_local_action||d.action==meta_canonical_action;
    if(!scoped&&d.action!="fold-identity")fail(module_at,"unknown directive rewrite action");
    if(scoped&&!d.function)fail(module_at,"directive rewrite "+d.action+" requires function scope");
  }else fail(module_at,"unknown directive kind");
  if(d.function&&!functions.contains(d.function))fail(module_at,"unknown directive function");
  if(!directive_keys.emplace(d.backend,d.target,d.kind,d.action,d.function,d.speculative).second)
   fail(module_at,"duplicate directive");
 }
 bool has_create=false;
 for(const auto& f:module.functions){
  std::map<Id,const Block*> blocks;std::map<Id,Type> slots;std::map<Id,Definition> defs;
  std::map<Id,std::vector<Id>> predecessors;
  auto value=[&](const Value& v,Id block){
   identity(v.id,v.where);if(!admitted(v.type,v.where))fail(v.where,"unsupported value type");
   defs.emplace(v.id,Definition{v.type,block,0});
  };
  for(const auto& s:f.slots){identity(s.id,s.where);if(!admitted(s.type,s.where))fail(s.where,"unsupported slot type");slots.emplace(s.id,s.type);}
  for(const auto& b:f.blocks){
   identity(b.id,b.where);identity(b.effect,b.where);blocks.emplace(b.id,&b);
   for(const auto& p:b.parameters)value(p,b.id);
   std::size_t position=0;
   for(const auto& i:b.instructions){
    identity(i.id,i.where);++position;
    if(!admitted(i.type,i.where)&&i.type!=Type::Void)fail(i.where,"unsupported instruction type");
    if(static_cast<std::size_t>(i.op)>=std::size(names))fail(i.where,"unknown opcode");
    if(owner_op(i.op)){
     if(!nominal_owner)fail(i.where,"nominal owner opcode requires explicit admission");
     if(i.type!=Type::Void)fail(i.where,"nominal owner operation cannot define an ordinary result");
    }else if(nominal_owner&&(i.op>Op::Assert||i.op==Op::Load||i.op==Op::Store||i.op==Op::Call))
     fail(i.where,"unsupported opcode in nominal-owner MIR");
    if(module.version==legacy_format_version&&i.op>Op::Assert)fail(i.where,"opcode requires MIR version 2");
    if(i.op==Op::Numeric&&module.version<numeric_format_version)fail(i.where,"numeric opcode requires MIR version 3");
    if(i.primitive.size()>64||(i.op!=Op::Numeric&&!i.primitive.empty()))fail(i.where,"invalid numeric descriptor");
    budget.work(i.where,i.primitive.size());
    if(i.type!=Type::Void)defs.emplace(i.id,Definition{i.type,b.id,position});
    if(i.effect_out)identity(i.effect_out,i.where);
   }
   identity(b.exit.id,b.exit.where);
  }
  for(const auto& b:f.blocks){
   const auto& t=b.exit;
   if(nominal_owner&&t.kind!=Exit::Return)fail(t.where,"nominal-owner block must return directly");
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
   // Owner places are disjoint from defs (ordinary SSA). Since a v5 function
   // has one block, each consuming instruction can check the exact live place.
   struct LiveOwner { Id place,nominal; };
   std::optional<LiveOwner> live;
   for(const auto& i:b.instructions){
    ++position;std::vector<Type> args;
    const bool owner=owner_op(i.op);
    if(live&&!owner)fail(i.where,"only owner.move or owner.drop is allowed with a live owner");
    if(!owner)for(auto id:i.operands)args.push_back(use(id,b.id,position,i.where));
    auto require=[&](bool condition,const char* message){if(!condition)fail(i.where,message);};
    auto signature=[&](Type result,std::vector<Type> parameters){require(same(i.type,result,i.where)&&same_list(args,parameters,i.where),"opcode type/arity mismatch");};
    if(effect(i.op)==Effect::Pure)require(!i.effect_in&&!i.effect_out,"pure operation has effect tokens");
    else{require(i.effect_in==token&&i.effect_out,"broken effect ordering");token=i.effect_out;}
    if(i.op!=Op::Constant&&i.op!=Op::Extract)require(i.immediate==0,"unexpected immediate");
    if(owner){
     require(i.operands.size()==1,"nominal owner operation requires one operand");
     budget.work(i.where);
     const Id operand=i.operands.front();
     if(i.op==Op::OwnerCreate){
      require(!live,"owner.create while an owner is live");
      require(use(operand,b.id,position,i.where)==Type::I64,"owner.create requires an ordinary i64 payload");
      require(nominal_types.contains(i.target),"unknown nominal owner declaration");
      live=LiveOwner{i.id,i.target};
      has_create=true;
     }else{
      if(!live||operand!=live->place)fail(i.where,"owner operation requires the current live place");
      if(i.op==Op::OwnerMove){
       require(i.target==live->nominal,"nominal owner identity mismatch");
       live->place=i.id;
      }else{
       require(i.target==0,"unexpected owner.drop target");
       live.reset();
      }
     }
     continue;
    }
    if(i.op!=Op::Load&&i.op!=Op::Store&&i.op!=Op::Call)require(i.target==0,"unexpected target");
    switch(i.op){
    case Op::Constant:{
      require((scalar(i.type)||i.type.is_numeric())&&args.empty(),"invalid constant type/arity");
     require(i.type!=Type::Bool||i.immediate==0||i.immediate==1,"invalid bool constant");
     if(i.type==Type::F64){auto bits=std::bit_cast<std::uint64_t>(i.immediate);
       require((bits&UINT64_C(0x7fffffffffffffff))<=UINT64_C(0x7ff0000000000000)||bits==UINT64_C(0x7ff8000000000000),"noncanonical f64 NaN constant");}
      if(i.type==Type::F32){auto bits=std::bit_cast<std::uint64_t>(i.immediate);
       require(bits<=UINT32_MAX,"f32 constant exceeds binary32 payload");
       require((bits&UINT32_C(0x7fffffff))<=UINT32_C(0x7f800000)||bits==UINT32_C(0x7fc00000),"noncanonical f32 NaN constant");}
      if(i.type.is_unsigned())require(std::bit_cast<std::uint64_t>(i.immediate)<=detail::unsigned_max(i.type),"unsigned constant out of range");
     break;
    }
    case Op::Add:case Op::Sub:case Op::Mul:case Op::Div:case Op::Mod:signature(Type::I64,{Type::I64,Type::I64});break;
    case Op::Neg:signature(Type::I64,{Type::I64});break;
    case Op::Not:signature(Type::Bool,{Type::Bool});break;
    case Op::Lt:case Op::Le:case Op::Gt:case Op::Ge:signature(Type::Bool,{Type::I64,Type::I64});break;
    case Op::Eq:case Op::Ne:require(i.type==Type::Bool&&args.size()==2&&scalar(args[0])&&args[0]==args[1],"invalid equality operands");break;
    case Op::Load:case Op::Store:
     require(slots.contains(i.target),"unknown local slot");
     if(i.op==Op::Load)signature(slots.at(i.target),{});else signature(Type::Void,{slots.at(i.target)});
     break;
    case Op::Call:{
     require(functions.contains(i.target),"unknown call target");
     const auto& callee=*functions.at(i.target);
     const auto& parameters=callee.blocks.front().parameters;
     // A forward callee's parameters may not have been budgeted yet.
     require(args.size()==parameters.size(),"opcode type/arity mismatch");
     require(same(i.type,callee.result,i.where),"opcode type/arity mismatch");
     budget.work(i.where); // Same list-check charge as the other signatures.
     for(std::size_t n=0;n<args.size();++n)
      require(same(args[n],parameters[n].type,i.where),"opcode type/arity mismatch");
     break;
    }
     case Op::Print:require(i.type==Type::Void&&args.size()==1&&(scalar(args[0])||args[0].is_numeric()),"invalid print signature");break;
    case Op::Assert:signature(Type::Void,{Type::Bool});break;
    case Op::FloatAdd:case Op::FloatSub:case Op::FloatMul:case Op::FloatDiv:signature(Type::F64,{Type::F64,Type::F64});break;
    case Op::FloatNeg:signature(Type::F64,{Type::F64});break;
    case Op::FloatEq:case Op::FloatNe:case Op::FloatLt:case Op::FloatLe:case Op::FloatGt:case Op::FloatGe:signature(Type::Bool,{Type::F64,Type::F64});break;
    case Op::FloatFromI64:case Op::FloatFromBits:signature(Type::F64,{Type::I64});break;
    case Op::FloatToI64:case Op::FloatToBits:signature(Type::I64,{Type::F64});break;
    case Op::FloatIsNaN:case Op::FloatIsFinite:case Op::FloatSignbit:signature(Type::Bool,{Type::F64});break;
    case Op::Tuple:require(i.type.is_tuple()&&same_list(i.type.elements(),args,i.where),"tuple field type/arity mismatch");break;
     case Op::Extract:
     require(args.size()==1&&args[0].is_tuple(),"extract requires a tuple");
     require(i.immediate>=0&&static_cast<std::uint64_t>(i.immediate)<args[0].elements().size(),"extract index out of bounds");
      require(same(i.type,args[0].elements()[static_cast<std::size_t>(i.immediate)],i.where),"extract result type mismatch");break;
     case Op::Numeric:{
      auto descriptor=detail::numeric_signature(i.primitive);require(descriptor.has_value(),"unknown numeric primitive");
      signature(descriptor->result(),descriptor->parameters());break;
     }
     case Op::OwnerCreate:case Op::OwnerMove:case Op::OwnerDrop:
      fail(i.where,"unexpected nominal owner opcode");
    }
   }
   const auto& t=b.exit;++position;
   if(live)fail(t.where,"normal exit with a live nominal owner; explicit drop required");
   if(t.effect!=token)fail(t.where,"terminator drops/reorders effects");
   if(t.kind==Exit::Branch){if(use(t.value,b.id,position,t.where)!=Type::Bool)fail(t.where,"branch requires bool");}
   else if(t.kind==Exit::Return&&f.result!=Type::Void){if(!same(use(t.value,b.id,position,t.where),f.result,t.where))fail(t.where,"return type mismatch");}
   else if(t.value)fail(t.where,"unexpected terminator value");
   for(const auto& e:t.edges)for(std::size_t n=0;n<e.arguments.size();++n)
    if(!same(use(e.arguments[n],b.id,position,t.where),blocks.at(e.block)->parameters[n].type,t.where))fail(t.where,"block argument type mismatch");
  }
  if(nominal_owner)continue; // No slots exist in this profile.
  // Forward must-initialization: greatest fixed point, with an empty entry.
  // A loop backedge must never justify an uninitialized first-iteration load.
  budget.work(f.where,slots.size());
  std::set<Id> all;for(const auto& [id,type]:slots){(void)type;all.insert(id);}
  std::map<Id,std::set<Id>> initialized;
  for(const auto& b:f.blocks){budget.work(b.where,all.size());initialized[b.id]=all;}
  auto incoming=[&](const Block& b){
   std::set<Id> state;
   if(b.id!=f.blocks.front().id){
    budget.work(b.where,all.size());state=all;
    for(auto p:predecessors.at(b.id)){
     if(state.empty())break;
     budget.work(b.where);
     budget.work(b.where,state.size());
     const auto& prior=initialized.at(p);
     for(auto it=state.begin();it!=state.end();)if(!prior.contains(*it))it=state.erase(it);else ++it;
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
    auto& previous=initialized.at(b.id);
    if(state.size()==previous.size()){
     budget.work(b.where,state.size());
     if(state==previous)continue;
    }
    previous=std::move(state);changed=true;
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
 if(nominal_owner&&!has_create)fail(module_at,"nominal-owner MIR requires an owner.create");
}
}
void verify(const Module& module,Limits limits){verify_profile(module,limits,false);}
void verify_nominal_owner(const Module& module,Limits limits){verify_profile(module,limits,true);}
}
