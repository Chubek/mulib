#include <on1x/meta.hpp>
#include <on1x/numeric.hpp>
#include <on1x/literals.hpp>
#include <on1x/patterns.hpp>
#include <on1x/operator_scan.h>
#include <on1x/operator_scope.hpp>
#include "ppx_internal.hpp"
#include <charconv>
#include <set>
#include <algorithm>
namespace on1x::meta {
namespace {
using K=Expr::Kind;
SExpr form(const std::string& tag,std::vector<SExpr> fields={}){fields.insert(fields.begin(),symbol(tag));return list(std::move(fields));}
SExpr loc(const Location& at){return form("Loc",{string(at.file),symbol(std::to_string(at.offset)),symbol(std::to_string(at.line)),symbol(std::to_string(at.column))});}
SExpr type(const Type& t){
 if(t.kind()==Type::Named){std::vector<SExpr> args;for(const auto& a:t.arguments())args.push_back(type(a));return form("NamedType",{string(t.nominal()),list(std::move(args))});}
 if(!t.nominal().empty())throw std::runtime_error("PPX cannot transport checked nominal layouts");
 if(t.is_data()){
  std::vector<SExpr> labels,fields;for(const auto& label:t.labels())labels.push_back(string(label));for(const auto& field:t.elements())fields.push_back(type(field));
  auto kind=t.kind()==Type::Record?"record":t.kind()==Type::Sum?"sum":t.kind()==Type::Array?"array":"slice";
  return form("ValueDataType",{string(kind),list(std::move(labels)),list(std::move(fields)),symbol(std::to_string(t.extent()))});
 }
   switch(t.kind()){
    case Type::Fiber:
     return form("CspType",{string("ifiber"),form("I64Type")});
    case Type::Channel:case Type::Sender:case Type::Receiver:
     return form("CspType",{
      string(t.kind()==Type::Channel?"channel":t.kind()==Type::Sender?"sender":"receiver"),
      type(t.csp_payload())});
   case Type::ExternalFiber:return form("CspType",{string("xfiber"),form("VoidType")});
 case Type::Invalid:return form("Inferred");case Type::Hole:return form("HoleType");
 case Type::I64:return form("I64Type");case Type::Bool:return form("BoolType");case Type::String:return form("StringType");case Type::Char:return form("CharType");
   case Type::F64:return form("F64Type");
   case Type::F32:return form("F32Type");
   case Type::U8:return form("U8Type");case Type::U16:return form("U16Type");
   case Type::U32:return form("U32Type");case Type::U64:return form("U64Type");
  case Type::Void:return form("VoidType");case Type::Json:return form("JsonType");case Type::JsonView:return form("JsonViewType");
   case Type::SExpr:return form("SExprType");case Type::Buffer:return form("BufferType");
   case Type::Own:throw Error({"<ppx-type>"},"ON1X-OWN-PPX: Unit-v1 PPX does not transport unique ownership","ON1X-OWN-PPX");
  case Type::Code:return form("CodeType",{type(t.code_result())});
 case Type::Bound:return form("BoundType",{symbol(std::to_string(t.generic_identity()))});
 case Type::Forall:return form("ForallType",{symbol(std::to_string(t.generic_identity())),type(t.body())});
 case Type::Tuple:{std::vector<SExpr> fields;for(const auto& e:t.elements())fields.push_back(type(e));return form("TupleType",{list(std::move(fields))});}
 case Type::Function:{std::vector<SExpr> fields;for(const auto& p:t.parameters())fields.push_back(type(p));return form("FunctionType",{list(std::move(fields)),type(t.result())});}
 default:throw std::runtime_error("PPX accepts source AST types, not inferred HIR identities");
 }
}
SExpr pattern(const Pattern& p){
 if(!p.is_tuple()&&p.kind!=Pattern::Kind::Name){
  static const std::vector<std::string> kinds={"name","tuple","wildcard","rest","literal","variant","array","record","or","as"};
  std::vector<SExpr> fields,parts;for(const auto& label:p.labels)fields.push_back(string(label));for(const auto& c:p.elements)parts.push_back(pattern(c));
  return form("StructuralPattern",{string(kinds.at(static_cast<std::size_t>(p.kind))),string(p.name),list(std::move(fields)),list(std::move(parts)),symbol(p.open?"true":"false"),loc(p.where)});
 }
 if(!p.is_tuple())return form("NamePattern",{string(p.name),loc(p.where)});
 std::vector<SExpr> fields;for(const auto& e:p.elements)fields.push_back(pattern(e));return form("TuplePattern",{list(std::move(fields)),loc(p.where)});
}
SExpr parameters(const std::vector<Param>& ps){
 for(const auto& p:ps)if(p.borrow!=BorrowMode::Value)
  throw Error(p.where,"borrowed parameter modes are not part of PPX Unit v1",
              "ON1X-REF");
 std::vector<SExpr> out;for(const auto& p:ps)out.push_back(p.pattern?form("PatternParameter",{pattern(*p.pattern),type(p.type),loc(p.where)}):form("Parameter",{string(p.name),type(p.type),loc(p.where)}));return list(std::move(out));
}
SExpr expression(const Expr& e);
SExpr block(const std::vector<Stmt>& body,const Location& at);
const char* position_name(OperatorPosition position){
 return position==OperatorPosition::Prefix?"prefix":position==OperatorPosition::Postfix?"postfix":"infix";
}
const char* arity_name(OperatorArity arity){
 return arity==OperatorArity::Unary?"unary":arity==OperatorArity::Ternary?"ternary":"binary";
}
const char* binds_name(OperatorAssociativity binds){return binds==OperatorAssociativity::Right?"right":"left";}
SExpr operator_header(const OperatorHeader& h){
 return form("OperatorHeader",{string(h.lexeme),string(h.lexeme2),string(arity_name(h.arity)),
  string(position_name(h.position)),symbol(std::to_string(h.precedence)),
  symbol(std::to_string(h.tightness)),string(binds_name(h.binds)),loc(h.where)});
}
SExpr operator_generics(const Function& f){
 std::vector<SExpr> names;for(const auto& parameter:f.type_parameters)names.push_back(string(parameter.name));
 return list(std::move(names));
}
SExpr operator_directive(const FixityDirective& d){
 return form("FixityDirective",{string(d.lexeme),symbol(std::to_string(d.tightness)),
  string(binds_name(d.binds)),loc(d.where)});
}
SExpr extension(const Expr& e,bool attribute){
 std::vector<SExpr> args;for(std::size_t i=1;i<e.children.size();++i)args.push_back(expression(*e.children[i]));
 return form(attribute?"Attribute":"ExtensionExpr",{string(e.text),string(literal_value(e.children.at(0)->text)),list(std::move(args)),loc(e.where)});
}
SExpr expression(const Expr& e){
  if(e.kind==K::Borrow||e.borrow!=BorrowMode::Value)
   throw Error(e.where,"lexical borrows are not PPX Unit v1 wire data",
               "ON1X-REF");
  if(e.kind==K::UnsafeBlock||e.unsafe_region||
     e.reference==Expr::Reference::UnsafeForeign||e.reference==Expr::Reference::ScalarForeign)
   throw Error(e.where,"ON1X-KERNEL-PPX: foreign identities and unsafe scopes are not PPX wire data","ON1X-KERNEL-PPX");
 if(e.kind==K::SourceCsp){
  std::vector<SExpr> args;for(const auto& c:e.children)args.push_back(expression(*c));
  return form("SourceCsp",{string(e.text),type(e.result),list(std::move(args)),loc(e.where)});
 }
 if(e.kind==K::Propagate)return form("Propagate",{expression(*e.children.at(0)),loc(e.where)});
  auto at=loc(e.where);
  if(e.kind==K::PatternMatch){
   std::vector<SExpr> arms;for(std::size_t i=1;i<e.children.size();++i){const auto& arm=*e.children[i];arms.push_back(form("MatchArm",{pattern(arm.patterns.at(0)),expression(*arm.children.at(0)),block(arm.body,arm.where),expression(*arm.children.at(1)),loc(arm.where)}));}
   auto subject=expression(*e.children.at(0));
   if(e.sum_match)return form("SourceSumMatch",{std::move(subject),list(std::move(arms)),std::move(at)});
   return form("SourceMatch",{string(e.text),std::move(subject),list(std::move(arms)),symbol(e.statement_match?"true":"false"),std::move(at)});
  }
  if(e.kind==K::Unit)return form("UnitValue",{std::move(at)});
  if(e.kind==K::Loop)return form("LoopValue",{string(e.text),symbol(std::to_string(e.context)),block(e.body,e.where),std::move(at)});
 if(e.kind>=K::Record&&e.kind<=K::Length){
  static const char* names[]={"record","variant","match","array","field","index","slice","length"};
  auto index=static_cast<std::size_t>(e.kind)-static_cast<std::size_t>(K::Record);
  if(index>=std::size(names))throw Error(e.where,"unknown data expression");
  std::vector<SExpr> labels,children;for(const auto& label:e.labels)labels.push_back(string(label));for(const auto& c:e.children)children.push_back(expression(*c));
  return form("ValueData",{string(names[index]),string(e.text),list(std::move(labels)),type(e.result),list(std::move(children)),std::move(at)});
 }
 switch(e.kind){
   case K::Integer:return form(numeric_literal_type(e.text).is_unsigned()?"UnsignedLiteral":e.text.starts_with("0x")||e.text.starts_with("0X")?"HexLiteral":"DecimalLiteral",{string(e.text),std::move(at)});
  case K::Float:return form("FloatLiteral",{string(e.text),std::move(at)});
 case K::String:return form("StringLiteral",{string(literal_value(e.text)),std::move(at)});
 case K::Character:return form("CharacterLiteral",{string(e.text),std::move(at)});
 case K::ByteBuffer:return form("ByteBufferLiteral",{string(e.text),std::move(at)});
 case K::Interpolated:{
  std::vector<SExpr> parts;for(const auto& c:e.children)parts.push_back(expression(*c));
  return form("InterpolatedString",{list(std::move(parts)),std::move(at)});
 }
 case K::Boolean:return form("BooleanLiteral",{symbol(e.text),std::move(at)});
  case K::Variable:return form("Identifier",{string(e.text),std::move(at)});
  case K::Unary:case K::Binary:
   if(e.children.size()!=(e.kind==K::Unary?1u:2u))
    throw Error(e.where,"native operator syntax is not a first-class reference","ON1X-OPERATOR-ARITY");
   if(e.kind==K::Unary)return form("Unary",{symbol(e.text),expression(*e.children[0]),std::move(at)});
   return form("Binary",{symbol(e.text),expression(*e.children[0]),expression(*e.children[1]),std::move(at)});
  case K::OperatorReference:return form("OperatorReference",{string(e.text),std::move(at)});
  case K::OperatorUnary:return form("OperatorUnary",{string(e.text),string(position_name(e.operator_position)),expression(*e.children.at(0)),std::move(at)});
  case K::OperatorBinary:return form("OperatorBinary",{string(e.text),expression(*e.children.at(0)),expression(*e.children.at(1)),std::move(at)});
  case K::OperatorTernary:return form("OperatorTernary",{string(e.text),string(e.operator_lexeme2),expression(*e.children.at(0)),expression(*e.children.at(1)),expression(*e.children.at(2)),std::move(at)});
 case K::Tuple:case K::Call:{
  std::vector<SExpr> args;for(const auto& c:e.children)args.push_back(expression(*c));
  if(e.kind==K::Call)return form("Call",{expression(*e.callee),list(std::move(args)),std::move(at)});
  return form("Tuple",{list(std::move(args)),std::move(at)});
 }
  case K::Lambda:return form(e.procedure?"ProcedureLambda":"Lambda",{parameters(e.parameters),type(e.result),block(e.body,e.where),std::move(at)});
  case K::Quote:case K::Splice:case K::Lift:case K::Run:
   return form(e.kind==K::Quote?"Quote":e.kind==K::Splice?"Splice":e.kind==K::Lift?"Lift":"Run",{expression(*e.children.at(0)),std::move(at)});
 case K::SyntaxBlock:return block(e.body,e.where);
 case K::Extension:return extension(e,false);
 default:throw Error(e.where,"PPX serialization requires macro-expanded, untyped source AST");
 }
}
 SExpr statement(const Stmt& s){
   if(s.ref_store)
    throw Error(s.where,"borrowed stores are not part of PPX Unit v1",
                "ON1X-REF");
  if(s.unsafe_block)throw Error(s.where,"ON1X-KERNEL-PPX: unsafe scopes are not PPX wire data","ON1X-KERNEL-PPX");
 auto at=loc(s.where);
  switch(s.kind){
  case Stmt::Kind::TaskGroup:
   if(s.value)return form("TaskGroupWithin",{expression(*s.value),block(s.body,s.where),at});
   return form("TaskGroup",{block(s.body,s.where),at});
 case Stmt::Kind::Defer:return form("Defer",{expression(*s.value),at});
 case Stmt::Kind::Let:
  return s.pattern?form("LetPattern",{pattern(*s.pattern),type(s.declared),expression(*s.value),std::move(at)}):form(s.mutable_binding?"Var":"Let",{string(s.name),type(s.declared),expression(*s.value),std::move(at)});
 case Stmt::Kind::Assign:return form("Assign",{string(s.name),expression(*s.value),std::move(at)});
 case Stmt::Kind::Return:return s.value?form("Return",{expression(*s.value),std::move(at)}):form("ReturnVoid",{std::move(at)});
 case Stmt::Kind::If:return form("If",{expression(*s.value),block(s.body,s.where),block(s.otherwise,s.where),std::move(at)});
  case Stmt::Kind::While:
   if(!s.label.empty()||s.label_context||!s.otherwise.empty())return form("ControlWhile",{string(s.label),symbol(std::to_string(s.label_context)),expression(*s.value),block(s.body,s.where),block(s.otherwise,s.where),std::move(at)});
   return form("While",{expression(*s.value),block(s.body,s.where),std::move(at)});
  case Stmt::Kind::Range:return form("RangeLoop",{string(s.name),expression(*s.value),symbol(s.inclusive?"true":"false"),string(s.label),symbol(std::to_string(s.label_context)),block(s.body,s.where),std::move(at)});
   case Stmt::Kind::Break:case Stmt::Kind::Continue:
    if(s.value){if(s.kind!=Stmt::Kind::Break)throw Error(s.where,"continue cannot carry a value");return form("ValueBreak",{string(s.label),symbol(std::to_string(s.label_context)),expression(*s.value),std::move(at)});}
    return form("LoopExit",{string(s.kind==Stmt::Kind::Break?"break":"continue"),string(s.label),symbol(std::to_string(s.label_context)),std::move(at)});
 case Stmt::Kind::Block:return form("Scope",{block(s.body,s.where),std::move(at)});
 case Stmt::Kind::Expr:return form("Do",{expression(*s.value),std::move(at)});
 default:throw Error(s.where,"inferred templates cannot be sent to PPX");
 }
}
SExpr block(const std::vector<Stmt>& body,const Location& at){std::vector<SExpr> out;for(const auto& s:body)out.push_back(statement(s));return form("Block",{list(std::move(out)),loc(at)});}
const std::string& tag(const SExpr& n){return n.items.at(0).text;}
const SExpr& field(const SExpr& n,std::size_t i){return n.items.at(i+1);}
std::size_t number(const SExpr& n){std::size_t v=0;std::from_chars(n.text.data(),n.text.data()+n.text.size(),v);return v;}
Location location(const SExpr& n){return {field(n,0).text,number(field(n,1)),number(field(n,2)),number(field(n,3))};}
[[noreturn]] void invalid(const SExpr& n,const std::string& message){throw Error(n.where,"PPX AST reconstruction: "+message);}
Type read_type(const SExpr& n,bool inferred=false,std::size_t bound=0){
   const auto& k=tag(n);
  if(k=="CspType"){
   auto name=field(n,0).text;auto payload=read_type(field(n,1),false,bound);
   if(name=="xfiber"){if(payload!=Type::Void)invalid(n,"xfiber has no generic payload");return Type::ExternalFiber;}
   if(name=="ifiber"){
    if(payload!=Type::I64)invalid(n,"internal fibers require i64 results");
    return Type::Fiber;
   }
   if(name=="channel")return Type::channel(std::move(payload));
   if(name=="sender")return Type::sender(std::move(payload));
   if(name=="receiver")return Type::receiver(std::move(payload));
  invalid(n,"unknown concurrency type");
 }
 if(k=="NamedType"){
  std::vector<Type> args;for(const auto& a:field(n,1).items)args.push_back(read_type(a,false,bound));
  if(args.size()>32)invalid(n,"nominal type argument limit exceeded");
  return Type::named(field(n,0).text,std::move(args));
 }
 if(k=="ValueDataType"){
  auto name=field(n,0).text;auto kind=name=="record"?Type::Record:name=="sum"?Type::Sum:name=="array"?Type::Array:name=="slice"?Type::Slice:Type::Invalid;
  std::vector<std::string> labels;std::set<std::string> unique;std::vector<Type> fields;
  for(const auto& label:field(n,1).items){if(!unique.insert(label.text).second)invalid(n,"duplicate data label");labels.push_back(label.text);}
  for(const auto& child:field(n,2).items)fields.push_back(read_type(child,false,bound));
  auto extent=number(field(n,3));
  if(kind==Type::Invalid||fields.size()>256||extent>256)invalid(n,"invalid data kind/extent/field count");
  if(kind==Type::Record||kind==Type::Sum){if(labels.size()!=fields.size()||extent||(kind==Type::Sum&&fields.empty()))invalid(n,"invalid labeled data shape");}
  else if(fields.size()!=1||!labels.empty())invalid(n,"invalid array/slice shape");
  return Type::data(kind,std::move(fields),std::move(labels),extent);
 }
 if(k=="Inferred"){if(!inferred)invalid(n,"Inferred is only valid for a local annotation");return Type::Invalid;}
 if(k=="HoleType")return Type::Hole;
    for(auto kind:{Type::I64,Type::F64,Type::F32,Type::U8,Type::U16,Type::U32,Type::U64,Type::Bool,Type::String,Type::Char,Type::Void,Type::Json,Type::JsonView,Type::SExpr,Type::Buffer})if(tag(type(Type(kind)))==k)return kind;
  if(k=="CodeType")return Type::code(read_type(field(n,0),false,bound));
 if(k=="BoundType"){auto index=number(field(n,0));if(index>=bound)invalid(n,"unbound quantified type variable");return Type::bound(index);}
 if(k=="ForallType"){
  if(bound)invalid(n,"nested forall binders are unsupported");
  if(number(field(n,0))>1024)invalid(n,"forall binder count limit exceeded");
  auto body=read_type(field(n,1),false,number(field(n,0)));
  if(!body.is_function()||body.has_holes()||body.has_foralls())invalid(n,"forall requires a closed function type without holes/nested binders");
  return Type::for_all(number(field(n,0)),std::move(body));
 }
 if(k=="TupleType"||k=="FunctionType"){
  std::vector<Type> fields;for(const auto& c:field(n,0).items)fields.push_back(read_type(c,false,bound));
  if(k=="TupleType"){if(fields.size()<2)invalid(n,"tuple type requires at least two elements");return Type::tuple(std::move(fields));}
  return Type::function(std::move(fields),read_type(field(n,1),false,bound));
 }
 invalid(n,"unhandled type constructor "+k);
}
Pattern read_pattern(const SExpr& n){
 Pattern p;p.where=location(n.items.back());
 if(tag(n)=="NamePattern")p.name=field(n,0).text;
 else if(tag(n)=="TuplePattern"){p.kind=Pattern::Kind::Tuple;
  for(const auto& c:field(n,0).items)p.elements.push_back(read_pattern(c));
  if(p.elements.size()<2)invalid(n,"tuple pattern requires at least two elements");
 }else if(tag(n)=="StructuralPattern"){
  static const std::vector<std::string> kinds={"name","tuple","wildcard","rest","literal","variant","array","record","or","as"};
  auto found=std::find(kinds.begin()+2,kinds.end(),field(n,0).text);if(found==kinds.end())invalid(n,"unknown structural pattern kind");p.kind=static_cast<Pattern::Kind>(found-kinds.begin());p.name=field(n,1).text;
  for(const auto& label:field(n,2).items)p.labels.push_back(label.text);
  for(const auto& c:field(n,3).items)p.elements.push_back(read_pattern(c));
  p.open=field(n,4).text=="true";
  if(p.elements.size()>256||p.labels.size()>256)invalid(n,"pattern width limit exceeded");
  if(p.binds()&&!p.name.empty())base_schema().validate(string(p.name),"Name","$.pattern_binder");
 }else invalid(n,"unhandled pattern constructor "+tag(n));return p;
}
 std::vector<Param> read_parameters(const SExpr& n,std::size_t bound=0){
  std::vector<Param> ps;for(const auto& c:n.items){Param p;p.where=location(c.items.back());p.type=read_type(field(c,1),false,bound);
  if(tag(c)=="Parameter")p.name=field(c,0).text;
  else if(tag(c)=="PatternParameter"){p.pattern=read_pattern(field(c,0));patterns::require_binding(*p.pattern);p.name="@argument_"+std::to_string(ps.size());}
  else invalid(c,"unhandled parameter constructor "+tag(c));
  ps.push_back(std::move(p));
 }return ps;
}
 struct BodyReader {
  std::size_t bound=0;
  ExprPtr read_expression(const SExpr& n);
  std::vector<Stmt> read_block(const SExpr& n);
 };
 ExprPtr BodyReader::read_expression(const SExpr& n){
   const auto& k=tag(n);auto e=std::make_unique<Expr>();e->where=location(n.items.back());
      if(k=="SourceCsp"){
        e->kind=K::SourceCsp;e->text=field(n,0).text;e->result=read_type(field(n,1),false,bound);
         static const std::set<std::string> ops={"launch","launch_async","await_timer","launch_exec","launch_capture","channel","split","join","cancel","send","recv","try_send","try_recv","close","poll","offer_recv","offer_send","offer_after"};
       if(!ops.contains(e->text))invalid(n,"unknown concurrency operation");
       for(const auto& arg:field(n,2).items)e->children.push_back(read_expression(arg));
         auto count=e->text=="channel"?3:e->text=="send"||e->text=="try_send"||
                    e->text=="offer_send"?2:1;
        if(e->text=="poll")count=static_cast<int>(e->children.size());
        if(e->text=="launch_exec"||e->text=="launch_capture")count=static_cast<int>(e->children.size());
         if(e->children.size()!=static_cast<std::size_t>(count)||
            (e->text=="channel"?e->result==Type::Invalid:e->result!=Type::Void))
          invalid(n,"concurrency operand/payload shape mismatch");
        if(e->text=="poll"&&(e->children.empty()||e->children.size()>18))invalid(n,"poll offer limit exceeded");
        if((e->text=="launch_exec"||e->text=="launch_capture")&&(e->children.empty()||e->children.size()>9))invalid(n,"external argv limit exceeded");
       if((e->text=="launch"||e->text=="launch_async")&&(e->children[0]->kind!=K::Lambda||!e->children[0]->parameters.empty()||e->children[0]->result!=Type::I64))invalid(n,"launch requires an i64 source block");
      }
      else if(k=="UnitValue")e->kind=K::Unit;
     else if(k=="Propagate"){e->kind=K::Propagate;e->children.push_back(read_expression(field(n,0)));}
    else if(k=="LoopValue"){e->kind=K::Loop;e->text=field(n,0).text;e->context=number(field(n,1));e->body=read_block(field(n,2));if(!e->text.empty())base_schema().validate(string(e->text),"Name","$.loop_label");}
    else if(k=="SourceMatch"||k=="SourceSumMatch"){
     e->kind=K::PatternMatch;e->sum_match=k=="SourceSumMatch";
     if(!e->sum_match){
      e->text=field(n,0).text;e->statement_match=field(n,3).text=="true";
      if(!e->text.empty())base_schema().validate(string(e->text),"Name","$.match_binding");
     }
     e->children.push_back(read_expression(field(n,e->sum_match?0:1)));
     const auto& arms=field(n,e->sum_match?1:2).items;
     if(arms.size()>256)invalid(n,"match arm limit exceeded");
     for(const auto& arm:arms){auto a=std::make_unique<Expr>();a->kind=K::Case;a->where=location(arm.items.back());a->patterns.push_back(read_pattern(field(arm,0)));a->children.push_back(read_expression(field(arm,1)));a->body=read_block(field(arm,2));a->children.push_back(read_expression(field(arm,3)));e->children.push_back(std::move(a));}
   }
   else if(k=="ValueData"){
    static const std::vector<std::string> names={"record","variant","match","array","field","index","slice","length"};
    auto found=std::find(names.begin(),names.end(),field(n,0).text);if(found==names.end())invalid(n,"unknown data expression kind");
    e->kind=static_cast<K>(static_cast<std::size_t>(K::Record)+static_cast<std::size_t>(found-names.begin()));e->text=field(n,1).text;
    for(const auto& label:field(n,2).items)e->labels.push_back(label.text);
      e->result=read_type(field(n,3),false,bound);for(const auto& c:field(n,4).items)e->children.push_back(read_expression(c));
     if(e->kind==K::Record&&e->result!=Type::Void&&e->result.kind()!=Type::Named)invalid(n,"named record annotation must be a named reference");
     if(e->kind!=K::Record&&e->kind!=K::Variant&&e->result!=Type::Void)invalid(n,"unexpected data expression annotation");
    std::size_t arity=e->children.size();
    if(e->labels.size()>256||arity>257)invalid(n,"data expression width limit exceeded");
    if(e->kind==K::Record&&arity!=e->labels.size())invalid(n,"record field/label arity mismatch");
    if(e->kind==K::Match&&arity!=e->labels.size()+1)invalid(n,"match arm arity mismatch");
    if((e->kind==K::Variant||e->kind==K::Field||e->kind==K::Length)&&arity!=1)invalid(n,"data projection/constructor arity mismatch");
    if(e->kind==K::Index&&arity!=2)invalid(n,"index arity mismatch");
    if(e->kind==K::Slice&&arity!=3)invalid(n,"slice arity mismatch");
    if(e->kind==K::Array&&arity>256)invalid(n,"array extent limit exceeded");
    if(e->kind!=K::Record&&e->kind!=K::Match&&!e->labels.empty())invalid(n,"unexpected data labels");
   }
    else if(k=="DecimalLiteral"||k=="HexLiteral"||k=="UnsignedLiteral"){e->kind=K::Integer;e->text=field(n,0).text;}
  else if(k=="FloatLiteral"){e->kind=K::Float;e->text=field(n,0).text;}
 else if(k=="StringLiteral"){e->kind=K::String;e->text=source_literal(field(n,0).text);}
 else if(k=="CharacterLiteral"){
  e->kind=K::Character;e->text=field(n,0).text;(void)literals::character(e->text,e->where);
 }
 else if(k=="ByteBufferLiteral"){
  e->kind=K::ByteBuffer;e->text=field(n,0).text;
  if(!literals::byte_payload(e->text))invalid(n,"invalid byte buffer payload");
 }
 else if(k=="InterpolatedString"){
  e->kind=K::Interpolated;
  const auto& parts=field(n,0).items;
  if(parts.empty()||parts.size()>literals::max_format_parts)invalid(n,"invalid interpolation part count");
  for(const auto& part:parts)e->children.push_back(read_expression(part));
 }
 else if(k=="BooleanLiteral"){e->kind=K::Boolean;e->text=field(n,0).text;}
 else if(k=="Identifier"){e->kind=K::Variable;e->text=field(n,0).text;}
  else if(k=="OperatorReference"||k=="OperatorUnary"||k=="OperatorBinary"||k=="OperatorTernary"){
   e->text=field(n,0).text;
   if(!on1x_operator_spelling(e->text.data(),e->text.size()))
    invalid(n,"invalid custom operator spelling");
   if(k=="OperatorReference")e->kind=K::OperatorReference;
   else if(k=="OperatorUnary"){
    e->kind=K::OperatorUnary;
    const auto& position=field(n,1).text;
    if(position!="prefix"&&position!="postfix")invalid(n,"unary operator position must be prefix or postfix");
    e->operator_position=position=="prefix"?OperatorPosition::Prefix:OperatorPosition::Postfix;
    e->children.push_back(read_expression(field(n,2)));
   }else if(k=="OperatorBinary"){
    e->kind=K::OperatorBinary;
    for(std::size_t i=1;i<=2;++i)e->children.push_back(read_expression(field(n,i)));
   }else{
    e->kind=K::OperatorTernary;e->operator_lexeme2=field(n,1).text;
    if(!on1x_operator_spelling(e->operator_lexeme2.data(),e->operator_lexeme2.size()))
     invalid(n,"invalid second ternary operator spelling");
    for(std::size_t i=2;i<=4;++i)e->children.push_back(read_expression(field(n,i)));
   }
  }else if(k=="Unary"||k=="Binary"){
  e->kind=k=="Unary"?K::Unary:K::Binary;e->text=field(n,0).text;e->children.push_back(read_expression(field(n,1)));
  if(k=="Binary")e->children.push_back(read_expression(field(n,2)));
 }else if(k=="Tuple"||k=="Call"){
  e->kind=k=="Tuple"?K::Tuple:K::Call;auto index=k=="Tuple"?0:1;
  if(k=="Call")e->callee=read_expression(field(n,0));
  for(const auto& c:field(n,index).items)e->children.push_back(read_expression(c));
  if(k=="Tuple"&&e->children.size()<2)invalid(n,"tuple expression requires at least two elements");
  }else if(k=="Quote"||k=="Splice"||k=="Lift"||k=="Run"){
   e->kind=k=="Quote"?K::Quote:k=="Splice"?K::Splice:k=="Lift"?K::Lift:K::Run;e->children.push_back(read_expression(field(n,0)));
  }else if(k=="Lambda"||k=="ProcedureLambda"){
   e->kind=K::Lambda;e->procedure=k=="ProcedureLambda";
    e->parameters=read_parameters(field(n,0),bound);e->result=read_type(field(n,1),false,bound);e->body=read_block(field(n,2));
 }else if(k=="ExtensionExpr"||k=="Attribute")invalid(n,"unhandled extension @"+field(n,0).text+"("+field(n,1).text+")");
 else invalid(n,"unhandled extension constructor "+k);
 return e;
}
 std::vector<Stmt> BodyReader::read_block(const SExpr& n){
 if(tag(n)!="Block")invalid(n,"unhandled block constructor "+tag(n));
 std::vector<Stmt> body;
 for(const auto& c:field(n,0).items){Stmt s;s.where=location(c.items.back());const auto& k=tag(c);
   if(k=="TaskGroup"||k=="TaskGroupWithin"){
    s.kind=Stmt::Kind::TaskGroup;
    if(k=="TaskGroupWithin")s.value=read_expression(field(c,0));
    s.body=read_block(field(c,k=="TaskGroupWithin"?1:0));
   }
   else if(k=="Let"||k=="Var"||k=="LetPattern"){
   s.kind=Stmt::Kind::Let;s.mutable_binding=k=="Var";if(k=="LetPattern"){s.pattern=read_pattern(field(c,0));patterns::require_binding(*s.pattern);}else s.name=field(c,0).text;
    s.declared=read_type(field(c,1),true,bound);s.value=read_expression(field(c,2));
   }else if(k=="Assign"){s.kind=Stmt::Kind::Assign;s.name=field(c,0).text;s.value=read_expression(field(c,1));}
   else if(k=="ControlWhile"){
    s.kind=Stmt::Kind::While;s.label=field(c,0).text;s.label_context=number(field(c,1));s.value=read_expression(field(c,2));s.body=read_block(field(c,3));s.otherwise=read_block(field(c,4));
   }else if(k=="RangeLoop"){
    s.kind=Stmt::Kind::Range;s.name=field(c,0).text;s.value=read_expression(field(c,1));s.inclusive=field(c,2).text=="true";s.label=field(c,3).text;s.label_context=number(field(c,4));s.body=read_block(field(c,5));
    if(s.value->kind!=K::Tuple||s.value->children.size()!=2)invalid(c,"range requires exactly two bounds");
    }else if(k=="LoopExit"){
    auto operation=field(c,0).text;if(operation!="break"&&operation!="continue")invalid(c,"unknown loop exit operation");
     s.kind=operation=="break"?Stmt::Kind::Break:Stmt::Kind::Continue;s.label=field(c,1).text;s.label_context=number(field(c,2));
    }else if(k=="ValueBreak"){
     s.kind=Stmt::Kind::Break;s.label=field(c,0).text;s.label_context=number(field(c,1));s.value=read_expression(field(c,2));
   }
  else if(k=="Return"||k=="ReturnVoid"){s.kind=Stmt::Kind::Return;if(k=="Return")s.value=read_expression(field(c,0));}
  else if(k=="If"||k=="While"){
   s.kind=k=="If"?Stmt::Kind::If:Stmt::Kind::While;s.value=read_expression(field(c,0));s.body=read_block(field(c,1));if(k=="If")s.otherwise=read_block(field(c,2));
  }else if(k=="Scope"){s.kind=Stmt::Kind::Block;s.body=read_block(field(c,0));}
   else if(k=="Do"||k=="Defer"){s.kind=k=="Do"?Stmt::Kind::Expr:Stmt::Kind::Defer;s.value=read_expression(field(c,0));}
  else invalid(c,"unhandled statement extension "+k);
   if(!s.label.empty())base_schema().validate(string(s.label),"Name","$.loop_label");
   body.push_back(std::move(s));
 }return body;
}
}
SExpr encode_type(const Type& value){return type(value);}
Type decode_type(const SExpr& tree){base_schema().validate(tree,"Type");return read_type(tree);}
Location decode_source_location(const SExpr& tree){base_schema().validate(tree,"Location");return location(tree);}
SExpr encode_source_module_metadata(const Unit& unit){
 auto components=[](const std::string& name){
  std::vector<SExpr> values;
  for(std::size_t begin=0;begin<name.size();){
   const auto end=name.find('.',begin);
   values.push_back(string(name.substr(begin,end==std::string::npos?end:end-begin)));
   if(end==std::string::npos)break;
   begin=end+1;
  }
  return list(std::move(values));
 };
 auto alias=[](const std::string& spelling){return spelling.empty()?form("SourceNoAlias"):form("SourceAlias",{string(spelling)});};
 auto header=unit.module
  ?form("SourceModuleHeader",{components(unit.module->name),loc(unit.module->where)})
  :form("SourceAnonymousModule");
 std::vector<SExpr> imports,declarations;
 for(const auto& request:unit.module_imports){
  const bool program=request.kind==Import::Kind::Program;
  auto target=request.quoted?form("SourceQuotedTarget",{string(request.module)}):
                             form("SourceNamedTarget",{components(request.module)});
  std::vector<SExpr> selectors;
  for(const auto& sel:request.names){
   if(sel.is_operator){
    if(program||!sel.alias.empty())throw Error(request.where,"unsupported source operator selector");
    selectors.push_back(form("SourceOperatorSelector",{string(sel.name)}));
   }else selectors.push_back(form("SourceValueSelector",{string(sel.name),alias(sel.alias)}));
  }
  if(program&&request.relative)throw Error(request.where,"a program import cannot be relative");
  auto fields=std::vector<SExpr>{std::move(target),symbol(request.selective?"true":"false"),alias(request.alias),
                                list(std::move(selectors)),loc(request.where)};
  if(!program)fields.insert(fields.begin()+1,symbol(std::to_string(request.relative)));
  imports.push_back(form(program?"SourceProgramImport":"SourceModuleImport",std::move(fields)));
 }
 auto rule=[&](const char* kind,const std::string& spelling,const VisibilityRule& visibility,const Location& where){
  SExpr spec;
  switch(visibility.kind){
  case DeclarationVisibility::Unspecified:return;
  case DeclarationVisibility::Public:spec=form("SourcePublic");break;
  case DeclarationVisibility::Shared:spec=form("SourceShared",{symbol(std::to_string(visibility.elevation))});break;
  case DeclarationVisibility::Private:spec=form("SourcePrivate");break;
  }
  auto fields=std::vector<SExpr>{string(spelling),std::move(spec),loc(where)};
  if(!kind)declarations.push_back(form("SourceOperatorVisibility",std::move(fields)));
  else{
   fields.insert(fields.begin(),form(kind));
   declarations.push_back(form("SourceDeclarationVisibility",std::move(fields)));
  }
 };
 for(const auto& f:unit.functions)rule("SourceFunctionKind",f.name,f.visibility,f.where);
 for(const auto& d:unit.nominals)rule("SourceNominalKind",d.name,d.visibility,d.where);
 for(const auto& d:unit.operators)rule(nullptr,d.header.lexeme,d.header.visibility,d.header.where);
 for(const auto& m:unit.macros)rule("SourceMacroKind",m.name,m.visibility,m.where);
 for(const auto& c:unit.cliches)rule("SourceClicheKind",c.name,c.visibility,c.where);
 auto tree=form("SourceModuleMetadata",{symbol("1"),std::move(header),list(std::move(imports)),list(std::move(declarations))});
 base_schema().validate(tree,"SourceModuleMetadata");
 return tree;
}
SExpr encode(const Unit& unit){
 if(!unit.opaque_nominal_owners.empty())
  throw Error(unit.module?unit.module->where:Location{"<ppx>"},
              "Unit-v1 AST cannot transport opaque module ABI authority","ON1X-PPX-SCOPE");
  require_no_kernel_imports(unit);
  auto reject=[&](const Type& type,const Location& at){
   if(type.contains_unique())
    throw Error(at,"ON1X-OWN-PPX: Unit-v1 PPX does not transport unique ownership","ON1X-OWN-PPX");
  };
  auto pattern=[&](auto&& self,const Pattern& p)->void{
   reject(p.type,p.where);for(const auto& child:p.elements)self(self,child);
  };
  auto expression=[&](auto&& self,const Expr& e)->void{
   reject(e.type,e.where);reject(e.result,e.where);
   for(const auto& p:e.parameters){reject(p.type,p.where);if(p.pattern)pattern(pattern,*p.pattern);}
   for(const auto& p:e.patterns)pattern(pattern,p);
   if(e.callee)self(self,*e.callee);
   for(const auto& child:e.children)if(child)self(self,*child);
    auto body=[&](auto&& visit,const std::vector<Stmt>& statements)->void{
     for(const auto& s:statements){
      reject(s.declared,s.where);if(s.pattern)pattern(pattern,*s.pattern);
      if(s.value)self(self,*s.value);
      visit(visit,s.body);visit(visit,s.otherwise);
     }
    };
   body(body,e.body);
  };
   auto body=[&](auto&& visit,const std::vector<Stmt>& statements)->void{
    for(const auto& s:statements){
     reject(s.declared,s.where);if(s.pattern)pattern(pattern,*s.pattern);
     if(s.value)expression(expression,*s.value);
     visit(visit,s.body);visit(visit,s.otherwise);
    }
   };
  for(const auto& d:unit.nominals)for(const auto& field:d.fields)reject(field,d.where);
   auto check_function=[&](const Function& f){
    reject(f.result,f.where);for(const auto& p:f.params){reject(p.type,p.where);if(p.pattern)pattern(pattern,*p.pattern);}
    body(body,f.body);
   };
   for(const auto& f:unit.functions)
    if(!unit.operator_lazy_functions.contains(f.name))check_function(f);
   for(const auto& d:unit.operators)
    if(!unit.operator_lazy_functions.contains(d.identity))check_function(d.function);
 if(!unit.cliches.empty()||!unit.cliche_implementations.empty())
  throw Error(!unit.cliches.empty()?unit.cliches.front().where:
              unit.cliche_implementations.front().where,
              "ON1X-CLICHE-UNSUPPORTED: Unit-v1 PPX does not transport intrinsic cliche declarations",
              "ON1X-CLICHE-UNSUPPORTED");
 if(!unit.inherent_implementations.empty())
  throw Error(unit.inherent_implementations.front().where,
              "ON1X-IMPL-UNSUPPORTED: Unit-v1 PPX does not transport intrinsic impl declarations",
              "ON1X-IMPL-UNSUPPORTED");
 if(!unit.extensions.empty())throw Error(unit.extensions.front().where,"ON1X-CAT-PPX: declaration carriers require native expansion (inspection does not execute processors)","ON1X-CAT-PPX");
 if(!unit.macros.empty())throw Error(unit.macros.front().where,"expand macros before PPX encoding");
 std::vector<SExpr> functions;
  for(const auto& d:unit.nominals){
  std::vector<SExpr> params,labels,fields;for(const auto& p:d.parameters)params.push_back(string(p));for(const auto& l:d.labels)labels.push_back(string(l));for(const auto& f:d.fields)fields.push_back(type(f));
  functions.push_back(form("NominalDeclaration",{string(d.kind==Type::Record?"record":"enum"),string(d.name),list(std::move(params)),list(std::move(labels)),list(std::move(fields)),loc(d.where)}));
 }
   auto add_function=[&](const Function& f){
     if(f.name.starts_with("@stdffi_"))
      throw Error(f.where,"portable stdffi definitions are not Unit-v1 PPX wire data","ON1X-STDFFI-PPX");
     if(!f.kernel_symbol.empty()||f.kernel_link_only)
    throw Error(f.where,"ON1X-KERNEL-PPX: foreign interfaces are not PPX wire data","ON1X-KERNEL-PPX");
   std::vector<SExpr> attributes;for(const auto& a:f.attributes)attributes.push_back(extension(*a,true));
  if(f.external)functions.push_back(form("Extern",{string(f.name),parameters(f.params),type(f.result),list(std::move(attributes)),loc(f.where)}));
  else functions.push_back(form(f.procedure?"Procedure":"Function",{string(f.name),parameters(f.params),type(f.result),block(f.body,f.where),list(std::move(attributes)),loc(f.where)}));
  };
  // Automatically loaded operator definitions/private helpers stay host-side.
  // Explicitly authored or imported declarations are source PPX syntax.
  for(const auto& f:unit.functions)if(!unit.operator_lazy_functions.contains(f.name))add_function(f);
  for(const auto& d:unit.operators){
   if(unit.operator_lazy_functions.contains(d.identity))continue;
   const auto& f=d.function;
   if(!f.attributes.empty()||f.external||f.procedure)
    throw Error(d.header.where,"PPX Unit v1 requires a source operator function body");
   functions.push_back(form("OperatorDeclaration",{operator_header(d.header),operator_generics(f),
    parameters(f.params),type(f.result),block(f.body,f.where)}));
  }
  for(const auto& d:unit.fixity_directives)functions.push_back(operator_directive(d));
  for(const auto& doc:unit.docstrings)
   functions.push_back(form("Docstring",{string(std::string(1,doc.marker)),string(doc.text),loc(doc.where)}));
  return form("Unit",{symbol("1"),list(std::move(functions))});
}
 namespace {
 bool same_wire(const SExpr& left,const SExpr& right){
  return left.kind==right.kind&&left.text==right.text&&left.items.size()==right.items.size()&&
   std::equal(left.items.begin(),left.items.end(),right.items.begin(),same_wire);
 }
 unsigned operator_rank(const SExpr& n){
  auto rank=number(n);if(rank>9)invalid(n,"operator rank must be in 0..9");
  return static_cast<unsigned>(rank);
 }
 OperatorAssociativity operator_binds(const SExpr& n){
  if(n.text=="left")return OperatorAssociativity::Left;
  if(n.text=="right")return OperatorAssociativity::Right;
  invalid(n,"operator binds must be left or right");
 }
 bool source_spelling(const std::string& text){
  return on1x_operator_spelling(text.data(),text.size());
 }
 OperatorHeader read_operator_header(const SExpr& n){
  OperatorHeader h;h.where=location(field(n,7));h.lexeme=field(n,0).text;h.lexeme2=field(n,1).text;
  const auto& arity=field(n,2).text;const auto& position=field(n,3).text;
  if(arity=="unary")h.arity=OperatorArity::Unary;
  else if(arity=="binary")h.arity=OperatorArity::Binary;
  else if(arity=="ternary")h.arity=OperatorArity::Ternary;
  else invalid(n,"operator arity must be unary, binary, or ternary");
  if(position=="prefix")h.position=OperatorPosition::Prefix;
  else if(position=="postfix")h.position=OperatorPosition::Postfix;
  else if(position=="infix")h.position=OperatorPosition::Infix;
  else invalid(n,"operator position must be prefix, postfix, or infix");
  if((h.arity==OperatorArity::Unary)==(h.position==OperatorPosition::Infix))
   invalid(n,"operator position disagrees with its arity");
  if(!source_spelling(h.lexeme)||
     (h.arity==OperatorArity::Ternary?(!source_spelling(h.lexeme2)||h.lexeme==h.lexeme2):!h.lexeme2.empty()))
   invalid(n,"invalid operator spelling or ternary delimiter");
  h.precedence=operator_rank(field(n,4));h.tightness=operator_rank(field(n,5));h.binds=operator_binds(field(n,6));
  return h;
 }
 OperatorDeclaration read_new_operator(const SExpr& n){
  OperatorDeclaration d;d.header=read_operator_header(field(n,0));
  auto& f=d.function;f.where=location(field(n,4).items.back());
  std::set<std::string> generics,formals;
  for(const auto& g:field(n,1).items){
   if(!generics.insert(g.text).second||generics.size()>32)invalid(g,"invalid operator generic binders");
   f.type_parameters.push_back({d.header.where,g.text,{}});
  }
  const auto count=d.header.arity==OperatorArity::Unary?1u:d.header.arity==OperatorArity::Binary?2u:3u;
  if(field(n,2).items.size()!=count)invalid(n,"operator formal count disagrees with its arity");
  for(const auto& p:field(n,2).items){
   if(tag(p)!="Parameter"||!formals.insert(field(p,0).text).second)
    invalid(p,"operator formals must be distinct names");
   Param formal;formal.where=location(p.items.back());formal.name=field(p,0).text;
   formal.type=read_type(field(p,1),false,generics.size());f.params.push_back(std::move(formal));
  }
  f.result=read_type(field(n,3),false,generics.size());
  f.body=BodyReader{generics.size()}.read_block(field(n,4));
  return d;
 }
 bool scalar_annotation(const Type& value){
  return value.is_numeric()||value==Type::Bool||value==Type::Char||value==Type::String||value==Type::Void;
 }
 void rewrite_operator_signature(const SExpr& n,OperatorDeclaration& d,const Unit& original){
  auto& f=d.function;
  const auto& formals=field(n,2);
  if(same_wire(formals,parameters(f.params))&&same_wire(field(n,3),type(f.result)))return;
  auto owner=original.operator_owners.find(d.identity);
  const bool entry=owner==original.operator_owners.end()?original.source_digests.size()<=1:
                                                        owner->second==original.operator_entry;
  const auto fail=[&]{invalid(n,"PPX cannot change the interface of this intrinsic operator declaration");};
  // Only ordinary scalar annotations and formal names may change in an entry
  // module's plain operator. Its syntax slot/identity and all imported,
  // constrained, borrowed or lexical interfaces remain host-owned.
  if(!entry||d.block_local||f.external||f.extension_generated||!f.type_parameters.empty()||
     !f.constraints.empty()||formals.items.size()!=f.params.size()||!scalar_annotation(f.result))
   fail();
  const auto result=read_type(field(n,3));
  if(!scalar_annotation(result))fail();
  std::set<std::string> names;
  for(std::size_t i=0;i<f.params.size();++i){
   auto& p=f.params[i];
   const auto& wire=formals.items[i];
   if(tag(wire)!="Parameter"||p.pattern||p.borrow!=BorrowMode::Value||
      !scalar_annotation(p.type)||!same_wire(wire.items.back(),loc(p.where))||
      !names.insert(field(wire,0).text).second)
    fail();
   auto t=read_type(field(wire,1));
   if(!scalar_annotation(t)||t==Type::Void)fail();
   p.name=field(wire,0).text;p.type=std::move(t);
  }
  f.result=result;
 }
 template<typename Decl>
 using SourceDeclarations=std::map<std::string,std::vector<const Decl*>>;
 template<typename Decl>
 void restore_declaration_metadata(Decl& declaration,const SourceDeclarations<Decl>& original,
                                   const SExpr& wire,bool structured){
  auto hit=original.find(declaration.name);
  if(hit==original.end()){
   if(structured)declaration.where={"<ppx-generated>"};
   return;
  }
  const auto& candidates=hit->second;
  const auto* before=candidates.front();
  if(!structured&&std::all_of(candidates.begin(),candidates.end(),[](const Decl* d){
       return d->visibility.kind==DeclarationVisibility::Unspecified;
     }))return;
  for(const auto* d:candidates)
   if(d->visibility.kind!=before->visibility.kind||d->visibility.elevation!=before->visibility.elevation)
    invalid(wire,"PPX cannot disambiguate conflicting declaration visibility: "+declaration.name);
  if(candidates.size()>1){
   auto exact=std::find_if(candidates.begin(),candidates.end(),[&](const Decl* d){
     return same_wire(wire.items.back(),loc(d->where));
   });
   if(exact==candidates.end())invalid(wire,"PPX cannot disambiguate source declarations: "+declaration.name);
   before=*exact;
  }
  declaration.visibility=before->visibility;
  declaration.where=before->where;
 }
 Unit decode_unit(const SExpr& tree,Unit* original){
  if(original&&!original->opaque_nominal_owners.empty())
   invalid(tree,"Unit-v1 rewrite cannot transport opaque module ABI authority");
  base_schema().validate(tree);
  std::size_t operators=0,directives=0,serial=0;
  std::set<std::string> hidden_names,identities;
  std::vector<OperatorDeclaration*> exposed;
  OperatorScope entry;
  SourceDeclarations<Function> functions;
  SourceDeclarations<NominalDeclaration> nominals;
  if(original){
   if(auto scope=original->operator_scopes.find(original->operator_entry);scope!=original->operator_scopes.end())
    for(const auto& binding:scope->second)entry.add(binding,binding.header.where);
   for(auto& d:original->operators){
    identities.insert(d.identity);
    if(!original->operator_lazy_functions.contains(d.identity))exposed.push_back(&d);
   }
   for(const auto& f:original->functions)
    if(original->operator_lazy_functions.contains(f.name))hidden_names.insert(f.name);
    else functions[f.name].push_back(&f);
   for(const auto& d:original->nominals)nominals[d.name].push_back(&d);
  }
  std::map<std::string,const Function*> generics;
  if(original){
   for(const auto& f:original->functions)
    if(!f.type_parameters.empty())generics.emplace(f.name,&f);
  }
   Unit unit;for(const auto& n:field(tree,1).items){
    if(tag(n)=="OperatorDeclaration"){
     if(!original)invalid(n,"an operator declaration requires trusted PPX readmission");
     if(operators==exposed.size()){
      if(unit.operators.size()>=operators+32)invalid(n,"PPX operator declaration limit exceeded");
      auto d=read_new_operator(n);
      const auto authored=d.header.where;
      do{
       ++serial;
       d.header.where={"<ppx-generated-operator>",serial,1,serial};
       d.identity=operator_identity(d.header);
      }while(!identities.insert(d.identity).second);
      d.function.name=d.identity;d.function.where=d.header.where;
      entry.add({d.header,d.identity},authored);
      unit.operators.push_back(std::move(d));continue;
     }
     auto& d=*exposed[operators++];const auto& f=d.function;
     auto header=read_operator_header(field(n,0));
      if(header.lexeme!=d.header.lexeme||header.lexeme2!=d.header.lexeme2||
         header.arity!=d.header.arity||header.position!=d.header.position||
         !same_wire(field(field(n,0),7),loc(d.header.where))||
         !same_wire(field(n,1),operator_generics(f)))
       invalid(n,"PPX cannot change the interface or order of intrinsic operator declarations");
      rewrite_operator_signature(n,d,*original);
      auto body=BodyReader{f.type_parameters.size()}.read_block(field(n,4));
     d.header.precedence=header.precedence;d.header.tightness=header.tightness;d.header.binds=header.binds;
     d.function.body=std::move(body);unit.operators.push_back(std::move(d));
     continue;
    }
    if(tag(n)=="FixityDirective"){
     if(!original)invalid(n,"a fixity directive requires trusted PPX readmission");
     if(directives==original->fixity_directives.size()){
       if(unit.fixity_directives.size()>=directives+32)invalid(n,"PPX fixity directive limit exceeded");
       const auto& lexeme=field(n,0).text;
       if(!source_spelling(lexeme))invalid(n,"invalid fixity spelling");
       // An appended directive has no authored lexical position. In
       // particular a wire location cannot apply it retroactively to source.
       auto generated=unit.fixity_directives.size()-directives+1;
       unit.fixity_directives.push_back({{"<ppx-generated-fixity>",generated,1,generated},
                                        lexeme,operator_rank(field(n,1)),operator_binds(field(n,2))});
      continue;
     }
     auto d=original->fixity_directives[directives++];
     if(d.lexeme!=field(n,0).text||!same_wire(field(n,3),loc(d.where)))
      invalid(n,"PPX cannot redirect an existing operator fixity directive");
     d.tightness=operator_rank(field(n,1));d.binds=operator_binds(field(n,2));
     unit.fixity_directives.push_back(std::move(d));continue;
    }
   if(tag(n)=="Docstring"){
    const auto& marker=field(n,0).text;
    if(marker!="!"&&marker!=">")invalid(n,"unknown docstring marker");
    unit.docstrings.push_back({marker[0],field(n,1).text,location(field(n,2))});
    continue;
   }
  if(tag(n)=="NominalDeclaration"){
   NominalDeclaration d;d.where=location(n.items.back());auto kind=field(n,0).text;d.kind=kind=="record"?Type::Record:kind=="enum"?Type::Sum:Type::Invalid;d.name=field(n,1).text;
   for(const auto& p:field(n,2).items)d.parameters.push_back(p.text);
   for(const auto& l:field(n,3).items)d.labels.push_back(l.text);
    if(d.parameters.size()>32||field(n,4).items.size()>256||unit.nominals.size()>=4096)invalid(n,"nominal declaration budget exceeded");
    for(const auto& f:field(n,4).items)d.fields.push_back(read_type(f,false,d.parameters.size()));
    if(original)restore_declaration_metadata(d,nominals,n,original->module.has_value());
   unit.nominals.push_back(std::move(d));continue;
  }
  Function f;f.where=location(n.items.back());
  f.external=tag(n)=="Extern";f.procedure=tag(n)=="Procedure";
  if(!f.external&&!f.procedure&&tag(n)!="Function")invalid(n,"unhandled declaration extension "+tag(n));
   f.name=field(n,0).text;
   if(hidden_names.contains(f.name))invalid(n,"PPX cannot replace a private operator helper: "+f.name);
   if(auto saved=generics.find(f.name);saved!=generics.end()){
    // Unit v1 has no generic-binder field on ordinary Function. In particular,
    // automatically visible operators can depend on generic ordinary helpers.
    // Read their source bodies under the existing host-declared binder list.
    const auto& baseline=*saved->second;
    if(f.external!=baseline.external||f.procedure!=baseline.procedure||
       !same_wire(field(n,1),parameters(baseline.params))||
       !same_wire(field(n,2),type(baseline.result)))
     invalid(n,"PPX cannot change a generic function interface");
    f.type_parameters=baseline.type_parameters;f.constraints=baseline.constraints;
    f.params=baseline.params;f.result=baseline.result;
   }else{
    f.params=read_parameters(field(n,1));f.result=read_type(field(n,2));
   }
   if(!f.external)f.body=BodyReader{f.type_parameters.size()}.read_block(field(n,3));
   const auto& attrs=field(n,f.external?3:4);if(!attrs.items.empty())invalid(attrs.items[0],"unhandled function attribute");
   if(original)restore_declaration_metadata(f,functions,n,original->module.has_value());
  unit.functions.push_back(std::move(f));
 }
   if(original){
    unit.module=std::move(original->module);
    unit.module_imports=std::move(original->module_imports);
    unit.imports=std::move(original->imports);
    if(operators!=exposed.size()||directives!=original->fixity_directives.size())
    invalid(tree,"PPX cannot erase operator declarations or fixity directives");
   for(std::size_t i=directives;i<unit.fixity_directives.size();++i){
    const auto& d=unit.fixity_directives[i];
    if(!entry.find(d.lexeme,OperatorPosition::Infix))
     throw Error(d.where,"PPX fixity directive requires a visible infix operator: '"+d.lexeme+"'","ON1X-OPERATOR-PPX");
   }
   unit.operator_scopes=std::move(original->operator_scopes);unit.operator_owners=std::move(original->operator_owners);
   unit.operator_entry=std::move(original->operator_entry);
   unit.operator_lazy_functions=std::move(original->operator_lazy_functions);
   if(unit.operators.size()>operators){
    record_operator_scope(unit,entry,unit.operator_entry);
    for(std::size_t i=operators;i<unit.operators.size();++i)
     unit.operator_owners[unit.operators[i].identity]=unit.operator_entry;
   }
   for(auto& d:original->operators)
    if(unit.operator_lazy_functions.contains(d.identity))unit.operators.push_back(std::move(d));
   for(auto& f:original->functions)
    if(unit.operator_lazy_functions.contains(f.name))unit.functions.push_back(std::move(f));
  }
  if(unit.functions.empty()&&unit.operators.empty())invalid(tree,"empty compilation unit");
  return unit;
 }
 void require_ready(const Unit& unit,bool source){
  require_no_kernel_imports(unit);
  if(!source&&(!unit.operators.empty()||!unit.fixity_directives.empty()))
   throw Error(!unit.operators.empty()?unit.operators.front().header.where:unit.fixity_directives.front().where,
    "intrinsic operator declarations/fixity must be lowered before core checking","ON1X-OPERATOR-LOWER");
 if(!unit.extensions.empty())throw Error(unit.extensions.front().where,"ON1X-CAT-PPX: unconsumed declaration carrier","ON1X-CAT-PPX");
  if(!unit.macros.empty())throw Error(unit.macros.front().where,"unexpanded macro definitions");
  if(!unit.ppx.empty())throw Error(unit.ppx.front().where,"source PPX registrations require --allow-source-ppx in the compiler driver");
  // Check after source rewriting and before inference can resolve an explicit
  // result hole. Only a nested callable changes the return's lexical owner.
  auto require_void=[](const Type& result,const Location& at){
   if(result!=Type::Void)throw Error(at,"proc must omit its result annotation or declare -> void","ON1X-PROC");
  };
   auto body=[](auto&& self,const auto& expr,const std::vector<Stmt>& statements,bool procedure)->void{
    auto statement=[&](auto&& visit,const Stmt& s)->void{
     if(procedure&&s.kind==Stmt::Kind::Return&&s.value)
      throw Error(s.where,"proc cannot return an expression; use bare return;","ON1X-PROC");
     if(s.value)expr(expr,*s.value,procedure);
     self(self,expr,s.body,procedure);self(self,expr,s.otherwise,procedure);
     for(const auto& instance:s.instances)if(instance)visit(visit,*instance);
    };
    for(const auto& s:statements)statement(statement,s);
  };
   auto expr=[&](auto&& self,const Expr& e,bool procedure)->void{
    if(e.kind==K::MacroCall||e.kind==K::SyntaxHole||e.kind==K::SyntaxBlock||e.kind==K::Extension||e.kind==K::SyntaxList||e.kind==K::SyntaxVector||e.kind==K::SyntaxKeyword||e.kind==K::SyntaxLet||!e.syntax_definitions.empty()||e.syntax_tail)throw Error(e.where,"unhandled metaprogramming syntax"+(e.text.empty()?"":": "+e.text));
    if(e.kind==K::OperatorReference||e.kind==K::OperatorUnary||e.kind==K::OperatorBinary||e.kind==K::OperatorTernary){
     if(!source)throw Error(e.where,"intrinsic operator must be lowered before core checking","ON1X-OPERATOR-LOWER");
     auto count=e.kind==K::OperatorReference?0u:e.kind==K::OperatorUnary?1u:e.kind==K::OperatorBinary?2u:3u;
     if(e.children.size()!=count||e.callee)
      throw Error(e.where,"intrinsic operator has the wrong operand shape","ON1X-OPERATOR-ARITY");
    }
    if((e.kind==K::Unary||e.kind==K::Binary)&&e.children.empty())
     throw Error(e.where,"native operator syntax is not a first-class reference","ON1X-OPERATOR-ARITY");
   if(e.procedure)require_void(e.result,e.where);
    if(e.callee)self(self,*e.callee,procedure);
     for(const auto& c:e.children){
      if(!c)throw Error(e.where,"missing expression operand","ON1X-AST-MALFORMED");
      self(self,*c,procedure);
     }
     for(const auto& proof:e.proofs){
      if(!proof)throw Error(e.where,"missing proof expression","ON1X-AST-MALFORMED");
      self(self,*proof,procedure);
     }
   body(body,self,e.body,e.kind==K::Lambda?e.procedure:procedure);
  };
   auto callable=[&](const Function& f){
    if(!f.attributes.empty()){
     if(!f.attributes.front())throw Error(f.where,"missing PPX attribute","ON1X-AST-MALFORMED");
     throw Error(f.attributes.front()->where,"unhandled PPX attribute: "+f.attributes.front()->text);
    }
   if(f.procedure)require_void(f.result,f.where);
   body(body,expr,f.body,f.procedure);
   };
   for(const auto& f:unit.functions)callable(f);
   for(const auto& d:unit.operators)callable(d.function);
   for(const auto& implementation:unit.cliche_implementations)for(const auto& member:implementation.members){
    callable(member.function);if(member.value)expr(expr,*member.value,false);
   }
   for(const auto& implementation:unit.inherent_implementations)for(const auto& member:implementation.members){
    callable(member.function);if(member.value)expr(expr,*member.value,false);
   }
 }
 }
 Unit decode(const SExpr& tree){return decode_unit(tree,nullptr);}
 Unit decode_rewrite(const SExpr& tree,Unit& original){return decode_unit(tree,&original);}
 void require_source_ready(const Unit& unit){require_ready(unit,true);}
 void require_core(const Unit& unit){require_ready(unit,false);}
 }
