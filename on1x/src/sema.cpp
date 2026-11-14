#include <on1x/ast.hpp>
#include <on1x/infer.hpp>
#include <charconv>
#include <climits>
#include <map>
#include <system_error>
#include <set>
namespace on1x {
bool is_intrinsic_name(const std::string& name){
 static const std::set<std::string> names={"println","assert","json_parse_or_die","json_get_or_die","json_size","json_number","json_print"};
 return names.contains(name);
}
namespace {
struct Binding { Type type; bool is_mutable; std::size_t scope; };
class Checker {
 Unit& unit;
 std::map<std::string,const Function*> funcs;
 std::map<std::string,Type> signatures;
 std::set<Type> checked_function_types;
 std::vector<std::map<std::string,Binding>> scopes;
 struct LambdaFrame { Expr* expression; std::size_t scope_begin; std::set<std::string> names; };
 std::vector<LambdaFrame> lambdas;
 std::size_t capture_count=0,capture_bytes=0;
 Type returns=Type::Void;
 [[noreturn]] void fail(const Location& loc,const std::string& msg){throw Error(loc,msg);}
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
   auto& captures=frame->expression->captures;
   if(frame->names.contains(name))continue;
   if(capture_count>=16384||name.size()>max_source_bytes-capture_bytes)
    fail(loc,"closure capture metadata limit exceeded");
   frame->names.insert(name);
   ++capture_count;capture_bytes+=name.size();captures.push_back({name,binding.type});
  }
 }
 static bool resource(Type t){return t==Type::Json||t==Type::JsonView;}
 void equal(Type actual,Type expected,const Location& loc){if(actual!=expected)fail(loc,std::string("expected ")+type_name(expected)+", got "+type_name(actual));}
 void validate_function_type(const Type& type,const Location& loc){
  if(!type.is_function())return;
  if(checked_function_types.contains(type))return;
  for(const auto& parameter:type.parameters()){
   if(parameter==Type::Void)fail(loc,"function parameter type cannot be void");
   if(resource(parameter))fail(loc,"function types cannot contain resource parameters");
   validate_function_type(parameter,loc);
  }
  if(resource(type.result()))fail(loc,"function types cannot contain resource returns");
  validate_function_type(type.result(),loc);
  checked_function_types.insert(type);
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
  if(n=="println"){arity(1);if(arg[0]!=Type::I64&&arg[0]!=Type::Bool&&arg[0]!=Type::String)fail(e.where,"println accepts i64, bool or string");return e.type=Type::Void;}
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
    case Expr::Kind::Integer:{std::int64_t v=0;auto [p,err]=std::from_chars(e.text.data(),e.text.data()+e.text.size(),v);
     if(err!=std::errc{}||p!=e.text.data()+e.text.size())fail(e.where,"integer literal out of i64 range");
     e.text=std::to_string(v); // Decimal source spelling must not become C octal.
     return e.type=Type::I64;}
   case Expr::Kind::Boolean:return e.type=Type::Bool;
   case Expr::Kind::String:
    // Current string ABI is const char*: do not imply support for binary NUL.
     for(std::size_t i=1;i+1<e.text.size();++i)if(e.text[i]=='\\'){
      if(e.text[++i]=='0')fail(e.where,"NUL escape is not supported by the C-string ABI");
     }
    return e.type=Type::String;
    case Expr::Kind::Variable:return resolve_name(e);
    case Expr::Kind::Lambda:{
     std::vector<Type> parameters;std::set<std::string> names;
     for(const auto& parameter:e.parameters){
      if(!names.insert(parameter.name).second)fail(parameter.where,"duplicate parameter: "+parameter.name);
      parameters.push_back(parameter.type);
     }
     auto type=Type::function(std::move(parameters),e.result);validate_function_type(type,e.where);
     e.captures.clear();auto previous_returns=returns;
      lambdas.push_back({&e,scopes.size(),{}});push();returns=e.result;
     for(const auto& parameter:e.parameters)define(parameter.name,parameter.type,false,parameter.where);
     bool complete=check_block(e.body,false);pop();lambdas.pop_back();returns=previous_returns;
     if(e.result!=Type::Void&&!complete)fail(e.where,"not all paths return in anonymous function");
     return e.type=std::move(type);
    }
   case Expr::Kind::Unary:{auto x=child(0);equal(x,e.text=="!"?Type::Bool:Type::I64,e.where);return e.type=x;}
   case Expr::Kind::Binary:{Type a=child(0),b=child(1);const auto&op=e.text;
    if(op=="+"||op=="-"||op=="*"||op=="/"||op=="%"){
     equal(a,Type::I64,e.where);equal(b,Type::I64,e.where);return e.type=Type::I64;}
    if(op=="&&"||op=="||"){equal(a,Type::Bool,e.where);equal(b,Type::Bool,e.where);return e.type=Type::Bool;}
    if(op=="<"||op==">"||op=="<="||op==">="){
     equal(a,Type::I64,e.where);equal(b,Type::I64,e.where);return e.type=Type::Bool;}
    if(op=="=="||op=="!="){
     if(a!=b||a==Type::Void||resource(a)||a.is_function())fail(e.where,"incompatible or non-scalar equality operands");
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
  for(auto& s:body){if(guarantees)fail(s.where,"unreachable statement after return");
   switch(s.kind){
     case Stmt::Kind::Let:{auto t=check_expr(*s.value);
      validate_function_type(s.declared,s.where);
     if(t==Type::Void)fail(s.where,"cannot bind a void expression");
     if(s.declared!=Type::Invalid)equal(t,s.declared,s.where);
     // Every JSON owner is unique. A name cannot be copied into another owner.
     if(resource(t)&&s.value->kind==Expr::Kind::Variable)fail(s.where,"resource aliases require an explicit borrowed API");
     if(t==Type::Json&&s.value->kind!=Expr::Kind::Call)fail(s.where,"json owners must originate from a constructor");
     if(t==Type::JsonView&&s.value->kind!=Expr::Kind::Call)fail(s.where,"json views may only come from borrowed operations");
     if(t==Type::Json||t==Type::JsonView){if(s.mutable_binding)fail(s.where,"mutable resources not yet supported");}
     define(s.name,t,s.mutable_binding,s.where);break;
    }
    case Stmt::Kind::Assign:{const auto& binding=find(s.name,s.where);if(!binding.is_mutable)fail(s.where,"assignment to immutable binding: "+s.name);
     if(resource(binding.type))fail(s.where,"resource reassignment not supported");
     equal(check_expr(*s.value),binding.type,s.where);break;}
    case Stmt::Kind::Return:{Type t=s.value?check_expr(*s.value):Type::Void;equal(t,returns,s.where);
     if(resource(t))fail(s.where,"returning resources requires ownership/lifetime lowering");
     guarantees=true;break;}
    case Stmt::Kind::If:{equal(check_expr(*s.value),Type::Bool,s.where);bool yes=check_block(s.body),no=check_block(s.otherwise);guarantees=yes&&!s.otherwise.empty()&&no;break;}
    case Stmt::Kind::While:{equal(check_expr(*s.value),Type::Bool,s.where);(void)check_block(s.body);break;}
    case Stmt::Kind::Block:{guarantees=check_block(s.body);break;}
    case Stmt::Kind::Expr:{Type t=check_expr(*s.value);
     if(resource(t))fail(s.where,"discarding an owned or borrowed resource is not allowed");
     break;}
   }
  }
  if(owns_scope)pop();
  return guarantees;
 }
 public:
 explicit Checker(Unit& u):unit(u){}
 void run(){
   for(auto& f:unit.functions){
    if(is_intrinsic_name(f.name))fail(f.where,"intrinsic name cannot be redefined: "+f.name);
    if(f.external) {
    static const std::set<std::string> c_keywords={"auto","break","case","char","const","continue","default","do","double","else","enum","extern","float","for","goto","if","inline","int","long","register","restrict","return","short","signed","sizeof","static","struct","switch","typedef","union","unsigned","void","volatile","while","_Alignas","_Alignof","_Atomic","_Bool","_Complex","_Generic","_Imaginary","_Noreturn","_Static_assert","_Thread_local"};
     if(c_keywords.contains(f.name))fail(f.where,"C keyword cannot be a raw extern name: "+f.name);
     if(f.result.is_function())fail(f.where,"function-valued extern ABI is unsupported; callbacks require reviewed contracts");
   }
   if(!funcs.emplace(f.name,&f).second)fail(f.where,"duplicate function: "+f.name);
    if(f.result==Type::Json||f.result==Type::JsonView)fail(f.where,"resource returns not supported in the initial ABI");
    validate_function_type(f.result,f.where);
    std::vector<Type> parameters;
    std::set<std::string> names;for(auto& p:f.params){if(!names.insert(p.name).second)fail(p.where,"duplicate parameter: "+p.name);
     if(p.type==Type::Void||p.type==Type::Json||p.type==Type::JsonView)fail(p.where,"unsupported parameter type in this milestone");
     if(f.external&&p.type.is_function())fail(p.where,"function-valued extern ABI is unsupported; callbacks require reviewed contracts");
     validate_function_type(p.type,p.where);parameters.push_back(p.type);
    }
    signatures.emplace(f.name,Type::function(std::move(parameters),f.result));
  }
  if(!funcs.contains("main"))fail(unit.functions.front().where,"missing main function");
  auto main=funcs.at("main");if(main->external||!main->params.empty()||(main->result!=Type::I64&&main->result!=Type::Void))fail(main->where,"main must be fn main() -> i64 or void");
  for(auto& f:unit.functions){if(f.external)continue;returns=f.result;push();
   for(auto& p:f.params)define(p.name,p.type,false,p.where);
   bool complete=check_block(f.body,false);pop();
   if(f.result!=Type::Void&&!complete)fail(f.where,"not all paths return in function: "+f.name);
  }
 }
};
}
void check(Unit&u){infer_types(u);Checker(u).run();}
}
