#include <on1x/ast.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace on1x {
namespace {
std::string c_string(const std::string& source){
 std::string out="\"";
 for(std::size_t i=1;i+1<source.size();++i){
  unsigned char byte=static_cast<unsigned char>(source[i]);
  if(byte=='\\'){
   byte=static_cast<unsigned char>(source[++i]);
   if(byte=='n')byte='\n';else if(byte=='r')byte='\r';else if(byte=='t')byte='\t';
  }
  if(byte=='"'||byte=='\\'||byte=='?'){out+='\\';out+=static_cast<char>(byte);}
  else if(byte>=32&&byte<=126)out+=static_cast<char>(byte);
  else {
   out+='\\';out+=static_cast<char>('0'+(byte>>6));
   out+=static_cast<char>('0'+((byte>>3)&7));out+=static_cast<char>('0'+(byte&7));
  }
 }
 return out+'"';
}

class Generator {
 struct Owner { Type type; std::string name; };
 struct Lambda { const Expr* expression; std::string invoke,make,environment,destroy; };
 const Unit& unit;
 std::ostringstream types,prototypes,helpers,definitions;
 std::map<Type,std::string> function_types;
 std::set<std::string> identifiers;
 std::map<std::string,std::string> functions,adapters;
 std::vector<Lambda> lambdas;
 std::unordered_map<const Expr*,std::size_t> lambda_index;
 std::vector<std::unordered_map<std::string,std::string>> scopes;
 std::vector<std::vector<Owner>> owners;
 // Every function-valued expression owns one temporary reference. These are
 // released at the end of the full expression, not at an enclosing loop exit.
 std::vector<std::string> temporary_functions;
 std::uint64_t serial=0,type_serial=0;
 int indent=0;

