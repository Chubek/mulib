#include <on1x/ast.hpp>
#include <on1x/meta.hpp>
#include <on1x/primitives.hpp>
#include <on1x/numeric.hpp>
#include <on1x/nominal.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/concurrency.hpp>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <bit>

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
 struct Lambda { const Expr* expression; std::string invoke,make,environment,destroy; bool deferred; };
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
  struct LoopTarget {std::size_t id,keep;std::string done,latch;};
  std::vector<LoopTarget> loop_targets;
  std::set<std::size_t> break_targets,continue_targets;
  std::vector<LoopTarget> region_targets;
  std::set<std::size_t> region_exit_targets;
 // Function values and tuples containing them own temporary references, released
 // at the end of the full expression rather than at an enclosing loop exit.
  std::vector<Owner> temporary_values;
  std::string defer_stack;
 std::uint64_t serial=0,type_serial=0,helper_serial=0;
  int indent=0;
   bool uses_f64=false,uses_f32=false,uses_unsigned=false;
    bool uses_value_data=false,uses_defer=false;
    bool uses_csp=false;
   std::string location(const Location& at)const{return c_string(meta::source_literal(at.file))+", "+std::to_string(at.line)+", "+std::to_string(at.column);}

 std::string reserve(const std::string& preferred){
  std::string name=preferred;std::size_t suffix=0;
  while(!identifiers.insert(name).second)name=preferred+"_"+std::to_string(++suffix);
  return name;
 }
 std::string unique(const std::string& name){return reserve("on1x_v"+std::to_string(++serial)+"_"+name);}
 std::string pad()const{return std::string(indent*2,' ');}
  std::string ctype(const Type& type){
   if(type.is_data()){
    uses_value_data=true;
    if(auto found=tuple_types.find(type);found!=tuple_types.end())return found->second;
    std::vector<std::string> elements;for(const auto& field:type.elements())elements.push_back(ctype(field));
    std::string backing;if(type.kind()==Type::Slice)backing=ctype(Type::data(Type::Array,type.elements(),{},type.extent()));
    auto name=reserve("on1x_data_type_"+std::to_string(++type_serial));tuple_types.emplace(type,name);
    types<<"typedef struct "<<name<<" {\n";
    if(type.kind()==Type::Array)types<<"  "<<elements[0]<<" elements["<<std::max<std::size_t>(1,type.extent())<<"];\n";
    else if(type.kind()==Type::Slice)types<<"  "<<backing<<" backing;\n  int64_t start, length;\n";
    else{
     if(type.kind()==Type::Sum)types<<"  int64_t tag;\n  union {\n";
     if(elements.empty())types<<"  unsigned char unused;\n";
     for(std::size_t i=0;i<elements.size();++i)types<<"  "<<elements[i]<<" element_"<<i<<";\n";
     if(type.kind()==Type::Sum)types<<"  } payload;\n";
    }
    types<<"} "<<name<<";\n";return name;
   }
    switch(type.kind()){
     case Type::Fiber:return "on1x_csp_task *";
     case Type::ExternalFiber:return "on1x_csp_process *";
     case Type::Channel:case Type::Sender:case Type::Receiver:case Type::ByteSender:case Type::ByteReceiver:return "on1x_csp_channel *";
    case Type::CspGroup:return "on1x_csp_group *";
   case Type::I64:return "int64_t";case Type::Bool:return "bool";
    case Type::F64:uses_f64=true;return "double";
    case Type::F32:uses_f64=true;uses_f32=true;return "float";
    case Type::U8:case Type::U16:case Type::U32:case Type::U64:
     uses_unsigned=true;return "uint"+type_name(type).substr(1)+"_t";
  case Type::String:return "const char *";case Type::Void:return "void";
   case Type::Json:return "on1x_json *";case Type::JsonView:return "const on1x_json *";
   case Type::SExpr:return "on1x_sexpr *";case Type::Buffer:return "on1x_buffer *";
   case Type::Code:return ctype(Type::function({},type.code_result()));
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
   if(type.is_reference())output<<prefix<<(retaining?"on1x_data_retain(":"on1x_data_release(")<<value<<");\n";
   else if(type.is_function()||type.is_code())output<<prefix<<(retaining?"on1x_env_retain(":"on1x_env_release(")<<value<<".environment);\n";
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
 void collect(const Expr& expression,bool deferred=false){
  if(expression.prototype)return;
   if(expression.kind==Expr::Kind::Lambda){
   auto index=lambdas.size();auto suffix=std::to_string(index+1);
   lambda_index.emplace(&expression,index);
   lambdas.push_back({&expression,reserve("on1x_lambda_"+suffix),reserve("on1x_make_lambda_"+suffix),
                      reserve("on1x_lambda_env_"+suffix),reserve("on1x_drop_lambda_"+suffix),deferred});
    collect(expression.body);
   }
   if(expression.kind==Expr::Kind::Region)collect(expression.body);
  if(expression.callee)collect(*expression.callee);
  for(const auto& child:expression.children)collect(*child);
 }
 void collect(const std::vector<Stmt>& body){
  for(const auto& statement:body){
    if(statement.prototype)continue;
    if(statement.kind==Stmt::Kind::Break)break_targets.insert(statement.control_target);
    if(statement.kind==Stmt::Kind::Continue)continue_targets.insert(statement.control_target);
    if(statement.kind==Stmt::Kind::Leave)region_exit_targets.insert(statement.control_target);
   if(statement.value&&statement.kind!=Stmt::Kind::Template)collect(*statement.value,statement.kind==Stmt::Kind::PushDefer);
   collect(statement.body);collect(statement.otherwise);
  }
 }
 void push(){scopes.emplace_back();owners.emplace_back();++indent;}
  void release(const Owner& owner){
   if(owner.type==Type::CspGroup)definitions<<pad()<<"on1x_csp_group_finish("<<owner.name<<");\n";
   else if(owner.type==Type::Json)definitions<<pad()<<"on1x_json_drop("<<owner.name<<");\n";
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
  bool has_defer(const Expr& e)const{
   if(e.prototype||e.kind==Expr::Kind::Lambda)return false;
   if(e.callee&&has_defer(*e.callee))return true;
   for(const auto& c:e.children)if(has_defer(*c))return true;
   return has_defer(e.body);
  }
  bool has_defer(const std::vector<Stmt>& b)const{
   for(const auto& s:b)if(!s.prototype&&(s.kind==Stmt::Kind::PushDefer||
      (s.value&&s.kind!=Stmt::Kind::Template&&has_defer(*s.value))||has_defer(s.body)||has_defer(s.otherwise)))return true;
   return false;
  }
   void run_defers(){if(!defer_stack.empty())definitions<<pad()<<"on1x_defer_run(&"<<defer_stack<<");\n";}
   void leave_call(){if(uses_csp)definitions<<pad()<<"on1x_csp_call_leave();\n";}
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
       case Expr::Kind::SourceCsp:throw Error(e.where,"unlowered concurrency operation");
        case Expr::Kind::Csp:{
         const auto& op=e.text;auto at=location(e.where);
         if(op=="poll"){
          std::size_t n=e.children.size();bool fallback=e.children.back()->kind==Expr::Kind::Boolean;
          if(fallback)--n;
          auto offers=unique("poll_offers");definitions<<pad()<<"on1x_csp_offer "<<offers<<"["<<n<<"] = {0};\n";
          for(std::size_t i=0;i<n;++i){const auto& offer=*e.children[i];
           auto endpoint=expr(*offer.children[0]);
           definitions<<pad()<<offers<<"["<<i<<"].channel = "<<endpoint<<";\n";
           definitions<<pad()<<offers<<"["<<i<<"].operation = "<<(offer.text=="offer_send"?1:2)<<";\n";
           if(offer.text=="offer_send"){
            auto value=expr(*offer.children[1]);definitions<<pad()<<offers<<"["<<i<<"].payload = "<<value<<";\n";
           }
          }
          auto payload=unique("poll_payload"),selected=unique("poll_selected"),closed=unique("poll_closed");
          definitions<<pad()<<"int64_t "<<payload<<" = 0;\n"<<pad()<<"size_t "<<selected<<" = 0;\n"<<pad()<<"bool "<<closed<<" = false;\n";
          definitions<<pad()<<"on1x_csp_select("<<offers<<", "<<n<<", "<<(fallback?"true":"false")<<", &"<<payload<<", &"<<selected<<", &"<<closed<<", "<<at<<");\n";
          auto result=temp(e.type,"{0}");
          auto index=[&](const std::string& label){auto it=std::find(e.type.labels().begin(),e.type.labels().end(),label);
           if(it==e.type.labels().end())throw Error(e.where,"missing checked poll result variant");
           return static_cast<std::size_t>(it-e.type.labels().begin());};
          if(fallback)definitions<<pad()<<"if ("<<selected<<" == "<<n<<") "<<result<<".tag = "<<index("Default")<<";\n";
          for(std::size_t i=0;i<n;++i){
           auto success=index((e.children[i]->text=="offer_recv"?"Recv":"Sent")+std::to_string(i));
           auto failure=index("Closed"+std::to_string(i));
           definitions<<pad()<<"if ("<<selected<<" == "<<i<<") "<<result<<".tag = "<<closed<<" ? "<<failure<<" : "<<success<<";\n";
            if(e.children[i]->text=="offer_recv")definitions<<pad()<<"if ("<<selected<<" == "<<i<<" && !"<<closed<<") "<<result<<".payload.element_"<<success<<" = ("<<ctype(e.type.elements()[success])<<")"<<payload<<";\n";
          }
          return result;
         }
         if(op=="launch"){
          auto fn=expr(*e.children.at(0));
          return temp(e.type,"on1x_csp_launch("+fn+".invoke, "+fn+".environment, "+at+")");
         }
         if(op=="launch_exec"){
          auto args=unique("external_argv");definitions<<pad()<<"const char *"<<args<<"["<<(e.children.size()+1)<<"] = {";
          for(const auto& arg:e.children)definitions<<expr(*arg)<<", ";
          definitions<<"NULL};\n";
          return temp(e.type,"on1x_csp_launch_exec("+args+", "+std::to_string(e.children.size())+", "+at+")");
         }
         if(op=="launch_capture"){
          auto args=unique("shell_argv");definitions<<pad()<<"const char *"<<args<<"["<<(e.children.size()+1)<<"] = {";
          for(const auto& arg:e.children)definitions<<expr(*arg)<<", ";
          definitions<<"NULL};\n";
          auto result=temp(e.type,"{0}");
          definitions<<pad()<<"on1x_csp_launch_capture("<<args<<", "<<e.children.size()<<", &"<<result<<".element_0, &"<<result<<".element_1, &"<<result<<".element_2, &"<<result<<".element_3, "<<at<<");\n";
          return result;
         }
        if(op=="channel")return temp(e.type,"on1x_csp_channel_create("+expr(*e.children.at(2))+", "+at+")");
        auto handle=expr(*e.children.at(0));
        if(op=="split")return temp(e.type,"("+ctype(e.type)+"){"+handle+", "+handle+"}");
         if(op=="join")return temp(e.type,std::string(e.children[0]->type==Type::ExternalFiber?"on1x_csp_process_join(":"on1x_csp_join(")+handle+", "+at+")");
         if(op=="cancel")return temp(e.type,std::string(e.children[0]->type==Type::Fiber?"on1x_csp_cancel_ready(":"on1x_csp_process_cancel(")+handle+", "+at+")");
         if(op=="close")return temp(e.type,std::string(e.children[0]->type==Type::ByteSender?"on1x_csp_stream_close(":"on1x_csp_close(")+handle+", "+at+")");
         if(op=="send"){
          auto payload=expr(*e.children.at(1));
          auto status=temp(Type::I64,std::string(e.children[0]->type==Type::ByteSender?"on1x_csp_stream_send(":"on1x_csp_send(")+handle+", "+payload+", "+at+")");
         auto result=temp(e.type,"{0}");
         definitions<<pad()<<result<<".tag = "<<status<<" == 0 ? 1 : 0;\n"
                    <<pad()<<"if ("<<status<<" != 0) "<<result<<".payload.element_0 = "<<status<<";\n";
         return result;
        }
         if(op=="recv"){
          auto byte=e.children[0]->type==Type::ByteReceiver;
          auto payload=temp(byte?Type::U8:Type::I64,"0");
          auto status=temp(Type::Bool,std::string(byte?"on1x_csp_stream_receive(":"on1x_csp_receive(")+handle+", &"+payload+", "+at+")");
         auto result=temp(e.type,"{0}");
         definitions<<pad()<<result<<".tag = "<<status<<" ? 1 : 0;\n"
                    <<pad()<<"if ("<<status<<") "<<result<<".payload.element_1 = "<<payload<<";\n";
         return result;
        }
        throw Error(e.where,"unknown canonical concurrency operation");
       }
      case Expr::Kind::Propagate:throw Error(e.where,"unlowered propagation");
     case Expr::Kind::Unit:return {};
      case Expr::Kind::PatternMatch:case Expr::Kind::Case:case Expr::Kind::Loop:throw Error(e.where,"surface value construct must be canonicalized");
     case Expr::Kind::Region:{
      auto output=e.type==Type::Void?std::string{}:unique("region_value");if(!output.empty())definitions<<pad()<<ctype(e.type)<<" "<<output<<" = {0};\n";
      auto outer=std::move(temporary_values);temporary_values.clear();definitions<<pad()<<"{\n";push();owners.back()=outer;definitions<<pad()<<"{\n";push();
      bool terminated=false;for(const auto& s:e.body)terminated=stmt(s);
      if(!terminated){auto result=expr(*e.children.at(0));if(!output.empty()){definitions<<pad()<<output<<" = "<<result<<";\n";retain(e.type,output);}finish();}
      pop(!terminated);definitions<<pad()<<"}\n";pop(false);definitions<<pad()<<"}\n";temporary_values=std::move(outer);
      if(!terminated&&e.type.has_functions())temporary_values.push_back({e.type,output});
      return output;
     }
     case Expr::Kind::TagTest:{auto root=expr(*e.children.at(0));return temp(Type::Bool,"("+root+".tag == INT64_C("+std::to_string(e.projection)+"))");}
     case Expr::Kind::Payload:{auto root=expr(*e.children.at(0));return temp(e.type,root+".payload.element_"+std::to_string(e.projection),true);}
     case Expr::Kind::Project:{auto root=expr(*e.children.at(0));return temp(e.type,root+".element_"+std::to_string(e.projection),true);}
    case Expr::Kind::Splice:case Expr::Kind::Lift:throw Error(e.where,"MSP stage lowering required before code generation");
    case Expr::Kind::Quote:return expr(*e.children.at(0));
    case Expr::Kind::Run:{auto code=expr(*e.children.at(0));return temp(e.type,code+".invoke("+code+".environment)");}
    case Expr::Kind::MacroCall:case Expr::Kind::SyntaxHole:case Expr::Kind::SyntaxBlock:case Expr::Kind::Extension:case Expr::Kind::SyntaxList:case Expr::Kind::SyntaxVector:case Expr::Kind::SyntaxKeyword:case Expr::Kind::SyntaxLet:
    throw Error(e.where,"unexpanded metaprogramming syntax before code generation");
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
   case Expr::Kind::Integer:
    if(e.type.is_unsigned()){uses_unsigned=true;return "UINT64_C("+std::to_string(std::bit_cast<std::uint64_t>(integer_literal_bits(e.text,e.where)))+")";}
    return e.text=="-9223372036854775808"?"(-INT64_C(9223372036854775807)-INT64_C(1))":"INT64_C("+e.text+")";
   case Expr::Kind::Float:{
    auto bits=float_literal_bits(e.text,e.where);
    auto payload=bits==INT64_MIN?"(-INT64_C(9223372036854775807)-INT64_C(1))":"INT64_C("+std::to_string(bits)+")";
    return temp(e.type,"on1x_"+type_name(e.type)+"_from_bits("+payload+")");
   }
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
   case Expr::Kind::Record:case Expr::Kind::Array:{
    std::vector<std::string> values;for(const auto& c:e.children)values.push_back(temp(c->type,expr(*c)));
    std::string result="("+ctype(e.type)+"){";
    if(e.kind==Expr::Kind::Array)result+="{";
    if(values.empty())result+="0";
    for(std::size_t i=0;i<values.size();++i){
     if(i)result+=", ";
     if(e.kind==Expr::Kind::Record){auto pos=std::find(e.type.labels().begin(),e.type.labels().end(),e.labels[i]);result+=".element_"+std::to_string(pos-e.type.labels().begin())+" = ";}
     result+=values[i];
    }
    if(e.kind==Expr::Kind::Array)result+="}";
    return temp(e.type,result+"}");
   }
   case Expr::Kind::Variant:{
    auto value=temp(e.children[0]->type,expr(*e.children[0]));
    return temp(e.type,"("+ctype(e.type)+"){.tag = INT64_C("+std::to_string(e.projection)+"), .payload = {.element_"+std::to_string(e.projection)+" = "+value+"}}");
   }
   case Expr::Kind::Field:{auto root=temp(e.children[0]->type,expr(*e.children[0]));return temp(e.type,root+".element_"+std::to_string(e.projection));}
   case Expr::Kind::Index:case Expr::Kind::Slice:case Expr::Kind::Length:{
    auto t=e.children[0]->type;bool slice=t.kind()==Type::Slice;
    // Fixed arrays are rvalue-safe value structs; only slices need a materialized
    // owner copy for chained slicing. Evaluation order stays left-to-right.
    auto root=slice?temp(t,expr(*e.children[0])):expr(*e.children[0]);
    auto length=slice?root+".length":"INT64_C("+std::to_string(t.extent())+")";
    if(e.kind==Expr::Kind::Length)return temp(Type::I64,length);
    auto lo=temp(Type::I64,expr(*e.children[1]));
    auto at=c_string(meta::source_literal(e.where.file))+", "+std::to_string(e.where.line);
    if(e.kind==Expr::Kind::Index){
     definitions<<pad()<<"on1x_bounds_index("<<lo<<", "<<length<<", "<<at<<");\n";
     return temp(e.type,slice?root+".backing.elements["+root+".start + "+lo+"]":root+".elements["+lo+"]");
    }
    auto hi=temp(Type::I64,expr(*e.children[2]));
    definitions<<pad()<<"on1x_bounds_slice("<<lo<<", "<<hi<<", "<<length<<", "<<at<<");\n";
    return temp(e.type,"("+ctype(e.type)+"){"+(slice?root+".backing":root)+", "+(slice?root+".start + ":"")+lo+", "+hi+" - "+lo+"}");
   }
   case Expr::Kind::Match:throw Error(e.where,"surface match must be canonicalized");
   case Expr::Kind::Unary:{
    auto value=expr(*e.children[0]);
     if(e.type.is_float())return temp(e.type,"on1x_neg_"+type_name(e.type)+"("+value+")");
    return temp(e.type,e.text=="-"?"on1x_neg_i64("+value+")":"(!"+value+")");
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
    if(!e.children[0]->type.has_functions())left=temp(e.children[0]->type,left);
    auto right=expr(*e.children[1]);
     if(e.children[0]->type.is_float()){
     uses_f64=true;
     static const std::map<std::string,std::string> methods={{"+","add"},{"-","sub"},{"*","mul"},{"/","div"},
      {"==","eq"},{"!=","ne"},{"<","lt"},{"<=","le"},{">","gt"},{">=","ge"}};
      return temp(e.type,"on1x_"+methods.at(op)+"_"+type_name(e.children[0]->type)+"("+left+", "+right+")");
    }
   if(op=="+"||op=="-"||op=="*"||op=="/"||op=="%"){
    std::string method=op=="+"?"add":op=="-"?"sub":op=="*"?"mul":op=="/"?"div":"mod";
     return temp(e.type,"on1x_"+method+"_"+type_name(e.type)+"("+left+", "+right+")");
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
    if(!child->type.has_functions())value=temp(child->type,value);
    if(callee.reference==Expr::Reference::Extern&&externs.at(callee.text)->params[i].pattern)
     flatten(*externs.at(callee.text)->params[i].pattern,child->type,value,args);
    else args.push_back(value);
   }
   if(builtin){
    const auto& name=callee.text;
    if(name=="println"){
      auto type=e.children[0]->type;target="on1x_print_"+type_name(type);
    }else if(name=="assert")target="on1x_assert";
    else if(name=="json_parse_or_die")target="on1x_json_parse_or_die";
    else if(name=="json_get_or_die")target="on1x_json_get_or_die";
    else if(name=="json_size")target="on1x_json_size";
    else if(name=="json_number")target="on1x_json_number";
     else if(name=="json_print")target="on1x_json_print";
     else if(data_primitive(name))target="on1x_"+name;
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
    case Stmt::Kind::TaskGroup:throw Error(s.where,"unlowered taskgroup");
    case Stmt::Kind::CspScope:{
     definitions<<pad()<<"{\n";push();
     auto owner=unique("taskgroup");
     definitions<<pad()<<"on1x_csp_group *"<<owner<<" = on1x_csp_group_enter("<<location(s.where)<<");\n";
     owners.back().push_back({Type::CspGroup,owner});
     bool terminated=false;for(const auto& child:s.body)terminated=stmt(child);
     pop(!terminated);definitions<<pad()<<"}\n";return terminated;
    }
   case Stmt::Kind::Defer:throw Error(s.where,"unlowered defer");
   case Stmt::Kind::PushDefer:{
    const auto& e=*s.value;const auto& lambda=lambdas.at(lambda_index.at(&e));
    auto at=c_string(meta::source_literal(s.where.file))+", "+std::to_string(s.where.line)+", "+std::to_string(s.where.column);
    auto call="on1x_defer_register(&"+defer_stack+", "+(e.captures.empty()?"0":"sizeof(struct "+lambda.environment+")")+", "+lambda.invoke+", "+(e.captures.empty()?"NULL":lambda.destroy)+", "+at+")";
    if(e.captures.empty())definitions<<pad()<<"(void)"<<call<<";\n";
    else{
     auto capture=unique("deferred_captures");definitions<<pad()<<"struct "<<lambda.environment<<" *"<<capture<<" = (struct "<<lambda.environment<<" *)"<<call<<";\n";
     for(std::size_t i=0;i<e.captures.size();++i){
      auto field=capture+"->capture_"+std::to_string(i);definitions<<pad()<<field<<" = "<<ref(e.captures[i].name)<<";\n";
      retain(e.captures[i].type,field);
     }
    }
    finish();return false;
   }
  case Stmt::Kind::Template:{for(const auto& instance:s.body)stmt(instance);return false;}
   case Stmt::Kind::Let:{
   auto value=expr(*s.value);
   if(s.pattern)bind_pattern(*s.pattern,s.value->type,value);else bind_name(s.name,s.value->type,value);
    finish();return false;
   }
   case Stmt::Kind::Storage:{
    auto name=unique("storage");scopes.back()[s.name]=name;definitions<<pad()<<ctype(s.declared)<<" "<<name<<" = {0};\n"<<pad()<<"(void)"<<name<<";\n";
    if(s.declared.has_functions())owners.back().push_back({s.declared,name});
    return false;
   }
   case Stmt::Kind::Initialize:case Stmt::Kind::Assign:{
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
     if(s.value->noreturn)return true;
    if(s.value->type!=Type::Void){
     auto result=unique("return");definitions<<pad()<<ctype(s.value->type)<<" "<<result<<" = "<<value<<";\n";
     retain(s.value->type,result);
      run_defers();finish();drops();leave_call();definitions<<pad()<<"return "<<result<<";\n";
      }else{run_defers();finish();drops();leave_call();definitions<<pad()<<"return;\n";}
     }else{run_defers();finish();drops();leave_call();definitions<<pad()<<"return;\n";}
   return true;
  }
   case Stmt::Kind::Expr:{
    auto value=expr(*s.value);if(s.value->noreturn)return true;if(!value.empty())definitions<<pad()<<"(void)"<<value<<";\n";
   finish();return false;
  }
  case Stmt::Kind::If:{
   auto condition=expr(*s.value);finish();definitions<<pad()<<"if ("<<condition<<") {\n";
   bool yes=body(s.body),no=false;definitions<<pad()<<"}";
   if(!s.otherwise.empty()){definitions<<" else {\n";no=body(s.otherwise);definitions<<pad()<<"}";}
   definitions<<"\n";return yes&&no;
  }
   case Stmt::Kind::While:{
    auto done=unique("break"),latch=unique("continue");
    loop_targets.push_back({s.control_target,owners.size(),done,latch});
    definitions<<pad()<<"while (true) {\n";++indent;
   auto condition=expr(*s.value);finish();definitions<<pad()<<"if (!("<<condition<<")) break;\n";
    definitions<<pad()<<"{\n";body(s.body);definitions<<pad()<<"}\n";
    if(continue_targets.contains(s.control_target))definitions<<pad()<<latch<<": ;\n";
     if(!s.otherwise.empty()){definitions<<pad()<<"{\n";body(s.otherwise);definitions<<pad()<<"}\n";}
     if(uses_csp)definitions<<pad()<<"on1x_csp_yield("<<location(s.where)<<");\n";
    --indent;definitions<<pad()<<"}\n";
    if(break_targets.contains(s.control_target))definitions<<pad()<<done<<": ;\n";
    loop_targets.pop_back();return false;
   }
   case Stmt::Kind::Break:case Stmt::Kind::Continue:{
    auto target=std::find_if(loop_targets.rbegin(),loop_targets.rend(),[&](const auto& t){return t.id==s.control_target;});
    if(target==loop_targets.rend())throw Error(s.where,"ON1X-CONTROL: missing C11 loop target");
    finish();
    for(std::size_t i=owners.size();i>target->keep;--i)for(auto owner=owners[i-1].rbegin();owner!=owners[i-1].rend();++owner)release(*owner);
    definitions<<pad()<<"goto "<<(s.kind==Stmt::Kind::Break?target->done:target->latch)<<";\n";return true;
   }
   case Stmt::Kind::Range:throw Error(s.where,"ON1X-CONTROL: unlowered range in C11 emitter");
   case Stmt::Kind::Region:{
    auto done=unique("region_exit");bool used=region_exit_targets.contains(s.control_target);region_targets.push_back({s.control_target,owners.size(),done,{}});
    definitions<<pad()<<"{\n";bool terminated=body(s.body);definitions<<pad()<<"}\n";region_targets.pop_back();if(used)definitions<<pad()<<done<<": ;\n";return terminated&&!used;
   }
   case Stmt::Kind::Leave:{
    auto target=std::find_if(region_targets.rbegin(),region_targets.rend(),[&](const auto& r){return r.id==s.control_target;});if(target==region_targets.rend())throw Error(s.where,"missing canonical region target");finish();
    for(std::size_t i=owners.size();i>target->keep;--i)for(auto owner=owners[i-1].rbegin();owner!=owners[i-1].rend();++owner)release(*owner);
    definitions<<pad()<<"goto "<<target->done<<";\n";return true;
   }
   case Stmt::Kind::Trap:definitions<<pad()<<"on1x_assert(false);\n"<<pad()<<"for (;;) {}\n";return true;
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
  if(!lambda.deferred){prototypes<<"static "<<ctype(e.type)<<" "<<lambda.make<<"(";capture_types(prototypes,e);prototypes<<");\n";}
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
  if(lambda.deferred)return;
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
                  const std::vector<Stmt>& statements,const Location& at,const Lambda* lambda=nullptr){
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
    if(uses_csp)definitions<<pad()<<"on1x_csp_call_enter("<<location(at)<<");\n";
   defer_stack.clear();if(has_defer(statements)){uses_defer=true;defer_stack=unique("defers");definitions<<pad()<<"on1x_defer_stack "<<defer_stack<<" = {0};\n";}
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
   if(!terminated)run_defers();
    pop(!terminated);if(!terminated)leave_call();if(result==Type::Void&&!terminated)definitions<<"  return;\n";
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
   uses_csp=csp::present(unit);
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
    for(const auto& f:unit.functions)if(!f.external&&!f.prototype)definition(functions.at(f.name),f.result,f.params,f.body,f.where);
   for(const auto& lambda:lambdas){const auto& e=*lambda.expression;definition(lambda.invoke,e.result,e.parameters,e.body,e.where,&lambda);}
  for(const auto& f:unit.functions)if(auto found=adapters.find(f.name);found!=adapters.end())adapter(f,found->second);
  emit_ownership_helpers();
  const auto& main=*std::find_if(unit.functions.begin(),unit.functions.end(),[](const auto& f){return f.name=="main";});
   if(uses_csp){
    auto entry=reserve("on1x_csp_entry");
    definitions<<"static int64_t "<<entry<<"(on1x_env *context) { (void)context; ";
    if(main.result==Type::Void)definitions<<functions.at("main")<<"(); return 0;";
    else definitions<<"return "<<functions.at("main")<<"();";
    definitions<<" }\nint main(void) { return (int)on1x_csp_run("<<entry<<", NULL, "<<location(main.where)<<"); }\n";
   }
   else if(main.result==Type::Void)definitions<<"int main(void) { "<<functions.at("main")<<"(); return 0; }\n";
  else definitions<<"int main(void) { return (int)"<<functions.at("main")<<"(); }\n";
   const std::string float_guard=uses_f64?
    "#if !defined(ON1X_F64_ABI_VERSION) || ON1X_F64_ABI_VERSION != 1\n#error \"unsupported On1x f64 target or runtime ABI\"\n#endif\n"
    "#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) || defined(__NO_SIGNED_ZEROS__) || defined(__ASSOCIATIVE_MATH__) || defined(__RECIPROCAL_MATH__)\n#error \"On1x f64 forbids fast/finite-only math and relaxed IEEE modes\"\n#endif\n\n":"";
   return "/* Generated by on1xc. Do not edit. */\n#include <on1x_runtime.h>\n#include <stdint.h>\n#include <stdbool.h>\n#include <string.h>\n"
    "#if !defined(ON1X_CLOSURE_ABI_VERSION) || ON1X_CLOSURE_ABI_VERSION != 1\n#error \"incompatible On1x closure runtime ABI\"\n#endif\n\n"
    "#if !defined(ON1X_DATA_ABI_VERSION) || ON1X_DATA_ABI_VERSION != 1\n#error \"incompatible On1x data runtime ABI\"\n#endif\n\n"
     +(uses_value_data?std::string("#if !defined(ON1X_VALUE_DATA_ABI_VERSION) || ON1X_VALUE_DATA_ABI_VERSION != 1\n#error \"incompatible On1x value data runtime ABI\"\n#endif\n\n"):std::string{})
      +(uses_f32?std::string("#if !defined(ON1X_F32_ABI_VERSION) || ON1X_F32_ABI_VERSION != 1\n#error \"unsupported On1x f32 target or runtime ABI\"\n#endif\n"):std::string{})
      +(uses_unsigned?std::string("#if !defined(ON1X_UNSIGNED_ABI_VERSION) || ON1X_UNSIGNED_ABI_VERSION != 1\n#error \"incompatible On1x unsigned runtime ABI\"\n#endif\n"):std::string{})
       +(uses_defer?std::string("#if !defined(ON1X_DEFER_ABI_VERSION) || ON1X_DEFER_ABI_VERSION != 1\n#error \"incompatible On1x defer runtime ABI\"\n#endif\n"):std::string{})
        +(uses_csp?std::string("#include <on1x_concurrency.h>\n#if !defined(ON1X_CSP_ABI_VERSION) || ON1X_CSP_ABI_VERSION != 2\n#error \"unsupported On1x cooperative fiber target or runtime ABI\"\n#endif\n"):std::string{})
      +float_guard+types.str()+"\n"+prototypes.str()+"\n"+value_helpers.str()+helpers.str()+definitions.str();
 }
};
}
std::string emit_c(const Unit& unit,const std::string& source_name){(void)source_name;canonical::verify_control(unit);meta::require_core(unit);canonical::verify_values(unit);verify_nominals(unit);csp::verify(unit,true);return Generator(unit).run();}
}
