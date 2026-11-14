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
 std::ostringstream types,prototypes,helpers,value_helpers,definitions;
 std::map<Type,std::string> function_types;
 std::map<Type,std::string> tuple_types,tuple_retains,tuple_releases;
 std::vector<std::pair<Type,bool>> ownership_helpers;
 std::set<std::string> identifiers;
 std::map<std::string,std::string> functions,adapters;
 std::map<std::string,const Function*> externs;
 std::vector<Lambda> lambdas;
 std::unordered_map<const Expr*,std::size_t> lambda_index;
 std::vector<std::unordered_map<std::string,std::string>> scopes;
 std::vector<std::vector<Owner>> owners;
 // Function values and tuples containing them own temporary references, released
 // at the end of the full expression rather than at an enclosing loop exit.
 std::vector<Owner> temporary_values;
 std::uint64_t serial=0,type_serial=0,helper_serial=0;
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
   case Type::Tuple:case Type::Forall:{
   if(auto found=tuple_types.find(type);found!=tuple_types.end())return found->second;
    std::vector<std::string> elements;for(const auto& element:fields(type))elements.push_back(ctype(element));
   auto name=reserve("on1x_tuple_type_"+std::to_string(++type_serial));tuple_types.emplace(type,name);
    types<<"typedef struct "<<name<<" {\n";
    if(elements.empty())types<<"  unsigned char unused;\n";
   for(std::size_t i=0;i<elements.size();++i)types<<"  "<<elements[i]<<" element_"<<i<<";\n";
   types<<"} "<<name<<";\n";return name;
  }
   default:throw std::runtime_error("unknown backend type");
   }
  }
 const std::vector<Type>& fields(const Type& type)const{
  if(type.is_tuple())return type.elements();
  auto found=unit.forall_layouts.find(type);static const std::vector<Type> empty;
  return found==unit.forall_layouts.end()?empty:found->second;
 }
 std::string ownership_helper(const Type& type,bool retaining){
  auto& table=retaining?tuple_retains:tuple_releases;
  if(auto found=table.find(type);found!=table.end())return found->second;
  auto name=reserve(std::string("on1x_tuple_")+(retaining?"retain_":"release_")+std::to_string(++helper_serial));
  table.emplace(type,name);ownership_helpers.emplace_back(type,retaining);
  prototypes<<"static void "<<name<<"(const "<<ctype(type)<<" *);\n";return name;
 }
 void adjust(std::ostream& output,const Type& type,const std::string& value,bool retaining,const std::string& prefix){
  if(!type.has_functions())return;
  if(type.is_function())output<<prefix<<(retaining?"on1x_env_retain(":"on1x_env_release(")<<value<<".environment);\n";
  else output<<prefix<<ownership_helper(type,retaining)<<"(&("<<value<<"));\n";
 }
 void emit_ownership_helpers(){
  // Generate only requested operations. Child requests may append to the list;
  // all prototypes precede all definitions, so dependency order remains valid.
  for(std::size_t i=0;i<ownership_helpers.size();++i){
   auto [type,retaining]=ownership_helpers[i];auto name=ownership_helper(type,retaining);
   value_helpers<<"static void "<<name<<"(const "<<ctype(type)<<" *on1x_value) {\n";
    value_helpers<<"  (void)on1x_value;\n";
    for(std::size_t j=0;j<fields(type).size();++j){
     auto index=retaining?j:fields(type).size()-1-j;
     adjust(value_helpers,fields(type)[index],"on1x_value->element_"+std::to_string(index),retaining,"  ");
   }
   value_helpers<<"}\n\n";
  }
 }
 void collect(const Expr& expression){
  if(expression.prototype)return;
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
   if(statement.prototype)continue;
   if(statement.value&&statement.kind!=Stmt::Kind::Template)collect(*statement.value);
   collect(statement.body);collect(statement.otherwise);
  }
 }
 void push(){scopes.emplace_back();owners.emplace_back();++indent;}
 void release(const Owner& owner){
  if(owner.type==Type::Json)definitions<<pad()<<"on1x_json_drop("<<owner.name<<");\n";
  else adjust(definitions,owner.type,owner.name,false,pad());
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
 void retain(const Type& type,const std::string& name){adjust(definitions,type,name,true,pad());}
 void finish(std::size_t keep=0){
  for(std::size_t i=temporary_values.size();i>keep;--i)release(temporary_values[i-1]);
  temporary_values.resize(keep);
 }
 std::string temp(const Type& type,const std::string& rhs,bool borrowed=false){
  if(type==Type::Void){definitions<<pad()<<rhs<<";\n";return {};}
  auto name=unique("tmp");definitions<<pad()<<ctype(type)<<" "<<name<<" = "<<rhs<<";\n";
  if(type.has_functions()){
   if(borrowed)retain(type,name);
   temporary_values.push_back({type,name});
  }
  return name;
 }
 std::string global_value(const Expr& expression){
  auto found=adapters.find(expression.text);
   if(found==adapters.end())found=adapters.emplace(expression.text,reserve("on1x_adapter_"+(expression.text.starts_with("@")?expression.text.substr(1):expression.text))).first;
  return temp(expression.type,"("+ctype(expression.type)+"){"+found->second+", NULL}");
 }
 void flatten(const Pattern& pattern,const Type& type,const std::string& value,std::vector<std::string>& args){
  if(!pattern.is_tuple()){args.push_back(value);return;}
  auto root=unique("extern_pattern");
  definitions<<pad()<<"const "<<ctype(type)<<" *"<<root<<" = &("<<value<<");\n";
  for(std::size_t i=0;i<pattern.elements.size();++i)
   flatten(pattern.elements[i],type.elements()[i],root+"->element_"+std::to_string(i),args);
 }
 std::string expr(const Expr& e){
   switch(e.kind){
   case Expr::Kind::PolyPack:{
    std::vector<std::string> elements;for(const auto& child:e.children)elements.push_back(expr(*child));
    std::string value="("+ctype(e.type)+"){";
    if(elements.empty())value+="0";
    for(std::size_t i=0;i<elements.size();++i){if(i)value+=", ";value+=elements[i];}
    return temp(e.type,value+"}",true);
   }
   case Expr::Kind::PolyProjection:{
    auto value=expr(*e.children[0]);return temp(e.type,value+".element_"+std::to_string(e.projection),true);
   }
  case Expr::Kind::Integer:return e.text=="-9223372036854775808"?"(-INT64_C(9223372036854775807)-INT64_C(1))":"INT64_C("+e.text+")";
  case Expr::Kind::Boolean:return e.text;
  case Expr::Kind::String:return c_string(e.text);
  case Expr::Kind::Variable:
   if(e.reference==Expr::Reference::Function)return global_value(e);
   if(e.reference==Expr::Reference::Local)return e.type.has_functions()?temp(e.type,ref(e.text),true):ref(e.text);
   throw std::runtime_error("internal codegen: non-value name "+e.text);
  case Expr::Kind::Lambda:{
   const auto& lambda=lambdas.at(lambda_index.at(&e));std::string call=lambda.make+"(";
   for(std::size_t i=0;i<e.captures.size();++i){if(i)call+=", ";call+=ref(e.captures[i].name);}
   return temp(e.type,call+")");
  }
  case Expr::Kind::Tuple:{
   std::vector<std::string> elements;
   for(const auto& child:e.children){
    auto value=expr(*child);elements.push_back(child->type.has_functions()?value:temp(child->type,value));
   }
   std::string value="("+ctype(e.type)+"){";
   for(std::size_t i=0;i<elements.size();++i){if(i)value+=", ";value+=elements[i];}
   // Aggregate initialization copies fields; acquire references before any
   // child temporary is released at the enclosing full-expression boundary.
   return temp(e.type,value+"}",true);
  }
  case Expr::Kind::Unary:{
   auto value=expr(*e.children[0]);return temp(e.type,e.text=="-"?"on1x_neg_i64("+value+")":"(!"+value+")");
  }
  case Expr::Kind::Binary:{
   auto mark=temporary_values.size();
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
   for(std::size_t i=0;i<e.children.size();++i){
    const auto& child=e.children[i];auto value=expr(*child);
    if(!builtin&&!child->type.has_functions())value=temp(child->type,value);
    if(callee.reference==Expr::Reference::Extern&&externs.at(callee.text)->params[i].pattern)
     flatten(*externs.at(callee.text)->params[i].pattern,child->type,value,args);
    else args.push_back(value);
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
 void bind_name(const std::string& binding,const Type& type,const std::string& value){
  auto name=unique(binding.starts_with("@")?"specialization":binding);scopes.back()[binding]=name;
  definitions<<pad()<<ctype(type)<<" "<<name<<" = "<<value<<";\n"<<pad()<<"(void)"<<name<<";\n";
  retain(type,name);
  if(type==Type::Json||type.has_functions())owners.back().push_back({type,name});
 }
 void bind_pattern(const Pattern& pattern,const Type& type,const std::string& value){
  if(!pattern.is_tuple()){bind_name(pattern.name,type,value);return;}
  // Keep projections linear in pattern size, instead of copying an arbitrarily
  // long common access path into every leaf of a deep/wide pattern.
  auto root=unique("pattern");
  definitions<<pad()<<"const "<<ctype(type)<<" *"<<root<<" = &("<<value<<");\n";
  for(std::size_t i=0;i<pattern.elements.size();++i)
   bind_pattern(pattern.elements[i],type.elements()[i],root+"->element_"+std::to_string(i));
 }
 bool stmt(const Stmt& s){
  if(s.prototype)return false;
  switch(s.kind){
  case Stmt::Kind::Template:{for(const auto& instance:s.body)stmt(instance);return false;}
  case Stmt::Kind::Let:{
   auto value=expr(*s.value);
   if(s.pattern)bind_pattern(*s.pattern,s.value->type,value);else bind_name(s.name,s.value->type,value);
   finish();return false;
  }
  case Stmt::Kind::Assign:{
   auto value=expr(*s.value),name=ref(s.name);
   // Snapshot even trivially copied values. Besides preserving assignment
   // sequencing, this avoids a C compiler self-assignment warning for x = x.
   if(!s.value->type.has_functions())value=temp(s.value->type,value);
   if(s.value->type.has_functions()){retain(s.value->type,value);adjust(definitions,s.value->type,name,false,pad());}
   definitions<<pad()<<name<<" = "<<value<<";\n";finish();return false;
  }
  case Stmt::Kind::Return:{
   if(s.value){
    auto value=expr(*s.value);
    if(s.value->type!=Type::Void){
     auto result=unique("return");definitions<<pad()<<ctype(s.value->type)<<" "<<result<<" = "<<value<<";\n";
     retain(s.value->type,result);
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
   for(std::size_t i=e.captures.size();i>0;--i)
    adjust(helpers,e.captures[i-1].type,"on1x_capture->capture_"+std::to_string(i-1),false,"  ");
   helpers<<"}\n";
  }
  helpers<<"static "<<ctype(e.type)<<" "<<lambda.make<<"(";capture_types(helpers,e,true);helpers<<") {\n";
  if(e.captures.empty())helpers<<"  return ("<<ctype(e.type)<<"){"<<lambda.invoke<<", NULL};\n";
  else{
   helpers<<"  struct "<<lambda.environment<<" *on1x_capture = (struct "<<lambda.environment<<" *)\n"
    <<"    on1x_env_alloc(sizeof(struct "<<lambda.environment<<"), "<<lambda.destroy<<");\n";
   for(std::size_t i=0;i<e.captures.size();++i){
    helpers<<"  on1x_capture->capture_"<<i<<" = on1x_capture_"<<i<<";\n";
    adjust(helpers,e.captures[i].type,"on1x_capture->capture_"+std::to_string(i),true,"  ");
   }
   helpers<<"  return ("<<ctype(e.type)<<"){"<<lambda.invoke<<", &on1x_capture->header};\n";
  }
  helpers<<"}\n\n";
 }
 void definition(const std::string& name,const Type& result,const std::vector<Param>& parameters,
                 const std::vector<Stmt>& statements,const Lambda* lambda=nullptr){
  scopes.clear();owners.clear();temporary_values.clear();indent=0;
  if(lambda){scopes.emplace_back();owners.emplace_back();}
  push();definitions<<(lambda?"static ":"")<<ctype(result)<<" "<<name<<"(";
  std::string context;
  if(lambda){context=unique("context");definitions<<"on1x_env *"<<context;}
  if(parameters.empty()&&!lambda)definitions<<"void";
  for(std::size_t i=0;i<parameters.size();++i){
   if(i||lambda)definitions<<", ";
   auto symbol=unique(parameters[i].pattern?"pattern_argument":parameters[i].name);
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
  for(const auto& parameter:parameters)if(parameter.pattern)
   bind_pattern(*parameter.pattern,parameter.type,ref(parameter.name));
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
  for(const auto& f:unit.functions)if(f.external)externs.emplace(f.name,&f);
   for(const auto& f:unit.functions)functions.emplace(f.name,f.external?f.name:reserve("on1x_fn_"+(f.name.starts_with("@")?f.name.substr(1):f.name)));
   for(const auto& f:unit.functions)if(!f.prototype)collect(f.body);
 }
 std::string run(){
   for(const auto& f:unit.functions){
    if(f.prototype)continue;
   prototypes<<(f.external?"extern ":"")<<ctype(f.result)<<" "<<functions.at(f.name)<<"(";
   if(f.external){
    auto flattened=extern_parameter_types(f);if(flattened.empty())prototypes<<"void";
    for(std::size_t i=0;i<flattened.size();++i){if(i)prototypes<<", ";prototypes<<ctype(flattened[i]);}
   }else parameter_types(prototypes,f.params);
   prototypes<<");\n";
  }
  for(const auto& lambda:lambdas)lambda_support(lambda);
   for(const auto& f:unit.functions)if(!f.external&&!f.prototype)definition(functions.at(f.name),f.result,f.params,f.body);
  for(const auto& lambda:lambdas){const auto& e=*lambda.expression;definition(lambda.invoke,e.result,e.parameters,e.body,&lambda);}
  for(const auto& f:unit.functions)if(auto found=adapters.find(f.name);found!=adapters.end())adapter(f,found->second);
  emit_ownership_helpers();
  const auto& main=*std::find_if(unit.functions.begin(),unit.functions.end(),[](const auto& f){return f.name=="main";});
  if(main.result==Type::Void)definitions<<"int main(void) { "<<functions.at("main")<<"(); return 0; }\n";
  else definitions<<"int main(void) { return (int)"<<functions.at("main")<<"(); }\n";
  return "/* Generated by on1xc. Do not edit. */\n#include <on1x_runtime.h>\n#include <stdint.h>\n#include <stdbool.h>\n#include <string.h>\n"
   "#if !defined(ON1X_CLOSURE_ABI_VERSION) || ON1X_CLOSURE_ABI_VERSION != 1\n#error \"incompatible On1x closure runtime ABI\"\n#endif\n\n"
   +types.str()+"\n"+prototypes.str()+"\n"+value_helpers.str()+helpers.str()+definitions.str();
 }
};
}
std::string emit_c(const Unit& unit,const std::string& source_name){(void)source_name;return Generator(unit).run();}
}
