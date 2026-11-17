#include <on1x/ast.hpp>
#include <on1x/infer.hpp>
#include <on1x/meta.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/patterns.hpp>
#include <on1x/nominal.hpp>
#include <on1x/primitives.hpp>
#include <on1x/numeric.hpp>
#include <on1x/concurrency.hpp>
#include <charconv>
#include <climits>
#include <map>
#include <system_error>
#include <set>
#include <algorithm>
#include <bit>
namespace on1x {
namespace {
void extern_leaves(const Pattern& pattern,const Type& type,std::vector<Type>& types,std::set<std::string>& names){
 patterns::require_binding(pattern);
 if(pattern.is_tuple()){
  if(!type.is_tuple()||type.elements().size()!=pattern.elements.size())throw Error(pattern.where,"extern pattern must match its concrete tuple annotation");
  for(std::size_t i=0;i<pattern.elements.size();++i)extern_leaves(pattern.elements[i],type.elements()[i],types,names);
 }else{
  if(!names.insert(pattern.name).second)throw Error(pattern.where,"duplicate extern parameter: "+pattern.name);
    if(!type.is_numeric()&&type!=Type::Bool&&type!=Type::String)throw Error(pattern.where,"flattened extern leaves must be numeric, bool or string");
  types.push_back(type);
 }
}
}
std::vector<Type> extern_parameter_types(const Function& function){
 std::vector<Type> types;std::set<std::string> names;
 for(const auto& p:function.params){
   if(has_type_holes(p.type)||p.type.has_foralls())throw Error(p.where,"extern signatures require concrete monotypes; inference is not ABI evidence");
  if(p.pattern)extern_leaves(*p.pattern,p.type,types,names);
  else{
   if(!names.insert(p.name).second)throw Error(p.where,"duplicate extern parameter: "+p.name);
   if(p.type.is_tuple())throw Error(p.where,"tuple-valued extern ABI requires an explicit flattening pattern");
   types.push_back(p.type);
  }
 }
 return types;
}
bool is_intrinsic_name(const std::string& name){
 static const std::set<std::string> names={"println","assert","json_parse_or_die","json_get_or_die","json_size","json_number","json_print"};
  return names.contains(name)||data_primitive(name).has_value();
}
namespace {
struct Binding { Type type; bool is_mutable; std::size_t scope; };
class Checker {
 std::vector<std::pair<std::size_t,bool>> regions;
  Unit& unit;
  NominalCatalog catalog;
 std::map<std::string,const Function*> funcs;
 std::map<std::string,Type> signatures;
 std::set<Type> checked_types;
 std::vector<std::map<std::string,Binding>> scopes;
 struct LambdaFrame { Expr* expression; std::size_t scope_begin; std::set<std::string> names; };
 std::vector<LambdaFrame> lambdas;
 std::size_t capture_count=0,capture_bytes=0;
  Type returns=Type::Void;
  std::size_t taskgroups=0;
  const Expr* launch_body=nullptr;
 [[noreturn]] void fail(const Location& loc,const std::string& msg){throw Error(loc,msg,"ON1X-SEMA");}
 void push(){scopes.emplace_back();}void pop(){scopes.pop_back();}
 const Binding* lookup(const std::string& name)const{
  for(auto it=scopes.rbegin();it!=scopes.rend();++it){auto hit=it->find(name);if(hit!=it->end())return &hit->second;}
  return nullptr;
 }
 const Binding& find(const std::string& name,const Location& loc){
  if(auto binding=lookup(name)){capture(name,*binding,loc);return *binding;}
  fail(loc,"undefined variable: "+name);}
 void define(const std::string& name,Type type,bool mut,const Location& where){if(scopes.back().contains(name))fail(where,"duplicate local: "+name);
  scopes.back()[name]={type,mut,scopes.size()-1};}
 void capture(const std::string& name,const Binding& binding,const Location& loc){
  for(auto frame=lambdas.rbegin();frame!=lambdas.rend();++frame){
   if(binding.scope>=frame->scope_begin)break;
   if(binding.is_mutable)fail(loc,"capturing mutable binding is unsupported; use an immutable let: "+name);
   if(resource(binding.type))fail(loc,"capturing JSON resources or views requires lifetime analysis: "+name);
    if(binding.type.is_csp()&&(frame->expression!=launch_body||
       (binding.type!=Type::Sender&&binding.type!=Type::Receiver&&binding.type!=Type::ByteSender&&binding.type!=Type::ByteReceiver)))
     fail(loc,"concurrency capabilities may only move into a launch body: "+name);
   auto& captures=frame->expression->captures;
   if(frame->names.contains(name))continue;
   if(capture_count>=16384||name.size()>max_source_bytes-capture_bytes)
    fail(loc,"closure capture metadata limit exceeded");
   frame->names.insert(name);
   ++capture_count;capture_bytes+=name.size();captures.push_back({name,binding.type});
  }
 }
  static bool resource(Type t){return t==Type::Json||t==Type::JsonView;}
  struct LoopValue {std::size_t target;Type type=Type::Invalid;bool bare=false;};
  std::vector<LoopValue> loop_values;
   std::size_t value_width(const Type& t,const Location& at,std::size_t depth=0){
    catalog.verify(t,at);
    if(depth>32)fail(at,"data value nesting exceeds 32-level limit");
     if(t.is_numeric()||t==Type::Bool||t.kind()==Type::Generic||t.kind()==Type::Bound)return 1;
   if(!t.is_tuple()&&!t.is_data())fail(at,"value data fields and match results require trivially copied types");
   for(const auto& a:t.arguments())(void)value_width(a,at,depth+1);
   std::size_t width=1;
   if(t.kind()==Type::Array||t.kind()==Type::Slice){
    auto child=value_width(t.elements()[0],at,depth+1);auto count=std::max<std::size_t>(1,t.extent());
    if(child>4096/count)fail(at,"expanded data value exceeds 4096 storage-unit limit");
    width+=child*count+(t.kind()==Type::Slice?2:0);
   }else for(const auto& field:t.elements()){
    auto child=value_width(field,at,depth+1);if(child>4096-width)fail(at,"expanded data value exceeds 4096 storage-unit limit");width+=child;
   }
   if(width>4096)fail(at,"expanded data value exceeds 4096 storage-unit limit");
   return width;
  }
 void equal(Type actual,Type expected,const Location& loc){if(actual!=expected)throw Error(loc,std::string("expected ")+type_name(expected)+", got "+type_name(actual),"ON1X-TYPE");}
   void validate_type(const Type& type,const Location& loc){
    catalog.verify(type,loc);
   if(type.has_holes())fail(loc,"unresolved inference hole in checked type");
    if(type.is_forall()){validate_type(type.body(),loc);return;}
    if(type.is_code()){
     if(resource(type.code_result()))fail(loc,"code values cannot return JSON resources or views");
     validate_type(type.code_result(),loc);return;
    }
   if(type.is_data()){
    if(checked_types.contains(type))return;
    (void)value_width(type,loc);checked_types.insert(type);return;
   }
   if(!type.is_function()&&!type.is_tuple())return;
  if(checked_types.contains(type))return;
  if(type.is_tuple()){
   for(const auto& element:type.elements()){
    if(element==Type::Void)fail(loc,"tuple elements cannot be void");
    if(resource(element))fail(loc,"tuple elements cannot contain JSON resources or views");
    validate_type(element,loc);
   }
   }else{
    for(const auto& p:type.parameters())if(csp::contains(p))fail(loc,"concurrency handles cannot cross ordinary function signatures");
    if(csp::contains(type.result()))fail(loc,"concurrency handles cannot escape through function results");
   for(const auto& parameter:type.parameters()){
    if(parameter==Type::Void)fail(loc,"function parameter type cannot be void");
    if(resource(parameter))fail(loc,"function types cannot contain resource parameters");
    validate_type(parameter,loc);
   }
   if(resource(type.result()))fail(loc,"function types cannot contain resource returns");
   validate_type(type.result(),loc);
  }
  checked_types.insert(type);
 }
  void bind_pattern(const Pattern& pattern,const Type& type){
   patterns::require_binding(pattern);
  if(!pattern.is_tuple()){define(pattern.name,type,false,pattern.where);return;}
  if(!type.is_tuple())fail(pattern.where,"tuple binding pattern requires a tuple, got "+type_name(type));
  if(pattern.elements.size()!=type.elements().size())fail(pattern.where,"tuple binding pattern has wrong arity");
  for(std::size_t i=0;i<pattern.elements.size();++i)bind_pattern(pattern.elements[i],type.elements()[i]);
 }
 Type resolve_name(Expr& e,bool call_position=false){
  if(auto binding=lookup(e.text)){
   capture(e.text,*binding,e.where);
   e.reference=Expr::Reference::Local;return e.type=binding->type;
  }
  if(auto found=funcs.find(e.text);found!=funcs.end()){
   if(found->second->external&&!call_position)
    fail(e.where,"raw extern functions cannot be used as values; write an On1x wrapper");
   e.reference=found->second->external?Expr::Reference::Extern:Expr::Reference::Function;
   return e.type=signatures.at(e.text);
  }
  if(is_intrinsic_name(e.text)){
   if(!call_position)fail(e.where,"intrinsic is not a first-class value; write an On1x wrapper: "+e.text);
   e.reference=Expr::Reference::Intrinsic;return e.type=Type::Invalid;
  }
  fail(e.where,(call_position?"unknown function: ":"undefined variable: ")+e.text);
 }
 Type check_intrinsic(Expr& e,const std::vector<Type>& arg){
  const auto& n=e.callee->text;
   auto arity=[&](std::size_t k){if(arg.size()!=k)fail(e.where,"wrong arity for "+n);};
   if(auto primitive=data_primitive(n)){
    arity(primitive->parameters().size());for(std::size_t i=0;i<arg.size();++i)equal(arg[i],primitive->parameters()[i],e.where);
    return e.type=primitive->result();
   }
    if(n=="println"){arity(1);if(!arg[0].is_numeric()&&arg[0]!=Type::Bool&&arg[0]!=Type::String)fail(e.where,"println accepts numeric scalars, bool or string");return e.type=Type::Void;}
  if(n=="assert"){arity(1);equal(arg[0],Type::Bool,e.where);return e.type=Type::Void;}
  if(n=="json_parse_or_die"){arity(1);equal(arg[0],Type::String,e.where);return e.type=Type::Json;}
  if(n=="json_get_or_die"){arity(2);
   if(arg[0]!=Type::Json&&arg[0]!=Type::JsonView)fail(e.where,"json_get_or_die expects json or json_view");
   if(e.children[0]->kind!=Expr::Kind::Variable)fail(e.where,"JSON borrow needs a named live owner or view (temporary would dangle)");
   equal(arg[1],Type::String,e.where);return e.type=Type::JsonView;
  }
  if(n=="json_size"||n=="json_number"){
   arity(1);if(arg[0]!=Type::Json&&arg[0]!=Type::JsonView)fail(e.where,n+" expects json or json_view");
   if(e.children[0]->kind!=Expr::Kind::Variable)fail(e.where,"JSON operation requires a named live owner or view");
   return e.type=Type::I64;
  }
  if(n=="json_print"){arity(1);
   if(arg[0]!=Type::Json&&arg[0]!=Type::JsonView)fail(e.where,"json_print expects json or json_view");
   if(e.children[0]->kind!=Expr::Kind::Variable)fail(e.where,"JSON operation requires a named live owner or view");
   return e.type=Type::Void;
  }
  fail(e.where,"unsupported intrinsic: "+n);
 }
 Type check_expr(Expr& e){
  auto child=[&](std::size_t i){return check_expr(*e.children.at(i));};
       switch(e.kind){
        case Expr::Kind::SourceCsp:case Expr::Kind::Csp:{
         auto arity=[&](std::size_t n){if(e.children.size()!=n)fail(e.where,"concurrency operand arity mismatch");};
         if(e.text=="poll"){
          if(e.children.empty()||e.children.size()>17)fail(e.where,"poll offer limit/arity mismatch");
          std::vector<Type> fields;std::vector<std::string> labels;
          for(std::size_t i=0;i<e.children.size();++i){auto& offer=*e.children[i];
           if(offer.kind==Expr::Kind::Boolean&&offer.text=="true"&&i+1==e.children.size()){
            fields.push_back(Type::data(Type::Record,{}));labels.push_back("Default");break;
           }
           if(offer.kind!=Expr::Kind::SourceCsp||(offer.text!="offer_recv"&&offer.text!="offer_send")||
              offer.children.size()!=(offer.text=="offer_recv"?1u:2u))fail(offer.where,"invalid poll offer");
           offer.type=Type::Void;
           equal(check_expr(*offer.children[0]),offer.text=="offer_recv"?Type::Receiver:Type::Sender,offer.where);
           if(offer.text=="offer_send")equal(check_expr(*offer.children[1]),Type::I64,offer.where);
           fields.push_back(offer.text=="offer_recv"?Type{Type::I64}:Type::data(Type::Record,{}));
           labels.push_back((offer.text=="offer_recv"?"Recv":"Sent")+std::to_string(i));
           fields.push_back(Type::data(Type::Record,{}));labels.push_back("Closed"+std::to_string(i));
          }
          return e.type=Type::data(Type::Sum,std::move(fields),std::move(labels));
         }
         if(e.text=="launch"){
         arity(1);if(!taskgroups||e.children[0]->kind!=Expr::Kind::Lambda)fail(e.where,"launch requires an enclosing taskgroup and a source block");
         auto saved=launch_body;launch_body=e.children[0].get();equal(child(0),Type::function({},Type::I64),e.where);launch_body=saved;
          for(const auto& c:e.children[0]->captures)if(c.type!=Type::I64&&c.type!=Type::Sender&&c.type!=Type::Receiver&&c.type!=Type::ByteSender&&c.type!=Type::ByteReceiver)fail(e.where,"launch captures support only immutable i64 or scoped endpoints");
          return e.type=Type::Fiber;
         }
         if(e.text=="launch_exec"){
          if(!taskgroups||e.children.empty()||e.children.size()>9)fail(e.where,"external launch requires a taskgroup and 1..9 literal argv items");
          for(auto& arg:e.children){if(arg->kind!=Expr::Kind::String)fail(arg->where,"external argv must be constant strings");equal(check_expr(*arg),Type::String,arg->where);}
          return e.type=Type::ExternalFiber;
         }
         if(e.text=="launch_capture"){
          if(!taskgroups||e.children.empty()||e.children.size()>9)fail(e.where,"captured shell launch requires a taskgroup and 1..9 literal argv items");
          for(auto& arg:e.children){if(arg->kind!=Expr::Kind::String)fail(arg->where,"captured shell argv must be constant strings");equal(check_expr(*arg),Type::String,arg->where);}
          return e.type=Type::tuple({Type::ExternalFiber,Type::ByteSender,Type::ByteReceiver,Type::ByteReceiver});
         }
        if(e.text=="channel"){
         arity(3);if(!taskgroups||e.result!=Type::I64)fail(e.where,"channel requires a taskgroup and i64 payload");
         for(std::size_t i=0;i<3;++i){equal(child(i),Type::I64,e.where);if(e.children[i]->kind!=Expr::Kind::Integer)fail(e.where,"channel cardinality/capacity must be decimal literals");}
         if(integer_literal_bits(e.children[0]->text,e.where)!=1||integer_literal_bits(e.children[1]->text,e.where)!=1)fail(e.where,"ON1X-CSP-UNSUPPORTED: initial endpoint cardinalities must be (1,1)");
         auto capacity=integer_literal_bits(e.children[2]->text,e.where);if(capacity<0||capacity>256)fail(e.where,"channel capacity must be in 0..256");
         return e.type=Type::Channel;
        }
        arity(e.text=="send"?2:1);
        if(e.text=="split"){equal(child(0),Type::Channel,e.where);return e.type=Type::tuple({Type::Sender,Type::Receiver});}
         if(e.text=="join"){
          auto t=child(0);if(t!=Type::Fiber&&t!=Type::ExternalFiber)fail(e.where,"join requires an internal or external fiber");
          return e.type=Type::I64;
         }
         if(e.text=="cancel"){
          auto t=child(0);if(t!=Type::Fiber&&t!=Type::ExternalFiber)fail(e.where,"cancel requires an internal or external fiber");
          return e.type=Type::Bool;
         }
         if(e.text=="recv"){
          auto endpoint=child(0);if(endpoint!=Type::Receiver&&endpoint!=Type::ByteReceiver)fail(e.where,"receive expects receiver<i64> or receiver<u8>");
          return e.type=csp::receive_result(endpoint==Type::ByteReceiver?Type::U8:Type::I64);
         }
         if(e.text=="close"){
          auto endpoint=child(0);if(endpoint!=Type::Sender&&endpoint!=Type::ByteSender)fail(e.where,"close expects sender<i64> or sender<u8>");
          return e.type=Type::Bool;
         }
         if(e.text=="send"){
          auto endpoint=child(0);if(endpoint!=Type::Sender&&endpoint!=Type::ByteSender)fail(e.where,"send expects sender<i64> or sender<u8>");
          equal(child(1),endpoint==Type::ByteSender?Type::U8:Type::I64,e.where);return e.type=csp::send_result();
         }
        fail(e.where,"unknown concurrency operation");
       }
      case Expr::Kind::Propagate:{
       if(e.children.size()!=1)fail(e.where,"propagation requires one operand");
       auto input=child(0);if(input.kind()!=Type::Sum||input.nominal().empty())fail(e.where,"propagation requires a nominal Result");
       catalog.result(input.nominal(),e.where);
       if(returns.kind()!=Type::Sum||returns.nominal()!=input.nominal())fail(e.where,"propagation requires the same nominal Result return declaration");
       equal(input.arguments().at(1),returns.arguments().at(1),e.where);e.result=returns;
       return e.type=input.arguments().at(0);
      }
     case Expr::Kind::Splice:case Expr::Kind::Lift:fail(e.where,"MSP stage lowering required before checking");
     case Expr::Kind::Quote:{
      if(!unit.stages_lowered)fail(e.where,"MSP quotation requires stage lowering");
      auto type=child(0);if(!type.is_function()||!type.parameters().empty())fail(e.where,"invalid lowered quotation");
      auto code=Type::code(type.result());validate_type(code,e.where);return e.type=std::move(code);
     }
     case Expr::Kind::Run:{
      auto type=child(0);if(!type.is_code())fail(e.where,"run requires code<T>, got "+type_name(type));
      return e.type=type.code_result();
     }
    case Expr::Kind::MacroCall:case Expr::Kind::SyntaxHole:case Expr::Kind::SyntaxBlock:case Expr::Kind::Extension:case Expr::Kind::SyntaxList:case Expr::Kind::SyntaxVector:case Expr::Kind::SyntaxKeyword:case Expr::Kind::SyntaxLet:
     fail(e.where,"unexpanded metaprogramming syntax before type checking");
    case Expr::Kind::PolyProjection:{
     auto schema=child(0);
     if(!forall_instance(schema,e.type))fail(e.where,"invalid forall projection");
     if(!e.prototype){
      auto found=unit.forall_layouts.find(schema);
      if(found==unit.forall_layouts.end()||e.projection>=found->second.size()||found->second[e.projection]!=e.type)
       fail(e.where,"invalid forall projection layout");
     }
     validate_type(e.type,e.where);return e.type;
    }
    case Expr::Kind::PolyPack:{
     if(!e.type.is_forall()||e.proofs.size()!=1||!forall_proof(e.type,check_expr(*e.proofs.front())))
      fail(e.where,"universal value requires a rigid forall proof");
     validate_type(e.type,e.where);
     if(!e.prototype){
      const auto found=unit.forall_layouts.find(e.type);
      const std::vector<Type> empty;const auto& layout=found==unit.forall_layouts.end()?empty:found->second;
      if(e.children.size()!=layout.size())fail(e.where,"invalid forall pack layout");
      for(std::size_t i=0;i<layout.size();++i)equal(child(i),layout[i],e.where);
     }
     return e.type;
    }
      case Expr::Kind::Integer:{auto t=numeric_literal_type(e.text);auto v=integer_literal_bits(e.text,e.where);
       e.text=t.is_unsigned()?std::to_string(std::bit_cast<std::uint64_t>(v))+type_name(t):std::to_string(v);
       return e.type=t;}
     case Expr::Kind::Float:(void)float_literal_bits(e.text,e.where);return e.type=numeric_literal_type(e.text);
    case Expr::Kind::Boolean:return e.type=Type::Bool;
   case Expr::Kind::String:
    // Current string ABI is const char*: do not imply support for binary NUL.
     for(std::size_t i=1;i+1<e.text.size();++i)if(e.text[i]=='\\'){
      if(e.text[++i]=='0')fail(e.where,"NUL escape is not supported by the C-string ABI");
     }
    return e.type=Type::String;
     case Expr::Kind::Variable:return resolve_name(e);
      case Expr::Kind::Unit:if(!e.children.empty()||!e.body.empty())fail(e.where,"malformed unit value");return e.type=Type::Void;
      case Expr::Kind::Loop:{
       loop_values.push_back({e.control_target});(void)check_block(e.body);auto exits=loop_values.back();loop_values.pop_back();
       e.noreturn=false;auto result=exits.bare||exits.type==Type::Invalid?Type{Type::Void}:exits.type;validate_type(result,e.where);return e.type=result;
      }
     case Expr::Kind::PatternMatch:{
      if(e.children.size()<2)fail(e.where,"empty match is not exhaustive");
      auto t=check_expr(*e.children[0]);if(t==Type::Void)fail(e.where,"match scrutinee cannot be void");push();if(!e.text.empty())define(e.text,t,false,e.where);
      Type result=Type::Invalid;bool all_exit=true;
      for(std::size_t i=1;i<e.children.size();++i){auto& arm=*e.children[i];if(arm.kind!=Expr::Kind::Case||arm.patterns.size()!=1||arm.children.size()!=2)fail(arm.where,"malformed match arm");
       auto bindings=patterns::check(arm.patterns[0],t);push();for(const auto& [name,type]:bindings)define(name,type,false,arm.where);
       equal(check_expr(*arm.children[0]),Type::Bool,arm.where);push();bool exits=check_block(arm.body,false);auto value=check_expr(*arm.children[1]);
       if(exits&&arm.children[1]->kind!=Expr::Kind::Unit)fail(arm.where,"unreachable match arm result");
       if(resource(value))fail(arm.where,"match results cannot transfer or discard unique JSON resources/views");
       exits=exits||arm.children[1]->noreturn;
       if(!exits){all_exit=false;if(!e.statement_match){if(result==Type::Invalid)result=value;else equal(value,result,arm.where);}}
       pop();pop();
      }
      pop();patterns::coverage(e);e.noreturn=all_exit;return e.type=e.statement_match||result==Type::Invalid?Type{Type::Void}:result;
     }
     case Expr::Kind::Case:fail(e.where,"match arm outside a match");
     case Expr::Kind::Region:{
      if(e.children.size()!=1)fail(e.where,"canonical value region requires one result");
      push();bool exits=check_block(e.body,false);auto result=check_expr(*e.children[0]);pop();
      if(exits!=e.noreturn)fail(e.where,"canonical value region control mismatch");
      equal(result,e.type,e.where);return e.type;
     }
     case Expr::Kind::TagTest:case Expr::Kind::Payload:{
      if(e.children.size()!=1)fail(e.where,"sum control operation requires one operand");
      auto t=child(0);if(t.kind()!=Type::Sum)fail(e.where,"sum control operation requires a closed sum");auto tag=std::find(t.labels().begin(),t.labels().end(),e.text);if(tag==t.labels().end())fail(e.where,"unknown sum control tag");e.projection=static_cast<std::size_t>(tag-t.labels().begin());
      auto result=e.kind==Expr::Kind::TagTest?Type{Type::Bool}:t.elements()[e.projection];equal(e.type,result,e.where);return result;
     }
     case Expr::Kind::Project:{
      if(e.children.size()!=1)fail(e.where,"tuple projection requires one operand");
      auto t=child(0);if(!t.is_tuple()||e.projection>=t.elements().size())fail(e.where,"invalid canonical tuple projection");equal(e.type,t.elements()[e.projection],e.where);return e.type;
     }
     case Expr::Kind::Tuple:{
     std::vector<Type> elements;for(auto& element:e.children)elements.push_back(check_expr(*element));
     auto type=Type::tuple(std::move(elements));validate_type(type,e.where);return e.type=std::move(type);
    }
    case Expr::Kind::Lambda:{
     std::vector<Type> parameters;std::set<std::string> names;
     for(const auto& parameter:e.parameters){
      if(!names.insert(parameter.name).second)fail(parameter.where,"duplicate parameter: "+parameter.name);
      parameters.push_back(parameter.type);
     }
     auto type=Type::function(std::move(parameters),e.result);validate_type(type,e.where);
      e.captures.clear();auto previous_returns=returns;auto previous_loops=std::move(loop_values);loop_values.clear();auto saved_groups=taskgroups;taskgroups=&e==launch_body?1:0;
      lambdas.push_back({&e,scopes.size(),{}});push();returns=e.result;
      for(const auto& parameter:e.parameters){
       if(parameter.pattern)bind_pattern(*parameter.pattern,parameter.type);
       else define(parameter.name,parameter.type,false,parameter.where);
      }
      bool complete=check_block(e.body,false);pop();lambdas.pop_back();returns=previous_returns;loop_values=std::move(previous_loops);taskgroups=saved_groups;
     if(e.result!=Type::Void&&!complete)fail(e.where,"not all paths return in anonymous function");
     return e.type=std::move(type);
    }
     case Expr::Kind::Record:{
      std::vector<Type> fields;for(auto& c:e.children)fields.push_back(check_expr(*c));
      if(fields.size()!=e.labels.size())fail(e.where,"record field/label arity mismatch");
      std::set<std::string> names;for(const auto& label:e.labels)if(!names.insert(label).second)fail(e.where,"duplicate record field: "+label);
       auto t=Type::data(Type::Record,std::move(fields),e.labels);
       if(e.result!=Type::Void){
        catalog.verify(e.result,e.where);if(e.result.kind()!=Type::Record||e.result.nominal().empty())fail(e.where,"named record constructor requires a declared record");
        equal(t,Type::data(Type::Record,e.result.elements(),e.result.labels()),e.where);t=e.result;
       }
       validate_type(t,e.where);return e.type=t;
     }
     case Expr::Kind::Array:{
      if(e.type.kind()!=Type::Array||e.type.extent()!=e.children.size())fail(e.where,"array requires an inferred fixed element type and extent");
      for(auto& c:e.children)equal(check_expr(*c),e.type.elements()[0],c->where);
      validate_type(e.type,e.where);return e.type;
     }
     case Expr::Kind::Variant:{
      if(e.result.kind()!=Type::Sum||e.children.size()!=1)fail(e.where,"variant requires a closed sum and one payload");
      validate_type(e.result,e.where);auto pos=std::find(e.result.labels().begin(),e.result.labels().end(),e.text);
      if(pos==e.result.labels().end())fail(e.where,"unknown sum constructor: "+e.text);
      e.projection=static_cast<std::size_t>(pos-e.result.labels().begin());equal(child(0),e.result.elements()[e.projection],e.where);return e.type=e.result;
     }
     case Expr::Kind::Field:{
      auto t=child(0);if(t.kind()!=Type::Record)fail(e.where,"field projection requires a record");
      auto pos=std::find(t.labels().begin(),t.labels().end(),e.text);if(pos==t.labels().end())fail(e.where,"unknown record field: "+e.text);
      e.projection=static_cast<std::size_t>(pos-t.labels().begin());return e.type=t.elements()[e.projection];
     }
     case Expr::Kind::Index:case Expr::Kind::Slice:case Expr::Kind::Length:{
      auto t=child(0);if(t.kind()!=Type::Array&&t.kind()!=Type::Slice)fail(e.where,"index/slice/length requires an array or slice");
      for(std::size_t i=1;i<e.children.size();++i)equal(child(i),Type::I64,e.where);
      return e.type=e.kind==Expr::Kind::Length?Type::I64:e.kind==Expr::Kind::Slice?Type::data(Type::Slice,t.elements(),{},t.extent()):t.elements()[0];
     }
     case Expr::Kind::Match:{
      auto t=child(0);if(t.kind()!=Type::Sum)fail(e.where,"match requires a closed sum");
      if(e.children.size()!=e.labels.size()+1)fail(e.where,"match arm shape mismatch");
      std::set<std::string> seen;Type result=Type::Invalid;
      for(std::size_t i=0;i<e.labels.size();++i){
       auto pos=std::find(t.labels().begin(),t.labels().end(),e.labels[i]);
       if(pos==t.labels().end())fail(e.where,"unknown match constructor: "+e.labels[i]);
       if(!seen.insert(e.labels[i]).second)fail(e.where,"duplicate match constructor: "+e.labels[i]);
       auto& arm=*e.children[i+1];if(arm.kind!=Expr::Kind::Lambda||arm.parameters.size()!=1)fail(arm.where,"match arm requires one payload pattern");
       auto a=check_expr(arm);equal(a.parameters()[0],t.elements()[static_cast<std::size_t>(pos-t.labels().begin())],arm.where);
       if(result==Type::Invalid)result=a.result();else equal(a.result(),result,arm.where);
      }
      if(seen.size()!=t.labels().size())fail(e.where,"non-exhaustive match: every constructor needs an arm");
      (void)value_width(result,e.where);return e.type=result;
     }
    case Expr::Kind::Unary:{auto x=child(0);
     if(e.text=="!")equal(x,Type::Bool,e.where);
      else if(!x.is_float())equal(x,Type::I64,e.where);
     return e.type=x;}
   case Expr::Kind::Binary:{Type a=child(0),b=child(1);const auto&op=e.text;
    if(op=="+"||op=="-"||op=="*"||op=="/"||op=="%"){
       if(!a.is_numeric()||(op=="%"&&a.is_float()))equal(a,Type::I64,e.where);
      equal(b,a,e.where);return e.type=a;}
    if(op=="&&"||op=="||"){equal(a,Type::Bool,e.where);equal(b,Type::Bool,e.where);return e.type=Type::Bool;}
    if(op=="<"||op==">"||op=="<="||op==">="){
       if(!a.is_numeric())equal(a,Type::I64,e.where);
      equal(b,a,e.where);return e.type=Type::Bool;}
    if(op=="=="||op=="!="){
       if(a!=b||(!a.is_numeric()&&a!=Type::Bool&&a!=Type::String))fail(e.where,"incompatible or non-scalar equality operands");
     return e.type=Type::Bool;}
    fail(e.where,"unknown operator: "+op);}
    case Expr::Kind::Call:{
     auto& callee=*e.callee;
     Type callable=callee.kind==Expr::Kind::Variable?resolve_name(callee,true):check_expr(callee);
     bool builtin=callee.reference==Expr::Reference::Intrinsic;
     if(!builtin&&!callable.is_function())fail(callee.where,"cannot call value of type "+type_name(callable));
     std::vector<Type> arg;for(auto& c:e.children)arg.push_back(check_expr(*c));
     if(builtin)return check_intrinsic(e,arg);
     const auto& n=callee.text;
     if(callable.parameters().size()!=arg.size())fail(e.where,"wrong number of arguments to "+(n.empty()?"function value":n));
     for(size_t i=0;i<arg.size();++i){equal(arg[i],callable.parameters()[i],e.children[i]->where);
      if(resource(arg[i]))fail(e.where,"resource arguments need a reviewed FFI adapter");}
     return e.type=callable.result();
   }
  }return Type::Invalid;
 }
 bool check_block(std::vector<Stmt>& body,bool owns_scope=true){if(owns_scope)push();bool guarantees=false;
  for(auto& s:body){if(guarantees)fail(s.where,"unreachable statement after return or loop exit");
     switch(s.kind){
     case Stmt::Kind::TaskGroup:case Stmt::Kind::CspScope:++taskgroups;guarantees=check_block(s.body);--taskgroups;break;
     case Stmt::Kind::PushDefer:equal(check_expr(*s.value),Type::function({},Type::Void),s.where);break;
     case Stmt::Kind::Defer:{
      if(!s.value||s.value->kind!=Expr::Kind::Call)fail(s.where,"defer requires a call expression");
       auto t=check_expr(*s.value);if(resource(t))fail(s.where,"defer cannot discard a JSON owner/view");
       if(csp::contains(t))fail(s.where,"defer cannot retain concurrency handles");
       for(const auto& a:s.value->children)if(resource(a->type))fail(a->where,"defer cannot retain JSON owners/views");
       for(const auto& a:s.value->children)if(csp::contains(a->type))fail(a->where,"defer cannot retain concurrency handles");
      break;
     }
    case Stmt::Kind::Template:{
     check_block(s.body,false);
     auto type=check_expr(*s.value);validate_type(type,s.where);
     if(s.declared!=Type::Invalid)equal(type,s.declared,s.where);
     if(s.pattern)bind_pattern(*s.pattern,type);else define(s.name,type,false,s.where);
     break;
    }
     case Stmt::Kind::Let:{auto t=check_expr(*s.value);
      validate_type(s.declared,s.where);
     if(t==Type::Void)fail(s.where,"cannot bind a void expression");
     if(s.declared!=Type::Invalid)equal(t,s.declared,s.where);
     // Every JSON owner is unique. A name cannot be copied into another owner.
     if(resource(t)&&s.value->kind==Expr::Kind::Variable)fail(s.where,"resource aliases require an explicit borrowed API");
     if(t==Type::Json&&s.value->kind!=Expr::Kind::Call)fail(s.where,"json owners must originate from a constructor");
     if(t==Type::JsonView&&s.value->kind!=Expr::Kind::Call)fail(s.where,"json views may only come from borrowed operations");
     if(t==Type::Json||t==Type::JsonView){if(s.mutable_binding)fail(s.where,"mutable resources not yet supported");}
     if(s.pattern){
      if(s.mutable_binding)fail(s.where,"mutable destructuring is unsupported; use let");
      bind_pattern(*s.pattern,t);
     }else define(s.name,t,s.mutable_binding,s.where);
     break;
    }
    case Stmt::Kind::Assign:{const auto& binding=find(s.name,s.where);if(!binding.is_mutable)fail(s.where,"assignment to immutable binding: "+s.name);
     if(resource(binding.type))fail(s.where,"resource reassignment not supported");
     equal(check_expr(*s.value),binding.type,s.where);break;}
    case Stmt::Kind::Return:{Type t=s.value?check_expr(*s.value):Type::Void;equal(t,returns,s.where);
     if(resource(t))fail(s.where,"returning resources requires ownership/lifetime lowering");
     guarantees=true;break;}
    case Stmt::Kind::If:{equal(check_expr(*s.value),Type::Bool,s.where);bool yes=check_block(s.body),no=check_block(s.otherwise);guarantees=yes&&!s.otherwise.empty()&&no;break;}
     case Stmt::Kind::While:{equal(check_expr(*s.value),Type::Bool,s.where);(void)check_block(s.body);(void)check_block(s.otherwise);break;}
      case Stmt::Kind::Break:{
       auto target=std::find_if(loop_values.rbegin(),loop_values.rend(),[&](const auto& l){return l.target==s.control_target;});
       if(target==loop_values.rend()){if(s.value)fail(s.where,"value-carrying exit requires an expression loop target");}
       else{
        auto index=static_cast<std::size_t>(target.base()-loop_values.begin()-1);
        if(s.value){auto type=check_expr(*s.value);if(resource(type))fail(s.where,"loop exits cannot transfer or discard unique JSON resources/views");if(!s.value->noreturn){if(loop_values[index].type==Type::Invalid)loop_values[index].type=type;else equal(type,loop_values[index].type,s.where);}}
        else loop_values[index].bare=true;
       }
       guarantees=true;break;
      }
      case Stmt::Kind::Continue:guarantees=true;break;
     case Stmt::Kind::Range:fail(s.where,"range was not canonicalized");
     case Stmt::Kind::Storage:validate_type(s.declared,s.where);if(s.declared==Type::Void||resource(s.declared))fail(s.where,"invalid canonical storage type");define(s.name,s.declared,false,s.where);break;
     case Stmt::Kind::Initialize:equal(check_expr(*s.value),find(s.name,s.where).type,s.where);break;
     case Stmt::Kind::Region:{regions.push_back({s.control_target,false});bool exits=check_block(s.body);bool joins=regions.back().second;regions.pop_back();guarantees=exits&&!joins;break;}
     case Stmt::Kind::Leave:{auto target=std::find_if(regions.rbegin(),regions.rend(),[&](const auto& r){return r.first==s.control_target;});if(target==regions.rend())fail(s.where,"missing canonical region target");target->second=true;guarantees=true;break;}
     case Stmt::Kind::Trap:guarantees=true;break;
    case Stmt::Kind::Block:{guarantees=check_block(s.body);break;}
    case Stmt::Kind::Expr:{Type t=check_expr(*s.value);
     if(resource(t))fail(s.where,"discarding an owned or borrowed resource is not allowed");
     if(s.value->noreturn)guarantees=true;
     break;}
   }
  }
  if(owns_scope)pop();
  return guarantees;
 }
 public:
 explicit Checker(Unit& u):unit(u),catalog(u){}
  void run(){
   if(unit.functions.empty())fail({"<unit>"},"missing main function");
    for(auto& f:unit.functions){
     if(f.result.has_generics()&&!f.prototype)fail(f.where,"quantified types require a checked prototype");
     for(const auto& parameter:f.params)if(parameter.type.has_generics()&&!f.prototype)
      fail(parameter.where,"quantified types are only valid in local let prototypes");
    if(is_intrinsic_name(f.name))fail(f.where,"intrinsic name cannot be redefined: "+f.name);
     if(f.external) {
     if(f.result.is_data())fail(f.where,"data-valued extern ABI requires reviewed aggregate adapters");
    static const std::set<std::string> c_keywords={"auto","break","case","char","const","continue","default","do","double","else","enum","extern","float","for","goto","if","inline","int","long","register","restrict","return","short","signed","sizeof","static","struct","switch","typedef","union","unsigned","void","volatile","while","_Alignas","_Alignof","_Atomic","_Bool","_Complex","_Generic","_Imaginary","_Noreturn","_Static_assert","_Thread_local"};
     if(c_keywords.contains(f.name))fail(f.where,"C keyword cannot be a raw extern name: "+f.name);
      if(f.result.is_function()||f.result.has_foralls())fail(f.where,"function-valued extern ABI is unsupported; callbacks require reviewed contracts");
       if(f.result.is_tuple())fail(f.where,"tuple-valued extern ABI is unsupported; aggregates require reviewed adapters");
       if(f.result.is_code()||f.result.is_reference())fail(f.where,"runtime-managed extern ABI requires reviewed adapters");
   }
   if(!funcs.emplace(f.name,&f).second)fail(f.where,"duplicate function: "+f.name);
     if(f.result==Type::Json||f.result==Type::JsonView)fail(f.where,"resource returns not supported in the initial ABI");
     if(csp::contains(f.result))fail(f.where,"concurrency handles cannot escape through function results");
    validate_type(f.result,f.where);
    std::vector<Type> parameters;
      std::set<std::string> names;for(auto& p:f.params){if(!names.insert(p.name).second)fail(p.where,"duplicate parameter: "+p.name);
      if(f.external&&p.type.is_data())fail(p.where,"data-valued extern ABI requires reviewed aggregate adapters");
      if(p.type==Type::Void||p.type==Type::Json||p.type==Type::JsonView)fail(p.where,"unsupported parameter type in this milestone");
      if(csp::contains(p.type))fail(p.where,"concurrency handles cannot cross ordinary function signatures");
      if(f.external&&p.type.is_function())fail(p.where,"function-valued extern ABI is unsupported; callbacks require reviewed contracts");
      if(f.external&&(p.type.is_code()||p.type.is_reference()))fail(p.where,"runtime-managed extern ABI requires reviewed adapters");
     if(f.external&&p.type.is_tuple()&&!p.pattern)fail(p.where,"tuple-valued extern ABI requires an explicit flattening pattern");
     validate_type(p.type,p.where);parameters.push_back(p.type);
    }
     signatures.emplace(f.name,Type::function(std::move(parameters),f.result));
     if(f.external)(void)extern_parameter_types(f);
  }
  if(!funcs.contains("main"))fail(unit.functions.front().where,"missing main function");
  auto main=funcs.at("main");if(main->external||!main->params.empty()||(main->result!=Type::I64&&main->result!=Type::Void))fail(main->where,"main must be fn main() -> i64 or void");
  for(auto& f:unit.functions){if(f.external)continue;returns=f.result;push();
    for(auto& p:f.params){if(p.pattern)bind_pattern(*p.pattern,p.type);else define(p.name,p.type,false,p.where);}
   bool complete=check_block(f.body,false);pop();
   if(f.result!=Type::Void&&!complete)fail(f.where,"not all paths return in function: "+f.name);
  }
 }
};
}
void check(Unit&u){expand_macros(u);meta::require_core(u);infer_types(u);canonical::lower_control(u);Checker(u).run();if(canonical::needs_values(u)){canonical::lower_values(u);Checker(u).run();}csp::lower(u);canonical::verify_control(u);canonical::verify_values(u);csp::verify(u,true);}
}
