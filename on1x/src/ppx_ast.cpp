#include <on1x/meta.hpp>
#include <charconv>
namespace on1x::meta {
namespace {
using K=Expr::Kind;
SExpr form(const std::string& tag,std::vector<SExpr> fields={}){fields.insert(fields.begin(),symbol(tag));return list(std::move(fields));}
SExpr loc(const Location& at){return form("Loc",{string(at.file),symbol(std::to_string(at.offset)),symbol(std::to_string(at.line)),symbol(std::to_string(at.column))});}
SExpr type(const Type& t){
 switch(t.kind()){
 case Type::Invalid:return form("Inferred");case Type::Hole:return form("HoleType");
 case Type::I64:return form("I64Type");case Type::Bool:return form("BoolType");case Type::String:return form("StringType");
  case Type::Void:return form("VoidType");case Type::Json:return form("JsonType");case Type::JsonView:return form("JsonViewType");
  case Type::SExpr:return form("SExprType");case Type::Buffer:return form("BufferType");
  case Type::Code:return form("CodeType",{type(t.code_result())});
 case Type::Bound:return form("BoundType",{symbol(std::to_string(t.generic_identity()))});
 case Type::Forall:return form("ForallType",{symbol(std::to_string(t.generic_identity())),type(t.body())});
 case Type::Tuple:{std::vector<SExpr> fields;for(const auto& e:t.elements())fields.push_back(type(e));return form("TupleType",{list(std::move(fields))});}
 case Type::Function:{std::vector<SExpr> fields;for(const auto& p:t.parameters())fields.push_back(type(p));return form("FunctionType",{list(std::move(fields)),type(t.result())});}
 default:throw std::runtime_error("PPX accepts source AST types, not inferred HIR identities");
 }
}
SExpr pattern(const Pattern& p){
 if(!p.is_tuple())return form("NamePattern",{string(p.name),loc(p.where)});
 std::vector<SExpr> fields;for(const auto& e:p.elements)fields.push_back(pattern(e));return form("TuplePattern",{list(std::move(fields)),loc(p.where)});
}
SExpr parameters(const std::vector<Param>& ps){
 std::vector<SExpr> out;for(const auto& p:ps)out.push_back(p.pattern?form("PatternParameter",{pattern(*p.pattern),type(p.type),loc(p.where)}):form("Parameter",{string(p.name),type(p.type),loc(p.where)}));return list(std::move(out));
}
SExpr expression(const Expr& e);
SExpr block(const std::vector<Stmt>& body,const Location& at);
SExpr extension(const Expr& e,bool attribute){
 std::vector<SExpr> args;for(std::size_t i=1;i<e.children.size();++i)args.push_back(expression(*e.children[i]));
 return form(attribute?"Attribute":"ExtensionExpr",{string(e.text),string(literal_value(e.children.at(0)->text)),list(std::move(args)),loc(e.where)});
}
SExpr expression(const Expr& e){
 auto at=loc(e.where);
 switch(e.kind){
 case K::Integer:return form(e.text.starts_with("0x")||e.text.starts_with("0X")?"HexLiteral":"DecimalLiteral",{string(e.text),std::move(at)});
 case K::String:return form("StringLiteral",{string(literal_value(e.text)),std::move(at)});
 case K::Boolean:return form("BooleanLiteral",{symbol(e.text),std::move(at)});
 case K::Variable:return form("Identifier",{string(e.text),std::move(at)});
 case K::Unary:return form("Unary",{symbol(e.text),expression(*e.children.at(0)),std::move(at)});
 case K::Binary:return form("Binary",{symbol(e.text),expression(*e.children.at(0)),expression(*e.children.at(1)),std::move(at)});
 case K::Tuple:case K::Call:{
  std::vector<SExpr> args;for(const auto& c:e.children)args.push_back(expression(*c));
  if(e.kind==K::Call)return form("Call",{expression(*e.callee),list(std::move(args)),std::move(at)});
  return form("Tuple",{list(std::move(args)),std::move(at)});
 }
  case K::Lambda:return form("Lambda",{parameters(e.parameters),type(e.result),block(e.body,e.where),std::move(at)});
  case K::Quote:case K::Splice:case K::Lift:case K::Run:
   return form(e.kind==K::Quote?"Quote":e.kind==K::Splice?"Splice":e.kind==K::Lift?"Lift":"Run",{expression(*e.children.at(0)),std::move(at)});
 case K::SyntaxBlock:return block(e.body,e.where);
 case K::Extension:return extension(e,false);
 default:throw Error(e.where,"PPX serialization requires macro-expanded, untyped source AST");
 }
}
SExpr statement(const Stmt& s){
 auto at=loc(s.where);
 switch(s.kind){
 case Stmt::Kind::Let:
  return s.pattern?form("LetPattern",{pattern(*s.pattern),type(s.declared),expression(*s.value),std::move(at)}):form(s.mutable_binding?"Var":"Let",{string(s.name),type(s.declared),expression(*s.value),std::move(at)});
 case Stmt::Kind::Assign:return form("Assign",{string(s.name),expression(*s.value),std::move(at)});
 case Stmt::Kind::Return:return s.value?form("Return",{expression(*s.value),std::move(at)}):form("ReturnVoid",{std::move(at)});
 case Stmt::Kind::If:return form("If",{expression(*s.value),block(s.body,s.where),block(s.otherwise,s.where),std::move(at)});
 case Stmt::Kind::While:return form("While",{expression(*s.value),block(s.body,s.where),std::move(at)});
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
 if(k=="Inferred"){if(!inferred)invalid(n,"Inferred is only valid for a local annotation");return Type::Invalid;}
 if(k=="HoleType")return Type::Hole;
  for(auto kind:{Type::I64,Type::Bool,Type::String,Type::Void,Type::Json,Type::JsonView,Type::SExpr,Type::Buffer})if(tag(type(Type(kind)))==k)return kind;
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
 else if(tag(n)=="TuplePattern"){
  for(const auto& c:field(n,0).items)p.elements.push_back(read_pattern(c));
  if(p.elements.size()<2)invalid(n,"tuple pattern requires at least two elements");
 }else invalid(n,"unhandled pattern constructor "+tag(n));return p;
}
std::vector<Param> read_parameters(const SExpr& n){
 std::vector<Param> ps;for(const auto& c:n.items){Param p;p.where=location(c.items.back());p.type=read_type(field(c,1));
  if(tag(c)=="Parameter")p.name=field(c,0).text;
  else if(tag(c)=="PatternParameter"){p.pattern=read_pattern(field(c,0));p.name="@argument_"+std::to_string(ps.size());}
  else invalid(c,"unhandled parameter constructor "+tag(c));
  ps.push_back(std::move(p));
 }return ps;
}
std::vector<Stmt> read_block(const SExpr& n);
ExprPtr read_expression(const SExpr& n){
 const auto& k=tag(n);auto e=std::make_unique<Expr>();e->where=location(n.items.back());
 if(k=="DecimalLiteral"||k=="HexLiteral"){e->kind=K::Integer;e->text=field(n,0).text;}
 else if(k=="StringLiteral"){e->kind=K::String;e->text=source_literal(field(n,0).text);}
 else if(k=="BooleanLiteral"){e->kind=K::Boolean;e->text=field(n,0).text;}
 else if(k=="Identifier"){e->kind=K::Variable;e->text=field(n,0).text;}
 else if(k=="Unary"||k=="Binary"){
  e->kind=k=="Unary"?K::Unary:K::Binary;e->text=field(n,0).text;e->children.push_back(read_expression(field(n,1)));
  if(k=="Binary")e->children.push_back(read_expression(field(n,2)));
 }else if(k=="Tuple"||k=="Call"){
  e->kind=k=="Tuple"?K::Tuple:K::Call;auto index=k=="Tuple"?0:1;
  if(k=="Call")e->callee=read_expression(field(n,0));
  for(const auto& c:field(n,index).items)e->children.push_back(read_expression(c));
  if(k=="Tuple"&&e->children.size()<2)invalid(n,"tuple expression requires at least two elements");
  }else if(k=="Quote"||k=="Splice"||k=="Lift"||k=="Run"){
   e->kind=k=="Quote"?K::Quote:k=="Splice"?K::Splice:k=="Lift"?K::Lift:K::Run;e->children.push_back(read_expression(field(n,0)));
  }else if(k=="Lambda"){
  e->kind=K::Lambda;e->parameters=read_parameters(field(n,0));e->result=read_type(field(n,1));e->body=read_block(field(n,2));
 }else if(k=="ExtensionExpr"||k=="Attribute")invalid(n,"unhandled extension @"+field(n,0).text+"("+field(n,1).text+")");
 else invalid(n,"unhandled extension constructor "+k);
 return e;
}
std::vector<Stmt> read_block(const SExpr& n){
 if(tag(n)!="Block")invalid(n,"unhandled block constructor "+tag(n));
 std::vector<Stmt> body;
 for(const auto& c:field(n,0).items){Stmt s;s.where=location(c.items.back());const auto& k=tag(c);
  if(k=="Let"||k=="Var"||k=="LetPattern"){
   s.kind=Stmt::Kind::Let;s.mutable_binding=k=="Var";if(k=="LetPattern")s.pattern=read_pattern(field(c,0));else s.name=field(c,0).text;
   s.declared=read_type(field(c,1),true);s.value=read_expression(field(c,2));
  }else if(k=="Assign"){s.kind=Stmt::Kind::Assign;s.name=field(c,0).text;s.value=read_expression(field(c,1));}
  else if(k=="Return"||k=="ReturnVoid"){s.kind=Stmt::Kind::Return;if(k=="Return")s.value=read_expression(field(c,0));}
  else if(k=="If"||k=="While"){
   s.kind=k=="If"?Stmt::Kind::If:Stmt::Kind::While;s.value=read_expression(field(c,0));s.body=read_block(field(c,1));if(k=="If")s.otherwise=read_block(field(c,2));
  }else if(k=="Scope"){s.kind=Stmt::Kind::Block;s.body=read_block(field(c,0));}
  else if(k=="Do"){s.kind=Stmt::Kind::Expr;s.value=read_expression(field(c,0));}
  else invalid(c,"unhandled statement extension "+k);
  body.push_back(std::move(s));
 }return body;
}
}
SExpr encode(const Unit& unit){
 if(!unit.macros.empty())throw Error(unit.macros.front().where,"expand macros before PPX encoding");
 std::vector<SExpr> functions;
 for(const auto& f:unit.functions){
  std::vector<SExpr> attributes;for(const auto& a:f.attributes)attributes.push_back(extension(*a,true));
  if(f.external)functions.push_back(form("Extern",{string(f.name),parameters(f.params),type(f.result),list(std::move(attributes)),loc(f.where)}));
  else functions.push_back(form("Function",{string(f.name),parameters(f.params),type(f.result),block(f.body,f.where),list(std::move(attributes)),loc(f.where)}));
 }return form("Unit",{symbol("1"),list(std::move(functions))});
}
Unit decode(const SExpr& tree){
 base_schema().validate(tree);
 Unit unit;for(const auto& n:field(tree,1).items){Function f;f.where=location(n.items.back());
  f.external=tag(n)=="Extern";if(!f.external&&tag(n)!="Function")invalid(n,"unhandled declaration extension "+tag(n));
  f.name=field(n,0).text;f.params=read_parameters(field(n,1));f.result=read_type(field(n,2));
  if(!f.external)f.body=read_block(field(n,3));
  const auto& attrs=field(n,f.external?3:4);if(!attrs.items.empty())invalid(attrs.items[0],"unhandled function attribute");
  unit.functions.push_back(std::move(f));
 }
 if(unit.functions.empty())invalid(tree,"empty compilation unit");
 return unit;
}
void require_core(const Unit& unit){
 if(!unit.macros.empty())throw Error(unit.macros.front().where,"unexpanded macro definitions");
 if(!unit.ppx.empty())throw Error(unit.ppx.front().where,"source PPX registrations require --allow-source-ppx in the compiler driver");
 auto expr=[&](auto&& self,const Expr& e)->void{
 if(e.kind==K::MacroCall||e.kind==K::SyntaxHole||e.kind==K::SyntaxBlock||e.kind==K::Extension||e.kind==K::SyntaxList||e.kind==K::SyntaxVector||e.kind==K::SyntaxKeyword||e.syntax_tail)throw Error(e.where,"unhandled metaprogramming syntax"+(e.text.empty()?"":": "+e.text));
  if(e.callee)self(self,*e.callee);
  for(const auto& c:e.children)self(self,*c);
  auto stmts=[&](auto&& visit,const std::vector<Stmt>& body)->void{for(const auto& s:body){if(s.value)self(self,*s.value);visit(visit,s.body);visit(visit,s.otherwise);}};
  stmts(stmts,e.body);
 };
 auto body=[&](auto&& self,const std::vector<Stmt>& b)->void{for(const auto& s:b){if(s.value)expr(expr,*s.value);self(self,s.body);self(self,s.otherwise);}};
 for(const auto& f:unit.functions){if(!f.attributes.empty())throw Error(f.attributes.front()->where,"unhandled PPX attribute: "+f.attributes.front()->text);body(body,f.body);}
}
}