 std::string reserve(const std::string& preferred){
  std::string name=preferred;std::size_t suffix=0;
  while(!identifiers.insert(name).second)name=preferred+"_"+std::to_string(++suffix);
  return name;
 }
 std::string unique(const std::string& name){return reserve("on1x_v"+std::to_string(++serial)+"_"+name);}
 std::string pad()const{return std::string(indent*2,' ');}
 std::string ctype(const Type& type){
  switch(type.kind()){
  case Type::I64:return "int64_t";case Type::Bool:return "bool";
  case Type::String:return "const char *";case Type::Void:return "void";
  case Type::Json:return "on1x_json *";case Type::JsonView:return "const on1x_json *";
  case Type::Function:{
   if(auto found=function_types.find(type);found!=function_types.end())return found->second;
   auto result=ctype(type.result());std::vector<std::string> parameters;
   for(const auto& parameter:type.parameters())parameters.push_back(ctype(parameter));
   auto name=reserve("on1x_function_type_"+std::to_string(++type_serial));
   function_types.emplace(type,name);
   types<<"typedef struct "<<name<<" {\n  "<<result<<" (*invoke)(on1x_env *";
   for(const auto& parameter:parameters)types<<", "<<parameter;
   types<<");\n  on1x_env *environment;\n} "<<name<<";\n";
   return name;
  }
  default:throw std::runtime_error("unknown backend type");
  }
 }
 void collect(const Expr& expression){
  if(expression.kind==Expr::Kind::Lambda){
   auto index=lambdas.size();auto suffix=std::to_string(index+1);
   lambda_index.emplace(&expression,index);
   lambdas.push_back({&expression,reserve("on1x_lambda_"+suffix),reserve("on1x_make_lambda_"+suffix),
                      reserve("on1x_lambda_env_"+suffix),reserve("on1x_drop_lambda_"+suffix)});
   collect(expression.body);
  }
  if(expression.callee)collect(*expression.callee);
  for(const auto& child:expression.children)collect(*child);
 }
 void collect(const std::vector<Stmt>& body){
  for(const auto& statement:body){
   if(statement.value)collect(*statement.value);
   collect(statement.body);collect(statement.otherwise);
  }
 }
 void push(){scopes.emplace_back();owners.emplace_back();++indent;}
 void release(const Owner& owner){
  if(owner.type==Type::Json)definitions<<pad()<<"on1x_json_drop("<<owner.name<<");\n";
  else definitions<<pad()<<"on1x_env_release("<<owner.name<<".environment);\n";
 }
 void drops(){
  for(auto scope=owners.rbegin();scope!=owners.rend();++scope)
   for(auto owner=scope->rbegin();owner!=scope->rend();++owner)release(*owner);
 }
 void pop(bool cleanup=true){
  if(cleanup)for(auto owner=owners.back().rbegin();owner!=owners.back().rend();++owner)release(*owner);
  scopes.pop_back();owners.pop_back();--indent;
 }
 std::string ref(const std::string& name)const{
  for(auto scope=scopes.rbegin();scope!=scopes.rend();++scope)
   if(auto found=scope->find(name);found!=scope->end())return found->second;
  throw std::runtime_error("internal codegen: missing symbol "+name);
 }
 void retain(const std::string& name){definitions<<pad()<<"on1x_env_retain("<<name<<".environment);\n";}
 void finish(std::size_t keep=0){
  for(std::size_t i=temporary_functions.size();i>keep;--i)
   definitions<<pad()<<"on1x_env_release("<<temporary_functions[i-1]<<".environment);\n";
  temporary_functions.resize(keep);
 }
 std::string temp(const Type& type,const std::string& rhs,bool borrowed=false){
  if(type==Type::Void){definitions<<pad()<<rhs<<";\n";return {};}
  auto name=unique("tmp");definitions<<pad()<<ctype(type)<<" "<<name<<" = "<<rhs<<";\n";
  if(type.is_function()){
   if(borrowed)retain(name);
   temporary_functions.push_back(name);
  }
  return name;
 }
 std::string global_value(const Expr& expression){
  auto found=adapters.find(expression.text);
  if(found==adapters.end())found=adapters.emplace(expression.text,reserve("on1x_adapter_"+expression.text)).first;
  return temp(expression.type,"("+ctype(expression.type)+"){"+found->second+", NULL}");
 }
 std::string expr(const Expr& e){
  switch(e.kind){
  case Expr::Kind::Integer:return e.text=="-9223372036854775808"?"(-INT64_C(9223372036854775807)-INT64_C(1))":"INT64_C("+e.text+")";
  case Expr::Kind::Boolean:return e.text;
  case Expr::Kind::String:return c_string(e.text);
  case Expr::Kind::Variable:
   if(e.reference==Expr::Reference::Function)return global_value(e);
   if(e.reference==Expr::Reference::Local)return e.type.is_function()?temp(e.type,ref(e.text),true):ref(e.text);
   throw std::runtime_error("internal codegen: non-value name "+e.text);
  case Expr::Kind::Lambda:{
   const auto& lambda=lambdas.at(lambda_index.at(&e));std::string call=lambda.make+"(";
   for(std::size_t i=0;i<e.captures.size();++i){if(i)call+=", ";call+=ref(e.captures[i].name);}
   return temp(e.type,call+")");
  }
  case Expr::Kind::Unary:{
   auto value=expr(*e.children[0]);return temp(e.type,e.text=="-"?"on1x_neg_i64("+value+")":"(!"+value+")");
  }
  case Expr::Kind::Binary:{
   auto mark=temporary_functions.size();
   auto left=expr(*e.children[0]);const auto& op=e.text;
   if(op=="&&"||op=="||"){
    auto result=unique("logic");definitions<<pad()<<"bool "<<result<<" = "<<left<<";\n";
    // Left-operand temporaries must not be mixed with conditionally initialized
    // right-operand temporaries at a later lexical cleanup point.
    finish(mark);definitions<<pad()<<"if ("<<(op=="&&"?result:"!"+result)<<") {\n";++indent;
    auto right=expr(*e.children[1]);definitions<<pad()<<result<<" = "<<right<<";\n";finish(mark);
    --indent;definitions<<pad()<<"}\n";return result;
   }
   auto right=expr(*e.children[1]);
   if(op=="+"||op=="-"||op=="*"||op=="/"||op=="%"){
    std::string method=op=="+"?"add":op=="-"?"sub":op=="*"?"mul":op=="/"?"div":"mod";
    return temp(e.type,"on1x_"+method+"_i64("+left+", "+right+")");
   }
   if((op=="=="||op=="!=")&&e.children[0]->type==Type::String)
    return temp(e.type,"(strcmp("+left+", "+right+") "+op+" 0)");
   return temp(e.type,"("+left+" "+op+" "+right+")");
  }
  case Expr::Kind::Call:{
   const auto& callee=*e.callee;bool builtin=callee.reference==Expr::Reference::Intrinsic;
   bool direct=builtin||callee.reference==Expr::Reference::Extern;
   auto target=direct?callee.text:expr(callee);std::vector<std::string> args;
   for(const auto& child:e.children){
    auto value=expr(*child);args.push_back(builtin||child->type.is_function()?value:temp(child->type,value));
   }
   if(builtin){
    const auto& name=callee.text;
    if(name=="println"){
     auto type=e.children[0]->type;target="on1x_print_"+std::string(type==Type::I64?"i64":type==Type::Bool?"bool":"string");
    }else if(name=="assert")target="on1x_assert";
    else if(name=="json_parse_or_die")target="on1x_json_parse_or_die";
    else if(name=="json_get_or_die")target="on1x_json_get_or_die";
    else if(name=="json_size")target="on1x_json_size";
    else if(name=="json_number")target="on1x_json_number";
    else if(name=="json_print")target="on1x_json_print";
   }
   std::string call=direct?target+"(":target+".invoke("+target+".environment";
   for(std::size_t i=0;i<args.size();++i){if(i||!direct)call+=", ";call+=args[i];}
   return temp(e.type,call+")");
  }
  }
  throw std::runtime_error("internal bad expression");
 }
 bool body(const std::vector<Stmt>& statements){
  push();bool terminated=false;for(const auto& statement:statements)terminated=stmt(statement);
  pop(!terminated);return terminated;
 }
 bool stmt(const Stmt& s){
  switch(s.kind){
  case Stmt::Kind::Let:{
   auto value=expr(*s.value),name=unique(s.name);scopes.back()[s.name]=name;
   definitions<<pad()<<ctype(s.value->type)<<" "<<name<<" = "<<value<<";\n";
   definitions<<pad()<<"(void)"<<name<<";\n";
   if(s.value->type.is_function())retain(name);
   if(s.value->type==Type::Json||s.value->type.is_function())owners.back().push_back({s.value->type,name});
   finish();return false;
  }
  case Stmt::Kind::Assign:{
   auto value=expr(*s.value),name=ref(s.name);
   if(s.value->type.is_function()){retain(value);definitions<<pad()<<"on1x_env_release("<<name<<".environment);\n";}
   definitions<<pad()<<name<<" = "<<value<<";\n";finish();return false;
  }
  case Stmt::Kind::Return:{
   if(s.value){
    auto value=expr(*s.value);
    if(s.value->type!=Type::Void){
     auto result=unique("return");definitions<<pad()<<ctype(s.value->type)<<" "<<result<<" = "<<value<<";\n";
     if(s.value->type.is_function())retain(result);
     finish();drops();definitions<<pad()<<"return "<<result<<";\n";
    }else{finish();drops();definitions<<pad()<<"return;\n";}
   }else{finish();drops();definitions<<pad()<<"return;\n";}
   return true;
  }
  case Stmt::Kind::Expr:{
   auto value=expr(*s.value);if(!value.empty())definitions<<pad()<<"(void)"<<value<<";\n";
   finish();return false;
  }
  case Stmt::Kind::If:{
   auto condition=expr(*s.value);finish();definitions<<pad()<<"if ("<<condition<<") {\n";
   bool yes=body(s.body),no=false;definitions<<pad()<<"}";
   if(!s.otherwise.empty()){definitions<<" else {\n";no=body(s.otherwise);definitions<<pad()<<"}";}
   definitions<<"\n";return yes&&no;
  }
  case Stmt::Kind::While:{
   definitions<<pad()<<"while (true) {\n";++indent;
   auto condition=expr(*s.value);finish();definitions<<pad()<<"if (!("<<condition<<")) break;\n";
   definitions<<pad()<<"{\n";body(s.body);definitions<<pad()<<"}\n";
   --indent;definitions<<pad()<<"}\n";return false;
  }
  case Stmt::Kind::Block:{
   definitions<<pad()<<"{\n";bool terminated=body(s.body);definitions<<pad()<<"}\n";return terminated;
  }
  }
  throw std::runtime_error("internal bad statement");
 }
 void parameter_types(std::ostream& output,const std::vector<Param>& parameters,bool environment=false){
  if(environment)output<<"on1x_env *";
  if(parameters.empty()&&!environment)output<<"void";
  for(std::size_t i=0;i<parameters.size();++i){if(i||environment)output<<", ";output<<ctype(parameters[i].type);}
 }
 void capture_types(std::ostream& output,const Expr& expression,bool names=false){
  if(expression.captures.empty())output<<"void";
  for(std::size_t i=0;i<expression.captures.size();++i){
   if(i)output<<", ";
   output<<ctype(expression.captures[i].type);
   if(names)output<<" on1x_capture_"<<i;
  }
 }
 void lambda_support(const Lambda& lambda){
  const auto& e=*lambda.expression;
  prototypes<<"static "<<ctype(e.result)<<" "<<lambda.invoke<<"(";parameter_types(prototypes,e.parameters,true);prototypes<<");\n";
  prototypes<<"static "<<ctype(e.type)<<" "<<lambda.make<<"(";capture_types(prototypes,e);prototypes<<");\n";
  if(!e.captures.empty()){
   helpers<<"struct "<<lambda.environment<<" {\n  on1x_env header;\n";
   for(std::size_t i=0;i<e.captures.size();++i)helpers<<"  "<<ctype(e.captures[i].type)<<" capture_"<<i<<";\n";
   helpers<<"};\n_Static_assert(offsetof(struct "<<lambda.environment<<", header) == 0, \"closure header must be first\");\n";
   helpers<<"static void "<<lambda.destroy<<"(on1x_env *on1x_context) {\n"
    <<"  struct "<<lambda.environment<<" *on1x_capture = (struct "<<lambda.environment<<" *)on1x_context;\n"
    <<"  (void)on1x_capture;\n";
   for(std::size_t i=e.captures.size();i>0;--i)if(e.captures[i-1].type.is_function())
    helpers<<"  on1x_env_release(on1x_capture->capture_"<<i-1<<".environment);\n";
   helpers<<"}\n";
  }
  helpers<<"static "<<ctype(e.type)<<" "<<lambda.make<<"(";capture_types(helpers,e,true);helpers<<") {\n";
  if(e.captures.empty())helpers<<"  return ("<<ctype(e.type)<<"){"<<lambda.invoke<<", NULL};\n";
  else{
   helpers<<"  struct "<<lambda.environment<<" *on1x_capture = (struct "<<lambda.environment<<" *)\n"
    <<"    on1x_env_alloc(sizeof(struct "<<lambda.environment<<"), "<<lambda.destroy<<");\n";
   for(std::size_t i=0;i<e.captures.size();++i){
    helpers<<"  on1x_capture->capture_"<<i<<" = on1x_capture_"<<i<<";\n";
    if(e.captures[i].type.is_function())helpers<<"  on1x_env_retain(on1x_capture->capture_"<<i<<".environment);\n";
   }
   helpers<<"  return ("<<ctype(e.type)<<"){"<<lambda.invoke<<", &on1x_capture->header};\n";
  }
  helpers<<"}\n\n";
 }
 void definition(const std::string& name,const Type& result,const std::vector<Param>& parameters,
                 const std::vector<Stmt>& statements,const Lambda* lambda=nullptr){
  scopes.clear();owners.clear();temporary_functions.clear();indent=0;
  if(lambda){scopes.emplace_back();owners.emplace_back();}
  push();definitions<<(lambda?"static ":"")<<ctype(result)<<" "<<name<<"(";
  std::string context;
  if(lambda){context=unique("context");definitions<<"on1x_env *"<<context;}
  if(parameters.empty()&&!lambda)definitions<<"void";
  for(std::size_t i=0;i<parameters.size();++i){
   if(i||lambda)definitions<<", ";
   auto symbol=unique(parameters[i].name);
   scopes.back()[parameters[i].name]=symbol;definitions<<ctype(parameters[i].type)<<" "<<symbol;
  }
  definitions<<") {\n";
  if(lambda){
   definitions<<pad()<<"(void)"<<context<<";\n";
   if(!lambda->expression->captures.empty()){
    auto capture=unique("captures");definitions<<pad()<<"struct "<<lambda->environment<<" *"<<capture<<" = (struct "<<lambda->environment<<" *)"<<context<<";\n";
    for(std::size_t i=0;i<lambda->expression->captures.size();++i)
     scopes.front()[lambda->expression->captures[i].name]=capture+"->capture_"+std::to_string(i);
   }
  }
  for(const auto& parameter:parameters)definitions<<pad()<<"(void)"<<ref(parameter.name)<<";\n";
  bool terminated=false;for(const auto& statement:statements)terminated=stmt(statement);
  pop(!terminated);if(result==Type::Void&&!terminated)definitions<<"  return;\n";
  definitions<<"}\n\n";scopes.clear();owners.clear();
 }
 void adapter(const Function& f,const std::string& name){
  prototypes<<"static "<<ctype(f.result)<<" "<<name<<"(";parameter_types(prototypes,f.params,true);prototypes<<");\n";
  helpers<<"static "<<ctype(f.result)<<" "<<name<<"(on1x_env *on1x_context";
  for(std::size_t i=0;i<f.params.size();++i)helpers<<", "<<ctype(f.params[i].type)<<" on1x_arg_"<<i;
  helpers<<") {\n  (void)on1x_context;\n  ";if(f.result!=Type::Void)helpers<<"return ";
  helpers<<functions.at(f.name)<<"(";
  for(std::size_t i=0;i<f.params.size();++i){if(i)helpers<<", ";helpers<<"on1x_arg_"<<i;}
  helpers<<");\n";if(f.result==Type::Void)helpers<<"  return;\n";helpers<<"}\n\n";
 }
public:
 explicit Generator(const Unit& source):unit(source){
  for(const auto& f:unit.functions)if(f.external)identifiers.insert(f.name);
  for(const auto& f:unit.functions)functions.emplace(f.name,f.external?f.name:reserve("on1x_fn_"+f.name));
  for(const auto& f:unit.functions)collect(f.body);
 }
 std::string run(){
  for(const auto& f:unit.functions){
   prototypes<<(f.external?"extern ":"")<<ctype(f.result)<<" "<<functions.at(f.name)<<"(";
   parameter_types(prototypes,f.params);prototypes<<");\n";
  }
  for(const auto& lambda:lambdas)lambda_support(lambda);
  for(const auto& f:unit.functions)if(!f.external)definition(functions.at(f.name),f.result,f.params,f.body);
  for(const auto& lambda:lambdas){const auto& e=*lambda.expression;definition(lambda.invoke,e.result,e.parameters,e.body,&lambda);}
  for(const auto& f:unit.functions)if(auto found=adapters.find(f.name);found!=adapters.end())adapter(f,found->second);
  const auto& main=*std::find_if(unit.functions.begin(),unit.functions.end(),[](const auto& f){return f.name=="main";});
  if(main.result==Type::Void)definitions<<"int main(void) { "<<functions.at("main")<<"(); return 0; }\n";
  else definitions<<"int main(void) { return (int)"<<functions.at("main")<<"(); }\n";
  return "/* Generated by on1xc. Do not edit. */\n#include <on1x_runtime.h>\n#include <stdint.h>\n#include <stdbool.h>\n#include <string.h>\n"
   "#if !defined(ON1X_CLOSURE_ABI_VERSION) || ON1X_CLOSURE_ABI_VERSION != 1\n#error \"incompatible On1x closure runtime ABI\"\n#endif\n\n"
   +types.str()+"\n"+prototypes.str()+"\n"+helpers.str()+definitions.str();
 }
};
}
std::string emit_c(const Unit& unit,const std::string& source_name){(void)source_name;return Generator(unit).run();}
}
