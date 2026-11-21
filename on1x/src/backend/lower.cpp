#include <on1x/backend/mir.hpp>
#include <on1x/extensions.hpp>
#include <on1x/nominal.hpp>
#include <on1x/meta.hpp>
#include <on1x/numeric.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/concurrency.hpp>
#include <on1x/cliche.hpp>
#include <on1x/inherent.hpp>
#include <map>
#include <limits>
#include <algorithm>
#include "numeric.hpp"

namespace on1x::mir {
namespace {
class Lowering {
 const Unit& unit;Limits limits;Module module;Id serial=1;
 std::size_t function=0,current=0;
 static constexpr auto ended=std::numeric_limits<std::size_t>::max();
 struct Binding {Id id;Type type;bool slot;};
 std::vector<std::map<std::string,Binding>> scopes;
 std::map<std::string,Id> functions;
 struct LoopTarget {
  std::size_t id,done,header,latch=ended;
  const Location* step=nullptr;
 };
 struct RegionTarget {std::size_t id;const Location& where;std::size_t done=ended;};
 std::vector<LoopTarget> loops;
  std::vector<RegionTarget> regions;
  // Optional shape recognition has its own finite allowance. Exhaustion falls
  // back to ordinary storage lowering, never bypasses HIR/MIR verification.
  std::size_t decision_work;
 [[noreturn]] void unsupported(const Location& at,const std::string& why){
  throw Error(at,"ON1X-MIR-UNSUPPORTED: "+why,"ON1X-MIR-UNSUPPORTED");
 }
 Id fresh(const Location& at){if(serial>=limits.nodes)throw Error(at,"ON1X-MIR-LOWER: node budget exhausted");return ++serial;}
 unsigned type_version(const Type& t,const Location& at,bool allow_void=false){
  if(t.contains_unique())
   unsupported(at,"unique ownership requires typed move/drop and cleanup MIR events (direct C11 HIR backend only)");
  if(t==Type::F32||t.is_unsigned())return numeric_format_version;
  if(t==Type::F64)return format_version;
  if(t.is_tuple()){
   unsigned version=format_version;
   for(const auto& element:t.elements())version=std::max(version,type_version(element,at));
   return version;
  }
  if(t==Type::I64||t==Type::Bool||(allow_void&&t==Type::Void))return legacy_format_version;
  unsupported(at,"scalar MIR cannot lower "+type_name(t));
 }
 void scalar(const Type& t,const Location& at,bool allow_void=false){
  module.version=std::max(module.version,type_version(t,at,allow_void));
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
 // An omitted inner step can contain a syntactic continue to an outer loop.
 // Allocate a latch only when we actually emit an edge to it.
 std::size_t continue_block(LoopTarget& loop){
  if(!loop.step)return loop.header;
  if(loop.latch==ended)loop.latch=make_block(*loop.step);
  return loop.latch;
 }
  Id emit(Op op,Type type,std::vector<Id> args,const Location& at,Id target=0,std::int64_t immediate=0){
  scalar(type,at,true);Instruction i;i.id=fresh(at);i.type=type;i.op=op;i.operands=std::move(args);i.where=at;i.target=target;i.immediate=immediate;
  if(effect(op)!=Effect::Pure){i.effect_in=token();i.effect_out=fresh(at);}
  auto result=type==Type::Void?0:i.id;block().instructions.push_back(std::move(i));return result;
 }
  Id numeric(const std::string& name,Type type,std::vector<Id> args,const Location& at){
   if(!detail::numeric_signature(name))unsupported(at,"unknown numeric operation: "+name);
   module.version=numeric_format_version;
   auto result=emit(Op::Numeric,type,std::move(args),at);block().instructions.back().primitive=name;return result;
  }
   Binding binding(const std::string& name,const Location& at){
   for(auto it=scopes.rbegin();it!=scopes.rend();++it)if(auto found=it->find(name);found!=it->end())return found->second;
   unsupported(at,"unresolved local "+name);
  }
  struct Call {Op op;Id target=0;unsigned version=legacy_format_version;};
  Call call(const Expr& e){
   if(!e.callee||e.callee->kind!=Expr::Kind::Variable)
    unsupported(e.where,"indirect or raw foreign call requires closure/foreign MIR descriptors");
   const auto& c=*e.callee;
   if(c.reference==Expr::Reference::Function){
    if(!functions.contains(c.text))unsupported(e.where,"unresolved direct call");
    return {Op::Call,functions.at(c.text)};
   }
   if(c.reference!=Expr::Reference::Intrinsic)
    unsupported(e.where,"indirect or raw foreign call requires closure/foreign MIR descriptors");
   if(c.text=="println")return {Op::Print};
   if(c.text=="assert")return {Op::Assert};
   static const std::map<std::string,Op> floats={{"f64_from_i64",Op::FloatFromI64},{"f64_to_i64",Op::FloatToI64},
    {"f64_from_bits",Op::FloatFromBits},{"f64_to_bits",Op::FloatToBits},{"f64_is_nan",Op::FloatIsNaN},
    {"f64_is_finite",Op::FloatIsFinite},{"f64_signbit",Op::FloatSignbit}};
   if(auto op=floats.find(c.text);op!=floats.end())return {op->second,0,format_version};
   if(numeric_primitive(c.text))return {Op::Numeric};
   unsupported(e.where,"intrinsic needs an explicit effect/resource descriptor: "+c.text);
  }
  unsigned expression_profile(const Expr& e){
   using E=Expr::Kind;
   if(e.kind==E::RegexLiteral||e.kind==E::RegexMatch||e.kind==E::RegexCapture)
    unsupported(e.where,"core regex operations require the direct C11 backend");
   auto version=type_version(e.type,e.where,true);
   switch(e.kind){
    case E::Unit:case E::Region:case E::Project:case E::Integer:case E::Boolean:
    case E::Float:case E::Tuple:case E::Unary:case E::Binary:case E::Call:return version;
    case E::Variable:
     if(e.reference!=Expr::Reference::Local)unsupported(e.where,"first-class function values");
     return version;
    default:unsupported(e.where,"expression requires aggregate, closure, resource or stage lowering");
   }
  }
  void statement_profile(const Stmt& s){
   switch(s.kind){
    case Stmt::Kind::Defer:case Stmt::Kind::PushDefer:
     unsupported(s.where,"dynamic defer effects require cleanup MIR descriptors");
    case Stmt::Kind::TaskGroup:case Stmt::Kind::CspScope:
     unsupported(s.where,"cooperative concurrency requires effectful/ownership MIR descriptors");
    case Stmt::Kind::Range:unsupported(s.where,"unlowered range");
    default:break;
   }
   if(s.ref_store)unsupported(s.where,"reference stores require loan MIR descriptors");
  }
  // An omitted latch contributes neither blocks nor a wire-version upgrade.
  // Still enforce the source profile there, including nested latch and region
  // bodies. Retained generic templates are not executable HIR; their concrete
  // instances are in the statement body.
  void check_omitted(const std::vector<Stmt>& statements){
   auto visit=[&](auto&& self,const Expr& e)->void{
    (void)expression_profile(e);
    if(e.kind==Expr::Kind::Call)(void)call(e);
    for(const auto& child:e.children)self(self,*child);
    check_omitted(e.body);
   };
   for(const auto& s:statements){
    if(s.prototype)continue;
    statement_profile(s);
    if(s.kind==Stmt::Kind::Storage)(void)type_version(s.declared,s.where);
    if(s.value&&s.kind!=Stmt::Kind::Template)visit(visit,*s.value);
    check_omitted(s.body);
    check_omitted(s.otherwise);
   }
  }
   bool independent_body(const std::vector<Stmt>& statements,const std::string& name,std::size_t depth){
    for(const auto& s:statements){
     if(depth>128||!decision_work)return false;
     --decision_work;
     if(s.name==name||s.prototype||s.pattern||s.kind==Stmt::Kind::Template)return false;
     if(s.value&&!independent_expression(*s.value,name,depth+1))return false;
     if(!independent_body(s.body,name,depth+1)||!independent_body(s.otherwise,name,depth+1))return false;
    }
    return true;
   }
   bool independent_expression(const Expr& e,const std::string& name,std::size_t depth=0){
    if(depth>128||!decision_work)return false;
    --decision_work;
    if(e.prototype||!e.captures.empty()||!e.proofs.empty()||
       (e.kind==Expr::Kind::Variable&&e.text==name))return false;
    if(e.callee&&!independent_expression(*e.callee,name,depth+1))return false;
    for(const auto& child:e.children)
     if(!independent_expression(*child,name,depth+1))return false;
    return independent_body(e.body,name,depth+1);
   }
   bool boolean_decision(const Expr& e){
    // verify_values has already checked all metadata, initialization, lexical
    // references and ownership. Only erase this exact non-addressable bool
    // temporary, never a general mutable binding or a resource-bearing value.
    if(e.type!=Type::Bool||e.noreturn||e.prototype||e.unsafe_region||
       e.body.size()!=2||e.children.size()!=1)return false;
    const auto& storage=e.body[0];const auto& branch=e.body[1];
    const auto& result=*e.children[0];
    if(storage.kind!=Stmt::Kind::Storage||storage.declared!=Type::Bool||storage.prototype||
       branch.kind!=Stmt::Kind::If||branch.prototype||!branch.value||
       branch.value->type!=Type::Bool||branch.body.size()!=1||branch.otherwise.size()!=1||
       result.kind!=Expr::Kind::Variable||result.reference!=Expr::Reference::Local||
       result.type!=Type::Bool||result.text!=storage.name)return false;
    for(const auto* arm:{&branch.body[0],&branch.otherwise[0]})
     if(arm->kind!=Stmt::Kind::Initialize||arm->prototype||arm->ref_store||
        arm->name!=storage.name||!arm->value||
        (arm->value->type!=Type::Bool&&!arm->value->noreturn))return false;
    return independent_expression(*branch.value,storage.name)&&
      independent_expression(*branch.body[0].value,storage.name)&&
      independent_expression(*branch.otherwise[0].value,storage.name);
   }
   Id decision(const Expr& e){
    const auto& branch=e.body[1];
    const auto& yes_value=*branch.body[0].value;
    const auto& no_value=*branch.otherwise[0].value;
    auto condition=expression(*branch.value);
    if(current==ended)return 0;
    // On a literal identity arm the condition itself is the selected value.
    // It is already evaluated once, including any loads/calls/traps.
    bool yes_identity=yes_value.kind==Expr::Kind::Boolean&&yes_value.text=="true";
    bool no_identity=no_value.kind==Expr::Kind::Boolean&&no_value.text=="false";
    auto merge=yes_identity||no_identity?make_block(e.where):ended;
    auto yes=yes_identity?merge:make_block(branch.where);
    auto no=no_identity?merge:make_block(branch.where);
    terminate(Exit::Branch,condition,
      {yes_identity?edge(yes,{condition}):edge(yes),
       no_identity?edge(no,{condition}):edge(no)},branch.where);
    Id yes_result=0,no_result=0;
    auto yes_tail=ended,no_tail=ended;
    if(!yes_identity){current=yes;yes_result=expression(yes_value);yes_tail=current;}
    if(!no_identity){current=no;no_result=expression(no_value);no_tail=current;}
    if(merge==ended){
     if(yes_tail==ended&&no_tail==ended){current=ended;return 0;}
     merge=make_block(e.where);
    }
    auto result=fresh(e.children[0]->where);
    fn().blocks[merge].parameters.push_back({result,Type::Bool,e.children[0]->where});
    if(yes_tail!=ended){current=yes_tail;jump(merge,branch.body[0].where,{yes_result});}
    if(no_tail!=ended){current=no_tail;jump(merge,branch.otherwise[0].where,{no_result});}
    current=merge;return result;
   }
   Id expression(const Expr& e){
   module.version=std::max(module.version,expression_profile(e));
   switch(e.kind){
   case Expr::Kind::Unit:return 0;
    case Expr::Kind::Region:{
     if(boolean_decision(e))return decision(e);
     scopes.emplace_back();body(e.body,false);auto result=current==ended?Id{0}:expression(*e.children.at(0));scopes.pop_back();return result;
    }
   case Expr::Kind::Project:{auto root=expression(*e.children.at(0));return emit(Op::Extract,e.type,{root},e.where,0,static_cast<std::int64_t>(e.projection));}
   case Expr::Kind::Integer:{
    return emit(Op::Constant,e.type,{},e.where,0,integer_literal_bits(e.text,e.where));
  }
   case Expr::Kind::Boolean:return emit(Op::Constant,Type::Bool,{},e.where,0,e.text=="true");
    case Expr::Kind::Float:return emit(Op::Constant,e.type,{},e.where,0,float_literal_bits(e.text,e.where));
   case Expr::Kind::Tuple:{
    std::vector<Id> fields;for(const auto& child:e.children)fields.push_back(expression(*child));
    return emit(Op::Tuple,e.type,std::move(fields),e.where);
   }
   case Expr::Kind::Variable:{
    auto b=binding(e.text,e.where);return b.slot?emit(Op::Load,b.type,{},e.where,b.id):b.id;
  }
   case Expr::Kind::Unary:{auto operand=expression(*e.children.at(0));
    if(e.type==Type::F32)return numeric("neg_f32",e.type,{operand},e.where);
    return emit(e.type==Type::F64?Op::FloatNeg:e.text=="-"?Op::Neg:Op::Not,e.type,{operand},e.where);}
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
     if(e.children[0]->type==Type::F32||e.children[0]->type.is_unsigned()){
      static const std::map<std::string,std::string> names={{"+","add"},{"-","sub"},{"*","mul"},{"/","div"},{"%","mod"},
       {"==","eq"},{"!=","ne"},{"<","lt"},{"<=","le"},{">","gt"},{">=","ge"}};
      return numeric(names.at(e.text)+"_"+type_name(e.children[0]->type),e.type,{left,right},e.where);
     }
    if(e.children[0]->type==Type::F64){
     static const std::map<std::string,Op> floats={{"+",Op::FloatAdd},{"-",Op::FloatSub},{"*",Op::FloatMul},{"/",Op::FloatDiv},
      {"==",Op::FloatEq},{"!=",Op::FloatNe},{"<",Op::FloatLt},{"<=",Op::FloatLe},{">",Op::FloatGt},{">=",Op::FloatGe}};
     return emit(floats.at(e.text),e.type,{left,right},e.where);
    }
   static const std::map<std::string,Op> ops={{"+",Op::Add},{"-",Op::Sub},{"*",Op::Mul},{"/",Op::Div},{"%",Op::Mod},
    {"==",Op::Eq},{"!=",Op::Ne},{"<",Op::Lt},{"<=",Op::Le},{">",Op::Gt},{">=",Op::Ge}};
   auto found=ops.find(e.text);if(found==ops.end())unsupported(e.where,"unknown binary operator");
   return emit(found->second,e.type,{left,right},e.where);
   }
   case Expr::Kind::Call:{
    auto descriptor=call(e);
    std::vector<Id> args;for(const auto& a:e.children)args.push_back(expression(*a));
    if(descriptor.op==Op::Numeric)return numeric(e.callee->text,e.type,std::move(args),e.where);
    module.version=std::max(module.version,descriptor.version);
    return emit(descriptor.op,e.type,std::move(args),e.where,descriptor.target);
  }
   default:unsupported(e.where,"expression requires aggregate, closure, resource or stage lowering");
   }
  }
  void bind_pattern(const Pattern& pattern,Type type,Id value){
   if(!pattern.is_tuple()){scopes.back().emplace(pattern.name,Binding{value,type,false});return;}
   for(std::size_t i=0;i<pattern.elements.size();++i){
    auto field=type.elements()[i];auto extracted=emit(Op::Extract,field,{value},pattern.elements[i].where,0,static_cast<std::int64_t>(i));
    bind_pattern(pattern.elements[i],field,extracted);
   }
  }
 void body(const std::vector<Stmt>& statements,bool scope=true){
  if(scope)scopes.emplace_back();
   for(const auto& s:statements){
    if(s.prototype)continue;
    if(current==ended)unsupported(s.where,"unreachable HIR statement");
    statement_profile(s);
     switch(s.kind){
      case Stmt::Kind::Defer:case Stmt::Kind::PushDefer:
      case Stmt::Kind::TaskGroup:case Stmt::Kind::CspScope:case Stmt::Kind::Range:
       throw std::logic_error("unsupported statement passed MIR profile check");
   case Stmt::Kind::Let:{
    auto value=expression(*s.value);scalar(s.value->type,s.where);
    if(s.pattern){bind_pattern(*s.pattern,s.value->type,value);break;}
    Binding b{value,s.value->type,false};
    if(s.mutable_binding){b.id=fresh(s.where);b.slot=true;fn().slots.push_back({b.id,b.type,s.where});emit(Op::Store,Type::Void,{value},s.where,b.id);}
    scopes.back().emplace(s.name,b);break;
   }
   case Stmt::Kind::Storage:{scalar(s.declared,s.where);auto id=fresh(s.where);fn().slots.push_back({id,s.declared,s.where});scopes.back().emplace(s.name,Binding{id,s.declared,true});break;}
   case Stmt::Kind::Initialize:case Stmt::Kind::Assign:{
    auto b=binding(s.name,s.where);if(!b.slot)unsupported(s.where,"assignment to immutable HIR binding");
    auto value=expression(*s.value);emit(Op::Store,Type::Void,{value},s.where,b.id);break;
   }
   case Stmt::Kind::Return:{auto value=s.value?expression(*s.value):0;if(current!=ended)terminate(Exit::Return,value,{},s.where);break;}
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
      loops.push_back({s.control_target,done,header,ended,s.otherwise.empty()?nullptr:&s.where});
      current=loop;body(s.body);
      if(current!=ended)jump(continue_block(loops.back()),s.where);
      auto latch=loops.back().latch;loops.pop_back();
      if(latch!=ended){
       current=latch;body(s.otherwise);if(current!=ended)jump(header,s.where);
      }else check_omitted(s.otherwise);
      current=done;break;
     }
    case Stmt::Kind::Break:case Stmt::Kind::Continue:{
      auto target=std::find_if(loops.rbegin(),loops.rend(),[&](const auto& t){return t.id==s.control_target;});
      if(target==loops.rend())unsupported(s.where,"missing canonical loop target");
      jump(s.kind==Stmt::Kind::Break?target->done:continue_block(*target),s.where);break;
     }
     case Stmt::Kind::Region:{
      regions.push_back({s.control_target,s.where});body(s.body);
      auto done=regions.back().done;regions.pop_back();
      if(done!=ended){if(current!=ended)jump(done,s.where);current=done;}break;
     }
     case Stmt::Kind::Leave:{
      auto target=std::find_if(regions.rbegin(),regions.rend(),[&](const auto& t){return t.id==s.control_target;});
      if(target==regions.rend())unsupported(s.where,"missing region join");
      if(target->done==ended)target->done=make_block(target->where);
      jump(target->done,s.where);break;
     }
    case Stmt::Kind::Trap:{auto condition=emit(Op::Constant,Type::Bool,{},s.where,0,0);emit(Op::Assert,Type::Void,{condition},s.where);auto trap=current;jump(trap,s.where);break;}
   }
  }
  if(scope)scopes.pop_back();
 }
public:
  Lowering(const Unit& u,Limits l):unit(u),limits(l),decision_work(std::min<std::size_t>(l.work,262144)){}
   Module run(){
      inherent::verify_elaborated(unit);
      cliches::verify_elaborated(unit);
      csp::verify(unit,true);
     for(const auto& declaration:unit.nominals)
      for(const auto& field:declaration.fields)if(field.contains_unique())
       unsupported(declaration.where,"unique ownership requires typed move/drop and cleanup MIR events (direct C11 HIR backend only)");
     if(csp::present(unit))unsupported(unit.functions.front().where,"cooperative concurrency requires effectful/ownership MIR descriptors (C11 HIR backend only)");
    canonical::verify_control(unit);
    meta::require_core(unit);
    canonical::verify_values(unit);
    verify_nominals(unit);
  for(const auto& f:unit.functions){
   if(f.prototype)continue;
    if(takes_borrows(f))
     unsupported(f.where,
       "borrowed parameters require addressable-place and loan MIR descriptors "
       "(C11 HIR backend only)");
   if(f.external)unsupported(f.where,"raw extern declarations require typed foreign MIR descriptors");
   scalar(f.result,f.where,true);Function target;target.id=fresh(f.where);target.name=f.name;target.result=f.result;target.where=f.where;
   functions.emplace(f.name,target.id);module.functions.push_back(std::move(target));
  }
  for(const auto& f:unit.functions){
   if(f.prototype)continue;
   current=make_block(f.where);scopes.emplace_back();
   for(const auto& p:f.params){
    scalar(p.type,p.where);auto id=fresh(p.where);block().parameters.push_back({id,p.type,p.where});scopes.back().emplace(p.name,Binding{id,p.type,false});
    if(p.pattern)bind_pattern(*p.pattern,p.type,id);
   }
   body(f.body,false);
   if(current!=ended){if(f.result!=Type::Void)unsupported(f.where,"missing return in checked HIR");terminate(Exit::Return,0,{},f.where);}
   scopes.clear();++function;
  }
  verify(module,limits);return std::move(module);
 }
};
}
Module lower(const Unit& unit,Limits limits){extensions::verify_evidence(unit);return Lowering(unit,limits).run();}
}
