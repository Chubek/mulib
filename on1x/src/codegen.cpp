#include <on1x/ast.hpp>
#include <on1x/kernel.hpp>
#include <on1x/container_shared.hpp>
#include <on1x/extensions.hpp>
#include <on1x/meta.hpp>
#include <on1x/primitives.hpp>
#include <on1x/numeric.hpp>
#include <on1x/literals.hpp>
#include <on1x/nominal.hpp>
#include <on1x/regex.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/canonical/values.hpp>
#include <on1x/concurrency.hpp>
#include <on1x/cliche.hpp>
#include <on1x/inherent.hpp>
#include <on1x/c11.hpp>
#include "../hir/MEMGraph/memgraph.hpp"
#include "../hir/MEMGraph/storage.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
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
 struct Owner { Type type; std::string name,cleanup; };
 struct Lambda { const Expr* expression; std::string invoke,make,environment,destroy; bool deferred; };
 struct AsyncFrame { const Expr* block; std::string type,resume,drop; };
  const Unit& unit;
  NominalCatalog catalog;
  memgraph::Bounds bounds;
  std::optional<std::size_t> scratch_stack_bytes;
  bool scratch_layout_used=false;
  std::string heap_cell_type;
  std::map<std::size_t,Location>* origins;
  const SourcePrefixes* source_prefixes;
  std::map<std::size_t,const Location*> proto_origins,body_origins;
  const Location* enclosing=nullptr;
 std::ostringstream types,prototypes,helpers,value_helpers,definitions;
 regex::Cache regex_patterns;
 std::map<std::string,std::string> regex_constants;
 std::map<Type,std::string> function_types;
 std::map<Type,std::string> tuple_types,tuple_retains,tuple_releases;
 std::vector<std::pair<Type,bool>> ownership_helpers;
 struct UniqueDestructor {Type target;std::string name,release;};
 std::map<Type,std::string> unique_destructor_names;
 std::vector<UniqueDestructor> unique_destructors;
 struct CleanupDestructor {Type type;std::string name,release;};
 std::map<Type,std::string> cleanup_destructor_names;
 std::vector<CleanupDestructor> cleanup_destructors;
 std::set<std::string> identifiers;
 std::map<std::string,std::string> functions,adapters;
  std::map<std::string,const Function*> externs;
 std::map<std::string,const Function*> borrow_functions;
 std::vector<Lambda> lambdas;
 std::unordered_map<const Expr*,std::size_t> lambda_index;
 std::vector<AsyncFrame> async_frames;
 std::unordered_map<const Expr*,std::size_t> async_frame_index;
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
    bool uses_value_data=false,uses_defer=false,uses_regex=false,uses_unique=false,uses_shared=false;
    bool ownership_cleanup=false;
     bool uses_csp=false,uses_group_deadlines=false;
     Type current_result=Type::Void;
      std::size_t cancellation_budget=262144,ownership_cleanup_budget=262144;
    std::string source_literal(const Location& at)const{
     return c_string(meta::source_literal(source_prefixes?remap_source_path(at.file,*source_prefixes):at.file));
    }
    std::string location(const Location& at)const{return source_literal(at)+", "+std::to_string(at.line)+", "+std::to_string(at.column);}

 std::string reserve(const std::string& preferred){
  std::string name=preferred;std::size_t suffix=0;
  while(!identifiers.insert(name).second)name=preferred+"_"+std::to_string(++suffix);
  return name;
 }
  std::string unique(const std::string& name){return reserve("on1x_v"+std::to_string(++serial)+"_"+name);}
   void use_heap_cells(){
    if(!heap_cell_type.empty())return;
    do { heap_cell_type=unique("memgraph_cell"); }
    while(identifiers.contains(heap_cell_type+"_alloc")||identifiers.contains(heap_cell_type+"_ptr"));
    identifiers.insert(heap_cell_type+"_alloc");
    identifiers.insert(heap_cell_type+"_ptr");
   }
  std::string pad()const{return std::string(indent*2,' ');}
 static bool needs_drop(const Type& type){return type.has_functions()||type.is_move_only();}
 std::string regex_literal(const Expr& e){
  if(auto found=regex_constants.find(e.text);found!=regex_constants.end())return "&"+found->second;
  const auto& program=regex_patterns.get(e.text,e.where);
  auto name=unique("regex");
  regex_constants.emplace(e.text,name);
  uses_regex=true;
  helpers<<"static const on1x_regex_edge "<<name<<"_edges[] = {\n";
  std::size_t edges=0;
  for(const auto& state:program.states)for(const auto& edge:state.edges){
   helpers<<"  {"<<edge.first<<", "<<edge.last<<", "<<edge.target<<"},\n";++edges;
  }
  if(!edges)helpers<<"  {0, 0, 0},\n";
  helpers<<"};\nstatic const on1x_regex_state "<<name<<"_states[] = {\n";
  edges=0;
  for(const auto& state:program.states){
   helpers<<"  {"<<edges<<", "<<edges+state.edges.size()<<", "<<(state.accepts?"true":"false")<<"},\n";
   edges+=state.edges.size();
  }
  helpers<<"};\n";
  if(!program.groups.empty()){
   helpers<<"static const on1x_regex_span "<<name<<"_groups[] = {\n";
   for(const auto& group:program.groups)helpers<<"  {"<<group.begin<<", "<<group.end<<"},\n";
   helpers<<"};\n";
  }
  helpers<<"static const on1x_regex "<<name<<" = {"<<name<<"_states, "<<name<<"_edges, "
         <<(program.groups.empty()?"NULL":name+"_groups")<<", "<<program.groups.size()<<", "
         <<(program.capturable?"true":"false")<<"};\n";
  return "&"+name;
 }
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
      case Type::Own:uses_unique=true;return "on1x_unique *";
     case Type::Fiber:return "on1x_csp_task *";
     case Type::ExternalFiber:return "on1x_csp_process *";
      case Type::Channel:return "on1x_csp_channel *";
      case Type::Sender:case Type::Receiver:return "on1x_csp_endpoint *";
    case Type::CspGroup:return "on1x_csp_group *";
   case Type::I64:return "int64_t";case Type::Bool:return "bool";case Type::Char:return "uint32_t";
    case Type::F64:uses_f64=true;return "double";
    case Type::F32:uses_f64=true;uses_f32=true;return "float";
    case Type::U8:case Type::U16:case Type::U32:case Type::U64:
     uses_unsigned=true;return "uint"+type_name(type).substr(1)+"_t";
  case Type::String:return "const char *";case Type::Void:return "void";
   case Type::Regex:uses_regex=true;return "const on1x_regex *";
   case Type::RegexCaptures:uses_regex=true;return "on1x_regex_captures";
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
  if(type.is_tuple()||type.is_data())return type.elements();
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
   if(retaining&&type.is_move_only())
    throw std::runtime_error("internal codegen: attempted to retain a unique value");
   if(retaining?!type.has_functions():!needs_drop(type))return;
    if(type.is_own())output<<prefix<<"on1x_unique_drop("<<value<<");\n";
    else if(type.is_reference())output<<prefix<<(retaining?"on1x_data_retain(":"on1x_data_release(")<<value<<");\n";
    else if(type.is_function()||type.is_code())output<<prefix<<(retaining?"on1x_env_retain(":"on1x_env_release(")<<value<<".environment);\n";
   else output<<prefix<<ownership_helper(type,retaining)<<"(&("<<value<<"));\n";
  }
  std::string unique_destructor(const Type& owner,const Location& at){
   if(!owner.is_own())throw std::runtime_error("internal codegen: destructor requested for non-owner");
   if(auto found=unique_destructor_names.find(owner);found!=unique_destructor_names.end())
    return found->second;
   const auto& target=owner.owned();
   auto value=catalog.instantiate(target.nominal(),target.arguments(),at);
   auto name=reserve("on1x_unique_destroy_"+std::to_string(++helper_serial));
   std::string release;
   if(needs_drop(value))release=ownership_helper(value,false);
   unique_destructor_names.emplace(owner,name);
   unique_destructors.push_back({std::move(value),name,std::move(release)});
   prototypes<<"static void "<<name<<"(void *);\n";
   return name;
  }
  std::string cleanup_destructor(const Type& type){
   if(!needs_drop(type))
    throw std::runtime_error("internal codegen: cleanup requested for trivial value");
   if(auto found=cleanup_destructor_names.find(type);found!=cleanup_destructor_names.end())
    return found->second;
   auto name=reserve("on1x_cleanup_destroy_"+std::to_string(++helper_serial));
   std::string release;
   if(!type.is_own()&&!type.is_reference()&&!type.is_function()&&!type.is_code())
    release=ownership_helper(type,false);
   cleanup_destructor_names.emplace(type,name);
   cleanup_destructors.push_back({type,name,std::move(release)});
   prototypes<<"static void "<<name<<"(void *);\n";
   uses_unique=true;
   return name;
  }
 void emit_ownership_helpers(){
  // Generate only requested operations. Child requests may append to the list;
  // all prototypes precede all definitions, so dependency order remains valid.
  for(std::size_t i=0;i<ownership_helpers.size();++i){
   auto [type,retaining]=ownership_helpers[i];auto name=ownership_helper(type,retaining);
   value_helpers<<"static void "<<name<<"(const "<<ctype(type)<<" *on1x_value) {\n";
    value_helpers<<"  (void)on1x_value;\n";
    if(type.kind()==Type::Sum){
     value_helpers<<"  switch (on1x_value->tag) {\n";
     for(std::size_t j=0;j<fields(type).size();++j){
      const auto& payload=fields(type)[j];
       if(retaining?!payload.has_functions():!needs_drop(payload))continue;
      value_helpers<<"  case "<<j<<":\n";
      adjust(value_helpers,payload,"on1x_value->payload.element_"+std::to_string(j),retaining,"    ");
      value_helpers<<"    break;\n";
     }
     value_helpers<<"  default: break;\n  }\n}\n\n";
     continue;
    }
    for(std::size_t j=0;j<fields(type).size();++j){
     auto index=retaining?j:fields(type).size()-1-j;
     adjust(value_helpers,fields(type)[index],"on1x_value->element_"+std::to_string(index),retaining,"  ");
   }
   value_helpers<<"}\n\n";
   }
  }
  void emit_unique_destructors(){
   for(const auto& destructor:unique_destructors){
    value_helpers<<"static void "<<destructor.name<<"(void *on1x_raw) {\n";
    if(destructor.release.empty())value_helpers<<"  (void)on1x_raw;\n";
    else value_helpers<<"  "<<destructor.release<<"((const "<<ctype(destructor.target)<<" *)on1x_raw);\n";
    value_helpers<<"}\n\n";
   }
   for(const auto& destructor:cleanup_destructors){
    value_helpers<<"static void "<<destructor.name<<"(void *on1x_raw) {\n"
                 <<"  "<<ctype(destructor.type)<<" *on1x_value = ("<<ctype(destructor.type)<<" *)on1x_raw;\n";
    if(destructor.type.is_own())
     value_helpers<<"  on1x_unique_drop(*on1x_value);\n";
    else if(destructor.type.is_reference())
     value_helpers<<"  on1x_data_release(*on1x_value);\n";
    else if(destructor.type.is_function()||destructor.type.is_code())
     value_helpers<<"  on1x_env_release(on1x_value->environment);\n";
    else value_helpers<<"  "<<destructor.release<<"(on1x_value);\n";
    value_helpers<<"  memset(on1x_value, 0, sizeof(*on1x_value));\n}\n\n";
   }
  }
  void collect(const Expr& expression,bool deferred=false){
   if(expression.prototype)return;
   if(expression.kind==Expr::Kind::Csp&&expression.text=="launch_async"){
    auto index=async_frames.size();auto suffix=std::to_string(index+1);
    async_frame_index.emplace(&expression,index);
    async_frames.push_back({expression.children[0].get(),reserve("on1x_async_fields_"+suffix),
                            reserve("on1x_async_resume_"+suffix),reserve("on1x_async_drop_"+suffix)});
    return; // Checked frame carrier is not an executable closure.
   }
    if(expression.type.contains_unique()||expression.result.contains_unique())
     uses_unique=true;
    for(const auto& parameter:expression.parameters)
     if(parameter.type.contains_unique())uses_unique=true;
    for(const auto& capture:expression.captures)
     if(capture.type.contains_unique())uses_unique=true;
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
     if(statement.declared.contains_unique())uses_unique=true;
    if(statement.kind==Stmt::Kind::CspScope&&statement.value)uses_group_deadlines=true;
    if(statement.kind==Stmt::Kind::Break)break_targets.insert(statement.control_target);
    if(statement.kind==Stmt::Kind::Continue)continue_targets.insert(statement.control_target);
    if(statement.kind==Stmt::Kind::Leave)region_exit_targets.insert(statement.control_target);
   if(statement.value&&statement.kind!=Stmt::Kind::Template)collect(*statement.value,statement.kind==Stmt::Kind::PushDefer);
   collect(statement.body);collect(statement.otherwise);
  }
 }
  void push(){scopes.emplace_back();owners.emplace_back();++indent;}
   void release(const Owner& owner){
    if(!owner.cleanup.empty())definitions<<pad()<<"on1x_cleanup_pop(&"<<owner.cleanup<<");\n";
    if(owner.type==Type::CspGroup)definitions<<pad()<<"on1x_csp_group_finish("<<owner.name<<");\n";
   else if(owner.type==Type::Json)definitions<<pad()<<"on1x_json_drop("<<owner.name<<");\n";
  else adjust(definitions,owner.type,owner.name,false,pad());
 }
 void drops(std::size_t live_scopes){
  for(auto i=live_scopes;i>0;--i)
   for(auto owner=owners[i-1].rbegin();owner!=owners[i-1].rend();++owner)release(*owner);
 }
 void drops(){drops(owners.size());}
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
    void run_defers(){if(!defer_stack.empty()){
     if(uses_csp)definitions<<pad()<<"on1x_csp_shield_enter();\n";
     definitions<<pad()<<"on1x_defer_run(&"<<defer_stack<<");\n";
     if(uses_csp)definitions<<pad()<<"on1x_csp_shield_leave();\n";
    }}
    void leave_call(){if(uses_csp)definitions<<pad()<<"on1x_csp_call_leave();\n";}
    void cancellation_edge(const Location& at,std::size_t live_scopes){
     if(!uses_csp)return;
     std::size_t work=1+temporary_values.size();
     for(std::size_t i=0;i<live_scopes;++i)work+=owners[i].size();
     if(work>cancellation_budget)throw Error(at,"cancellation cleanup expansion budget exhausted","ON1X-CSP-LIMIT");
     cancellation_budget-=work;
     definitions<<pad()<<"if (on1x_csp_cancelled()) {\n";++indent;
     definitions<<pad()<<"on1x_csp_shield_enter();\n";
     run_defers();
     for(auto t=temporary_values.rbegin();t!=temporary_values.rend();++t)release(*t);
     drops(live_scopes);definitions<<pad()<<"on1x_csp_shield_leave();\n";leave_call();
     definitions<<pad()<<"return"<<(current_result==Type::Void?"":" ("+ctype(current_result)+"){0}")<<";\n";
     --indent;definitions<<pad()<<"}\n";
    }
    void cancellation_edge(const Location& at){cancellation_edge(at,owners.size());}
  void finish(std::size_t keep=0){
   for(std::size_t i=temporary_values.size();i>keep;--i)release(temporary_values[i-1]);
   temporary_values.resize(keep);
  }
  void clear_value(const Type& type,const std::string& value){
   if(!needs_drop(type))return;
   definitions<<pad()<<"memset(&("<<value<<"), 0, sizeof("<<value<<"));\n";
  }
  Owner track(const Type& type,const std::string& name){
   Owner owner{type,name,{}};
   // The emergency registry is needed only in units that can reach a unique
   // allocation. Such units register every live managed value so terminal
   // allocation failure also releases unrelated shared references in callers.
   if(ownership_cleanup&&needs_drop(type)){
    owner.cleanup=unique("cleanup");
    definitions<<pad()<<"on1x_cleanup "<<owner.cleanup<<" = {0};\n"
               <<pad()<<"on1x_cleanup_push(&"<<owner.cleanup<<", &("<<name<<"), "
               <<cleanup_destructor(type)<<");\n";
   }
   return owner;
  }
  std::string temp(const Type& type,const std::string& rhs,bool borrowed=false){
   if(type==Type::Void){definitions<<pad()<<rhs<<";\n";return {};}
   auto name=unique("tmp");definitions<<pad()<<ctype(type)<<" "<<name<<" = "<<rhs<<";\n";
   if(needs_drop(type)){
    if(borrowed)retain(type,name);
    temporary_values.push_back(track(type,name));
   }
   return name;
  }
  std::string transfer_temp(const Type& type,const std::string& source){
   if(!needs_drop(type))return temp(type,source);
   auto result=temp(type,source);
   clear_value(type,source);
   return result;
  }
  std::string move_source(const Type& type,const std::string& source){
   return needs_drop(type)?transfer_temp(type,source):temp(type,source);
  }
  void disarm_temporary(const std::string& name){
   auto found=std::find_if(temporary_values.rbegin(),temporary_values.rend(),
                           [&](const Owner& owner){return owner.name==name;});
   if(found==temporary_values.rend()||found->cleanup.empty())
    throw std::runtime_error("internal codegen: missing unique argument cleanup");
   definitions<<pad()<<"on1x_cleanup_pop(&"<<found->cleanup<<");\n";
   found->cleanup.clear();
  }
  void allocation_failure(const std::string& owner,const Location& at){
   std::size_t work=1+temporary_values.size();
   for(const auto& scope:owners)work+=scope.size();
   if(work>ownership_cleanup_budget)
    throw Error(at,"ownership allocation cleanup expansion budget exhausted","ON1X-OWN-LIMIT");
   ownership_cleanup_budget-=work;
   definitions<<pad()<<"if (!"<<owner<<") {\n";++indent;
   for(auto value=temporary_values.rbegin();value!=temporary_values.rend();++value)
    release(*value);
   drops();
   definitions<<pad()<<"on1x_unique_allocation_fail("<<location(at)<<");\n";
   --indent;definitions<<pad()<<"}\n";
  }
  std::string global_value(const Expr& expression){
  auto found=adapters.find(expression.text);
   if(found==adapters.end())found=adapters.emplace(expression.text,reserve("on1x_adapter_"+(expression.text.starts_with("@")?expression.text.substr(1):expression.text))).first;
   return temp(expression.type,"("+ctype(expression.type)+"){"+found->second+", NULL}");
  }
  std::string borrow_address(const Expr& source,const Type& referent,
                             const Location& at){
   if(source.kind!=Expr::Kind::Variable||
      source.reference!=Expr::Reference::Local)
    throw Error(source.where,"borrow lowering requires a checked local place",
                "ON1X-REF");
   if(source.type.is_own()&&source.type!=referent){
    uses_unique=true;
    auto target=ctype(referent);
    return "(("+target+" *)on1x_unique_borrow("+ref(source.text)+
      ", sizeof("+target+"), "+location(at)+"))";
   }
   return "&("+ref(source.text)+")";
  }
#include "container_shared_emit.inc"
  void flatten(const Pattern& pattern,const Type& type,const std::string& value,std::vector<std::string>& args){
  if(!pattern.is_tuple()){args.push_back(value);return;}
  auto root=unique("extern_pattern");
  definitions<<pad()<<"const "<<ctype(type)<<" *"<<root<<" = &("<<value<<");\n";
  for(std::size_t i=0;i<pattern.elements.size();++i)
   flatten(pattern.elements[i],type.elements()[i],root+"->element_"+std::to_string(i),args);
 }
   std::string expr(const Expr& e){
    auto result=emit_expr(e);
    if(e.kind==Expr::Kind::Call||e.kind==Expr::Kind::Csp||(uses_group_deadlines&&e.kind==Expr::Kind::Run))cancellation_edge(e.where);
    return result;
   }
   std::string emit_expr(const Expr& e){
        switch(e.kind){
        case Expr::Kind::ClicheMember:
         throw Error(e.where,"unresolved cliche projection reached C11 lowering",
                     "ON1X-CLICHE-UNSUPPORTED");
        case Expr::Kind::InherentMember:
         throw Error(e.where,"unresolved inherent projection reached C11 lowering",
                     "ON1X-IMPL");
         case Expr::Kind::SourceCsp:throw Error(e.where,"unlowered concurrency operation");
         case Expr::Kind::Borrow:
          throw Error(e.where,"borrow expression escaped its lexical let binding",
                      "ON1X-REF");
        case Expr::Kind::Csp:{
         if(uses_group_deadlines)cancellation_edge(e.where);
         const auto& op=e.text;auto at=location(e.where);
          if(op=="poll"){
          std::size_t n=e.children.size();bool fallback=e.children.back()->kind==Expr::Kind::Boolean;
          if(fallback)--n;
          bool deadline=n&&e.children[n-1]->text=="offer_after";if(deadline)--n;
           auto offers=unique("poll_offers");definitions<<pad()<<"on1x_csp_offer "<<offers<<"["<<std::max<std::size_t>(1,n)<<"] = {0};\n";
           std::vector<std::string> received(n);
           for(std::size_t i=0;i<n;++i){const auto& offer=*e.children[i];
            auto endpoint=expr(*offer.children[0]);
            const auto payload_type=offer.children[0]->type.csp_payload();
            definitions<<pad()<<offers<<"["<<i<<"].endpoint = "<<endpoint<<";\n";
            definitions<<pad()<<offers<<"["<<i<<"].operation = "<<(offer.text=="offer_send"?1:2)<<";\n";
            definitions<<pad()<<offers<<"["<<i<<"].element_size = sizeof("<<ctype(payload_type)<<");\n";
            if(offer.text=="offer_send"){
             auto value=temp(payload_type,expr(*offer.children[1]));
             definitions<<pad()<<offers<<"["<<i<<"].send_value = &"<<value<<";\n";
            }else{
             received[i]=temp(payload_type,"{0}");
             definitions<<pad()<<offers<<"["<<i<<"].receive_value = &"<<received[i]<<";\n";
            }
           }
           auto selected=unique("poll_selected"),closed=unique("poll_closed");
           definitions<<pad()<<"size_t "<<selected<<" = 0;\n"<<pad()<<"bool "<<closed<<" = false;\n";
           std::string delay;if(deadline)delay=expr(*e.children[n]->children[0]);
           definitions<<pad()<<(deadline?"on1x_csp_select_after(":"on1x_csp_select(")<<offers<<", "<<n<<", "<<(fallback?"true":"false")<<", "<<(deadline?delay+", ":"")<<"&"<<selected<<", &"<<closed<<", "<<at<<");\n";
          auto result=temp(e.type,"{0}");
          auto index=[&](const std::string& label){auto it=std::find(e.type.labels().begin(),e.type.labels().end(),label);
           if(it==e.type.labels().end())throw Error(e.where,"missing checked poll result variant");
           return static_cast<std::size_t>(it-e.type.labels().begin());};
          if(deadline)definitions<<pad()<<"if ("<<selected<<" == "<<n<<") "<<result<<".tag = "<<index("After")<<";\n";
          if(fallback)definitions<<pad()<<"if ("<<selected<<" == "<<n+(deadline?1:0)<<") "<<result<<".tag = "<<index("Default")<<";\n";
          for(std::size_t i=0;i<n;++i){
           auto success=index((e.children[i]->text=="offer_recv"?"Recv":"Sent")+std::to_string(i));
           auto failure=index("Closed"+std::to_string(i));
           definitions<<pad()<<"if ("<<selected<<" == "<<i<<") "<<result<<".tag = "<<closed<<" ? "<<failure<<" : "<<success<<";\n";
             if(e.children[i]->text=="offer_recv")
              definitions<<pad()<<"if ("<<selected<<" == "<<i<<" && !"<<closed<<") "
                         <<result<<".payload.element_"<<success<<" = "<<received[i]<<";\n";
           }
          return result;
         }
         if(op=="launch"){
          auto fn=expr(*e.children.at(0));
          return temp(e.type,"on1x_csp_launch("+fn+".invoke, "+fn+".environment, "+at+")");
         }
         if(op=="launch_async"){
          const auto& frame=async_frames.at(async_frame_index.at(&e));
          auto raw=unique("async_frame");
          definitions<<pad()<<"void *"<<raw<<" = on1x_async_frame_alloc_v1(sizeof(struct "<<frame.type<<"), "<<at<<");\n";
          return temp(e.type,"on1x_async_launch_v1("+frame.resume+", "+frame.drop+", "+raw+", "+at+")");
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
         if(op=="channel"){
          auto payload=ctype(e.type.csp_payload());
          return temp(e.type,"on1x_csp_channel_create(sizeof("+payload+"), "+
                      std::to_string(e.type.sender_cardinality())+", "+
                      std::to_string(e.type.receiver_cardinality())+", "+
                      expr(*e.children.at(2))+", "+at+")");
         }
         auto handle=expr(*e.children.at(0));
         if(op=="split"){
          auto result=temp(e.type,"{0}");
          auto endpoints=unique("split_endpoints");
          auto senders=e.children[0]->type.sender_cardinality();
          auto receivers=e.children[0]->type.receiver_cardinality();
          auto count=senders+receivers;
          definitions<<pad()<<"on1x_csp_endpoint *"<<endpoints<<"["<<count<<"] = {0};\n"
                     <<pad()<<"on1x_csp_channel_split("<<handle<<", "<<endpoints<<", "
                     <<senders<<", "<<receivers<<", "<<at<<");\n";
          for(std::size_t i=0;i<count;++i)
           definitions<<pad()<<result<<".element_"<<i<<" = "<<endpoints<<"["<<i<<"];\n";
          return result;
         }
         if(op=="join")return temp(e.type,std::string(e.children[0]->type==Type::ExternalFiber?"on1x_csp_process_join(":
                             async_frames.empty()?"on1x_csp_join(":"on1x_async_join_internal_v1(")+handle+", "+at+")");
          if(op=="cancel")return temp(e.type,std::string(e.children[0]->type==Type::Fiber?"on1x_csp_cancel(":"on1x_csp_process_cancel(")+handle+", "+at+")");
          if(op=="close")return temp(e.type,"on1x_csp_close("+handle+", "+at+")");
          if(op=="send"){
           auto payload_type=e.children[0]->type.csp_payload();
           auto payload=temp(payload_type,expr(*e.children.at(1)));
           auto status=temp(Type::I64,"on1x_csp_send("+handle+", &"+payload+
                            ", sizeof("+ctype(payload_type)+"), "+at+")");
         auto result=temp(e.type,"{0}");
         definitions<<pad()<<result<<".tag = "<<status<<" == 0 ? 1 : 0;\n"
                    <<pad()<<"if ("<<status<<" != 0) "<<result<<".payload.element_0 = "<<status<<";\n";
         return result;
         }
          if(op=="recv"){
           auto payload_type=e.children[0]->type.csp_payload();
           auto payload=temp(payload_type,"{0}");
           auto status=temp(Type::Bool,"on1x_csp_receive("+handle+", &"+payload+
                            ", sizeof("+ctype(payload_type)+"), "+at+")");
         auto result=temp(e.type,"{0}");
         definitions<<pad()<<result<<".tag = "<<status<<" ? 1 : 0;\n"
                    <<pad()<<"if ("<<status<<") "<<result<<".payload.element_1 = "<<payload<<";\n";
          return result;
         }
         if(op=="try_send"){
          auto payload_type=e.children[0]->type.csp_payload();
          auto payload=temp(payload_type,expr(*e.children.at(1)));
          auto status=temp(Type::I64,"on1x_csp_try_send("+handle+", &"+payload+
                           ", sizeof("+ctype(payload_type)+"), "+at+")");
          auto result=temp(e.type,"{0}");
          auto tag=[&](const std::string& label){
           auto found=std::find(e.type.labels().begin(),e.type.labels().end(),label);
           if(found==e.type.labels().end())throw Error(e.where,"missing checked try_send result variant");
           return static_cast<std::size_t>(found-e.type.labels().begin());
          };
          definitions<<pad()<<result<<".tag = "<<status<<" == ON1X_CSP_TRANSFERRED ? "
                     <<tag("Sent")<<" : ("<<status<<" == ON1X_CSP_CLOSED ? "
                     <<tag("Closed")<<" : "<<tag("WouldBlock")<<");\n";
          return result;
         }
         if(op=="try_recv"){
          auto payload_type=e.children[0]->type.csp_payload();
          auto payload=temp(payload_type,"{0}");
          auto status=temp(Type::I64,"on1x_csp_try_receive("+handle+", &"+payload+
                           ", sizeof("+ctype(payload_type)+"), "+at+")");
          auto result=temp(e.type,"{0}");
          auto tag=[&](const std::string& label){
           auto found=std::find(e.type.labels().begin(),e.type.labels().end(),label);
           if(found==e.type.labels().end())throw Error(e.where,"missing checked try_recv result variant");
           return static_cast<std::size_t>(found-e.type.labels().begin());
          };
          definitions<<pad()<<result<<".tag = "<<status<<" == ON1X_CSP_TRANSFERRED ? "
                     <<tag("Value")<<" : ("<<status<<" == ON1X_CSP_CLOSED ? "
                     <<tag("Closed")<<" : "<<tag("Empty")<<");\n"
                     <<pad()<<"if ("<<status<<" == ON1X_CSP_TRANSFERRED) "
                     <<result<<".payload.element_"<<tag("Value")<<" = "<<payload<<";\n";
          return result;
         }
        throw Error(e.where,"unknown canonical concurrency operation");
       }
      case Expr::Kind::Propagate:throw Error(e.where,"unlowered propagation");
     case Expr::Kind::Unit:return {};
       case Expr::Kind::PatternMatch:case Expr::Kind::Case:case Expr::Kind::Loop:case Expr::Kind::UnsafeBlock:throw Error(e.where,"surface value construct must be canonicalized");
      case Expr::Kind::Region:{
       auto output=e.type==Type::Void?std::string{}:unique("region_value");if(!output.empty())definitions<<pad()<<ctype(e.type)<<" "<<output<<" = {0};\n";
       auto outer=std::move(temporary_values);temporary_values.clear();definitions<<pad()<<"{\n";push();owners.back()=outer;definitions<<pad()<<"{\n";push();
       bool terminated=false;for(const auto& s:e.body)terminated=stmt(s);
       if(!terminated){auto result=expr(*e.children.at(0));if(!output.empty()){
        definitions<<pad()<<output<<" = "<<result<<";\n";
        if(e.type.is_move_only())clear_value(e.type,result);else retain(e.type,output);
       }finish();}
       pop(!terminated);definitions<<pad()<<"}\n";pop(false);definitions<<pad()<<"}\n";temporary_values=std::move(outer);
       if(!terminated&&needs_drop(e.type))temporary_values.push_back(track(e.type,output));
       return output;
      }
      case Expr::Kind::TagTest:{auto root=expr(*e.children.at(0));return temp(Type::Bool,"("+root+".tag == INT64_C("+std::to_string(e.projection)+"))");}
      case Expr::Kind::Payload:{
       auto root=expr(*e.children.at(0));
       auto place=root+".payload.element_"+std::to_string(e.projection);
       return e.move_out?transfer_temp(e.type,place):temp(e.type,place,e.type.has_functions());
      }
      case Expr::Kind::Project:{
       auto root=expr(*e.children.at(0));
       auto place=root+".element_"+std::to_string(e.projection);
       return e.move_out?transfer_temp(e.type,place):temp(e.type,place,e.type.has_functions());
      }
    case Expr::Kind::Splice:case Expr::Kind::Lift:throw Error(e.where,"MSP stage lowering required before code generation");
    case Expr::Kind::Quote:return expr(*e.children.at(0));
   case Expr::Kind::Run:{
    auto code=expr(*e.children.at(0));
    if(uses_group_deadlines)cancellation_edge(e.where);
    return temp(e.type,code+".invoke("+code+".environment)");
   }
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
  case Expr::Kind::RegexLiteral:return regex_literal(e);
  case Expr::Kind::RegexMatch:case Expr::Kind::RegexCapture:{
   auto pattern=temp(Type::Regex,expr(*e.children[0]));
   auto text=temp(Type::String,expr(*e.children[1]));
   return temp(e.type,std::string(e.kind==Expr::Kind::RegexMatch?"on1x_regex_match_v1(":"on1x_regex_capture_v1(")+
               pattern+", "+text+", "+location(e.where)+")");
  }
  case Expr::Kind::Character:return "UINT32_C("+std::to_string(literals::character(e.text,e.where))+")";
  case Expr::Kind::ByteBuffer:{
   // Split C tokens to stay below the C11 minimum character-string
   // translation limit for individual buffer literal chunks.
   auto buffer=temp(e.type,"on1x_buffer_new()");
   for(std::size_t i=0;i<e.text.size();i+=2048)
    definitions<<pad()<<"on1x_buffer_append_hex("<<buffer<<", \""<<e.text.substr(i,2048)<<"\");\n";
   return buffer;
  }
  case Expr::Kind::Interpolated:return format_value(e,false);
   case Expr::Kind::Variable:
    if(e.reference==Expr::Reference::Function)return global_value(e);
    if(e.reference==Expr::Reference::Local){
     auto value=ref(e.text);
     if(e.move_out)return transfer_temp(e.type,value);
     if(e.type.is_move_only())return value;
     return e.type.has_functions()?temp(e.type,value,true):value;
    }
    throw std::runtime_error("internal codegen: non-value name "+e.text);
  case Expr::Kind::Lambda:{
   const auto& lambda=lambdas.at(lambda_index.at(&e));std::string call=lambda.make+"(";
   for(std::size_t i=0;i<e.captures.size();++i){if(i)call+=", ";call+=ref(e.captures[i].name);}
   return temp(e.type,call+")");
  }
    case Expr::Kind::Tuple:case Expr::Kind::Record:case Expr::Kind::Array:{
     auto result=temp(e.type,"("+ctype(e.type)+"){0}");
     for(std::size_t i=0;i<e.children.size();++i){
      const auto& child=*e.children[i];
      auto value=expr(child);
      if(!needs_drop(child.type))value=temp(child.type,value);
      std::string place;
      if(e.kind==Expr::Kind::Array)place=result+".elements["+std::to_string(i)+"]";
      else if(e.kind==Expr::Kind::Record){
       auto pos=std::find(e.type.labels().begin(),e.type.labels().end(),e.labels[i]);
       place=result+".element_"+std::to_string(pos-e.type.labels().begin());
      }else place=result+".element_"+std::to_string(i);
      definitions<<pad()<<place<<" = "<<value<<";\n";
      clear_value(child.type,value);
     }
     return result;
    }
    case Expr::Kind::Variant:{
     auto result=temp(e.type,"("+ctype(e.type)+"){0}");
     const auto& child=*e.children[0];auto value=expr(child);
     if(!needs_drop(child.type))value=temp(child.type,value);
     definitions<<pad()<<result<<".payload.element_"<<e.projection<<" = "<<value<<";\n";
     clear_value(child.type,value);
     definitions<<pad()<<result<<".tag = INT64_C("<<e.projection<<");\n";
     return result;
    }
    case Expr::Kind::Field:{
     auto type=e.children[0]->type;auto root=expr(*e.children[0]);
     if(!needs_drop(type))root=temp(type,root);
     auto place=root+".element_"+std::to_string(e.projection);
     return e.move_out?transfer_temp(e.type,place):temp(e.type,place,e.type.has_functions());
    }
   case Expr::Kind::Index:case Expr::Kind::Slice:case Expr::Kind::Length:{
    auto t=e.children[0]->type;bool slice=t.kind()==Type::Slice;
     if(t==Type::RegexCaptures){
      auto root=temp(t,expr(*e.children[0]));
      if(e.kind==Expr::Kind::Length)return temp(Type::I64,"on1x_regex_count_v1("+root+")");
       auto index=temp(Type::I64,expr(*e.children[1]));
       (void)bounds.omit_check(e);  // Retained: regex captures have a different representation.
      return temp(Type::String,"on1x_regex_index_v1("+root+", "+index+", "+location(e.where)+")");
    }
    // Fixed arrays are rvalue-safe value structs; only slices need a materialized
    // owner copy for chained slicing. Evaluation order stays left-to-right.
    auto root=slice?temp(t,expr(*e.children[0])):expr(*e.children[0]);
    auto length=slice?root+".length":"INT64_C("+std::to_string(t.extent())+")";
    if(e.kind==Expr::Kind::Length)return temp(Type::I64,length);
    auto lo=temp(Type::I64,expr(*e.children[1]));
      auto at=[&]{return source_literal(e.where)+", "+std::to_string(e.where.line);};
     if(e.kind==Expr::Kind::Index){
       if(!bounds.omit_check(e))
       definitions<<pad()<<"on1x_bounds_index("<<lo<<", "<<length<<", "<<at()<<");\n";
      return temp(e.type,slice?root+".backing.elements["+root+".start + "+lo+"]":root+".elements["+lo+"]");
     }
     auto hi=temp(Type::I64,expr(*e.children[2]));
      if(!bounds.omit_check(e))
      definitions<<pad()<<"on1x_bounds_slice("<<lo<<", "<<hi<<", "<<length<<", "<<at()<<");\n";
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
     if(builtin&&callee.text=="format")return format_value(e,true);
     if(builtin)if(const auto* operation=shared_intrinsic(callee.text))
      return emit_shared_call(*operation,e);
    if(builtin&&callee.text=="move")return expr(*e.children.at(0));
    if(builtin&&callee.text=="box"){
     const auto& child=*e.children.at(0);
     auto source=move_source(child.type,expr(child));
     auto destroy=unique_destructor(e.type,e.where);
     auto result=temp(e.type,"on1x_unique_try_box(&("+source+"), sizeof("+source+"), "+destroy+")");
     allocation_failure(result,e.where);
     return result;
    }
    if(builtin&&callee.text=="take"){
     auto owner=expr(*e.children.at(0));
     auto result=temp(e.type,"("+ctype(e.type)+"){0}");
     definitions<<pad()<<"on1x_unique_take(&("<<owner<<"), &("<<result<<"), sizeof("<<result<<"), "<<location(e.where)<<");\n";
     return result;
    }
    if(builtin&&callee.text=="drop"){
     const auto& child=*e.children.at(0);auto value=expr(child);
     adjust(definitions,child.type,value,false,pad());
     clear_value(child.type,value);
     return {};
    }
    if(builtin&&callee.text=="ref_replace"){
     const auto type=ctype(e.type);
     auto slot=unique("replace_place");
     definitions<<pad()<<type<<" *"<<slot<<" = &("<<ref(e.children[0]->text)<<");\n";
     auto next=expr(*e.children[1]);
     // Only touch the old value after the replacement completes. The
     // previous slot's ownership transfers to a rooted expression temporary.
     auto previous=temp(e.type,"*"+slot);
     if(!e.type.is_move_only())retain(e.type,next);
     definitions<<pad()<<"*"<<slot<<" = "<<next<<";\n";
     if(e.type.is_move_only())clear_value(e.type,next);
     return previous;
    }
     const auto* borrower=callee.reference==Expr::Reference::BorrowFunction?borrow_functions.at(callee.text):nullptr;
     const bool foreign=callee.reference==Expr::Reference::Extern||callee.reference==Expr::Reference::UnsafeForeign||
                        callee.reference==Expr::Reference::ScalarForeign;
     const auto ipc=callee.reference==Expr::Reference::UnsafeForeign?
      std::string_view(externs.at(callee.text)->kernel_symbol):std::string_view{};
    bool direct=builtin||foreign||borrower;
    auto target=(foreign||borrower)?functions.at(callee.text):direct?callee.text:expr(callee);
     std::vector<std::string> args;
     std::vector<Owner> moved_args;
     for(std::size_t i=0;i<e.children.size();++i){
     const auto& child=e.children[i];
      if(i==0&&ipc_kernel_session_loan(ipc)){
       // The verified direct call holds an exclusive loan, not a transferred
       // box. Snapshot the pointer in source order; do not track it as an owner.
       auto loan=unique("ipc_loan");
       definitions<<pad()<<"on1x_unique *const "<<loan<<" = "<<ref(child->text)<<";\n";
       args.push_back(std::move(loan));
       continue;
      }
     if(borrower&&borrower->params[i].borrow!=BorrowMode::Value){
      // The verifier proved this is an initialized lexical place, and that
      // later arguments cannot conflict with it. No temporary or copy-back.
      args.push_back(borrow_address(*child,borrower->params[i].type,
                                    child->where));
      continue;
     }
     auto value=expr(*child);
     if(!needs_drop(child->type))value=temp(child->type,value);
     if(callee.reference==Expr::Reference::Extern&&externs.at(callee.text)->params[i].pattern)
      flatten(*externs.at(callee.text)->params[i].pattern,child->type,value,args);
      else args.push_back(value);
      if(child->type.is_move_only())
       moved_args.push_back({child->type,value,{}});
     }
     if(uses_group_deadlines)cancellation_edge(e.where);
      if(ipc_kernel_symbol(ipc)){
       const auto result=temp(e.type,"{0}");
       const auto out=unique("ipc_out"),status=unique("ipc_status");
       const bool opening=ipc_kernel_opening(ipc);
       const bool transfers=ipc==ipc_kernel_roundtrip;
        const bool output=opening||transfers||ipc==ipc_kernel_poll;
        const bool place_scratch=scratch_stack_bytes&&(transfers||ipc==ipc_kernel_poll);
        const bool heap=place_scratch&&*scratch_stack_bytes<memgraph::scalar_cell_bytes;
        std::string out_address="&"+out,out_value=out;
        if(heap){
         use_heap_cells();
         definitions<<pad()<<"/* MEMGraph: heap (call-scoped i64 scratch) */\n"
                    <<pad()<<heap_cell_type<<" "<<out<<";\n"
                    <<pad()<<heap_cell_type<<"_alloc(&"<<out<<", INT64_C(0), "<<location(e.where)<<");\n";
         out_address=heap_cell_type+"_ptr(&"+out+")";
         out_value="*"+out_address;
        }else if(output){
         if(place_scratch)definitions<<pad()<<"/* MEMGraph: stack (call-scoped i64 scratch) */\n";
         definitions<<pad()<<(opening?"on1x_unique *":"int64_t ")<<out
                    <<(opening?" = NULL;\n":" = 0;\n");
        }
        scratch_layout_used|=place_scratch;
       if(transfers)disarm_temporary(args.at(0));
       definitions<<pad()<<"int64_t "<<status<<" = "<<target<<"(";
       if(transfers){
        // The adapter clears the source slot and destroys its owner BEFORE
        // invoking On1x. The callback environment remains rooted by expr().
        definitions<<"&"<<args[0]<<", "<<args[1]<<", "<<args[2]<<", "
                   <<args[3]<<".invoke, "<<args[3]<<".environment, ";
       }else if(ipc_kernel_session_loan(ipc)){
        definitions<<args[0];
        if(ipc==ipc_kernel_register)
         definitions<<", "<<args[1]<<".invoke, "<<args[1]<<".environment";
        else for(std::size_t i=1;i<args.size();++i)definitions<<", "<<args[i];
        if(output)definitions<<", ";
       }
        if(output)definitions<<out_address;
       definitions<<");\n"
                  <<pad()<<"if ("<<status<<" == 0) {\n"
                   <<pad()<<"  "<<result<<".payload.element_1 = "<<(output?out_value:"INT64_C(0)")<<";\n"
                 <<pad()<<"  "<<result<<".tag = INT64_C(1);\n"
                 <<pad()<<"} else {\n"
                 <<pad()<<"  "<<result<<".payload.element_0 = "<<status<<";\n"
                 <<pad()<<"}\n";
       if(heap)definitions<<pad()<<"free("<<out<<".data);\n";
       return result;
     }
    if((callee.reference==Expr::Reference::ScalarForeign||callee.reference==Expr::Reference::UnsafeForeign)&&
       clamp_interval_symbol(externs.at(callee.text)->kernel_symbol)){
     // All arguments are already evaluated once, in source order. Enforce
     // the canonical C precondition before entering either foreign path.
     definitions<<pad()<<"on1x_assert("<<args.at(1)<<" <= "<<args.at(2)<<");\n";
    }
    if(callee.reference==Expr::Reference::ScalarForeign&&pricing_kernel_symbol(externs.at(callee.text)->kernel_symbol)){
     const bool candidates=externs.at(callee.text)->kernel_symbol=="mulib_priceopt_candidates";
     std::vector<std::string> wire;
     if(candidates){
      for(std::size_t i=0;i<2;++i){
       const auto& slice=args[i];
       wire.insert(wire.end(),{slice+".backing.elements","INT64_C("+std::to_string(e.children[i]->type.extent())+")",
                               slice+".start",slice+".length"});
      }
     }else{
      // Private wire follows lexicographic nominal field order. It is not
      // the MuLIB record layout; the adapter constructs actual C records.
      const auto& m=args[0];
      wire.insert(wire.end(),{m+".element_0.tag",m+".element_1",m+".element_2",m+".element_3"});
     }
     const auto& limits=args[candidates?2:1];
     for(unsigned i=0;i<7;++i)wire.push_back(limits+".element_"+std::to_string(i));
     wire.push_back(args.back()+".tag");
     const auto result=unique("decision"), demand=unique("demand"), price=unique("price");
     const auto score=unique("score"), count=unique("evaluations");
     definitions<<pad()<<"double "<<demand<<", "<<price<<", "<<score<<";\n"
                <<pad()<<"uint64_t "<<count<<";\n"<<pad()<<ctype(e.type)<<" "<<result<<" = {.tag = INT64_C(0)};\n"
                <<pad()<<"if ("<<target<<"(";
     for(const auto& arg:wire)definitions<<arg<<", ";
     definitions<<"&"<<demand<<", &"<<price<<", &"<<score<<", &"<<count<<")) {\n";
     unsigned index=0;
     for(const auto& field:{count,demand,price,score})
      definitions<<pad()<<"  "<<result<<".payload.element_1.element_"<<index++<<" = "<<field<<";\n";
     definitions<<pad()<<"  "<<result<<".tag = INT64_C(1);\n"<<pad()<<"}\n";
     return result;
    }
    if(callee.reference==Expr::Reference::ScalarForeign&&history_kernel_symbol(externs.at(callee.text)->kernel_symbol)){
     const auto& slice=args.at(0);
     const auto ty=ctype(e.type), result=unique("forecast"), point=unique("point"), variance=unique("variance");
     definitions<<pad()<<"double "<<point<<", "<<variance<<";\n"
                <<pad()<<ty<<" "<<result<<" = {.tag = INT64_C(0)};\n"
                <<pad()<<"if ("<<target<<"("<<slice<<".backing.elements, INT64_C("<<e.children[0]->type.extent()
                <<"), "<<slice<<".start, "<<slice<<".length";
     for (std::size_t i=1;i<args.size();++i)definitions<<", "<<args[i];
     definitions<<", &"<<point<<", &"<<variance<<")) {\n"
                <<pad()<<"  "<<result<<".payload.element_1.element_0 = "<<point<<";\n"
                <<pad()<<"  "<<result<<".payload.element_1.element_1 = "<<variance<<";\n"
                <<pad()<<"  "<<result<<".tag = INT64_C(1);\n"<<pad()<<"}\n";
     return result;
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
    for(const auto& moved:moved_args)disarm_temporary(moved.name);
    auto result=temp(e.type,call+")");
    for(std::size_t i=moved_args.size();i>0;--i)
     clear_value(moved_args[i-1].type,moved_args[i-1].name);
    return result;
   }
  }
  throw std::runtime_error("internal bad expression");
 }
 std::string format_value(const Expr& e,bool explicit_format){
  // Evaluate each operand into an ordinary C statement, never a C argument
  // list. Borrowed string values outlive all subsequent operand evaluations.
  std::string pattern=explicit_format?temp(Type::String,expr(*e.children.at(0))):"NULL";
  const std::size_t first=explicit_format?1:0,count=e.children.size()-first;
  // Unary format is the identity on already-interpolated text. In particular,
  // braces contributed by a $name substitution are not interpreted again.
  if(explicit_format&&!count){
   if(uses_group_deadlines)cancellation_edge(e.where);
   return pattern;
  }
  std::string parts=count?unique("format_parts"):"NULL";
  if(count)definitions<<pad()<<"on1x_format_value "<<parts<<"["<<count<<"];\n";
  for(std::size_t i=first;i<e.children.size();++i){
   const auto& child=*e.children[i];
   const auto value=expr(child);
   std::string kind,field;
   switch(child.type.kind()){
    case Type::String:kind="TEXT";field="text";break;
    case Type::Bool:kind="BOOL";field="truth";break;
    case Type::I64:kind="I64";field="i";break;
    case Type::U8:case Type::U16:case Type::U32:case Type::U64:kind="U64";field="u";break;
    case Type::F32:kind="F32";field="f";break;
    case Type::F64:kind="F64";field="f";break;
    case Type::Char:kind="CHAR";field="cp";break;
    default:throw Error(child.where,"invalid C11 format operand");
   }
   definitions<<pad()<<parts<<"["<<(i-first)<<"] = (on1x_format_value){.kind=ON1X_FMT_"
              <<kind<<", .value."<<field<<"="<<value<<"};\n";
  }
  if(uses_group_deadlines)cancellation_edge(e.where);
  return temp(Type::String,"on1x_format("+pattern+", "+std::to_string(count)+", "+parts+")");
 }
 bool body(const std::vector<Stmt>& statements){
  push();bool terminated=false;for(const auto& statement:statements)terminated=stmt(statement);
  pop(!terminated);return terminated;
 }
 void bind_name(const std::string& binding,const Type& type,const std::string& value){
   auto name=unique(binding.starts_with("@")?"specialization":binding);scopes.back()[binding]=name;
   definitions<<pad()<<ctype(type)<<" "<<name<<" = "<<value<<";\n"<<pad()<<"(void)"<<name<<";\n";
   if(type.is_move_only())clear_value(type,value);else retain(type,name);
   if(type==Type::Json)owners.back().push_back({type,name,{}});
   else if(needs_drop(type))owners.back().push_back(track(type,name));
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
  if(!origins)return emit_stmt(s);
  const auto begin=static_cast<std::size_t>(definitions.tellp());
  auto old=enclosing;
  enclosing=&s.where;body_origins[begin]=enclosing;
  const bool terminated=emit_stmt(s);
  enclosing=old;body_origins[static_cast<std::size_t>(definitions.tellp())]=old;
  return terminated;
 }
 bool emit_stmt(const Stmt& s){
  if(s.prototype)return false;
    switch(s.kind){
    case Stmt::Kind::TaskGroup:throw Error(s.where,"unlowered taskgroup");
    case Stmt::Kind::CspScope:{
     std::string delay;
     if(s.value){
      auto mark=temporary_values.size();
      delay=temp(Type::I64,expr(*s.value));
      finish(mark);
      cancellation_edge(s.where);
     }
     definitions<<pad()<<"{\n";push();
     auto owner=unique("taskgroup");
     definitions<<pad()<<"on1x_csp_group *"<<owner<<" = "
                <<(s.value?"on1x_csp_group_enter_within(":"on1x_csp_group_enter(")
                <<(s.value?delay+", ":"")<<location(s.where)<<");\n";
      owners.back().push_back({Type::CspGroup,owner,{}});
     if(s.value)cancellation_edge(s.where);
     bool terminated=false;for(const auto& child:s.body)terminated=stmt(child);
     if(!terminated&&uses_group_deadlines)cancellation_edge(s.where);
     pop(!terminated);definitions<<pad()<<"}\n";
     if(!terminated&&uses_group_deadlines)cancellation_edge(s.where);
     return terminated;
    }
   case Stmt::Kind::Defer:throw Error(s.where,"unlowered defer");
   case Stmt::Kind::PushDefer:{
    const auto& e=*s.value;const auto& lambda=lambdas.at(lambda_index.at(&e));
     auto at=location(s.where);
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
    if(s.value->kind==Expr::Kind::Borrow){
     const auto& borrow=*s.value;
     auto name=unique(s.name);
     auto address=borrow_address(*borrow.children.at(0),borrow.type,
                                 borrow.where);
     scopes.back()[s.name]="(*"+name+")";
     definitions<<pad()<<ctype(borrow.type)
                <<(borrow.borrow==BorrowMode::Shared?" const *":" *")
                <<name<<" = "<<address
                <<";\n"<<pad()<<"(void)"<<name<<";\n";
     finish();return false;
    }
    auto value=expr(*s.value);
   if(s.pattern)bind_pattern(*s.pattern,s.value->type,value);else bind_name(s.name,s.value->type,value);
    finish();return false;
   }
    case Stmt::Kind::Storage:{
     auto name=unique("storage");scopes.back()[s.name]=name;definitions<<pad()<<ctype(s.declared)<<" "<<name<<" = {0};\n"<<pad()<<"(void)"<<name<<";\n";
     if(needs_drop(s.declared))owners.back().push_back(track(s.declared,name));
     return false;
    }
   case Stmt::Kind::Initialize:case Stmt::Kind::Assign:{
   auto value=expr(*s.value),name=ref(s.name);
   // Snapshot even trivially copied values. Besides preserving assignment
   // sequencing, this avoids a C compiler self-assignment warning for x = x.
    if(!needs_drop(s.value->type))value=temp(s.value->type,value);
    if(s.value->type.is_move_only()){
     adjust(definitions,s.value->type,name,false,pad());
    }else if(s.value->type.has_functions()){
     retain(s.value->type,value);adjust(definitions,s.value->type,name,false,pad());
    }
    definitions<<pad()<<name<<" = "<<value<<";\n";
    if(s.value->type.is_move_only())clear_value(s.value->type,value);
    finish();return false;
  }
  case Stmt::Kind::Return:{
    if(s.value){
     auto value=expr(*s.value);
     if(s.value->noreturn)return true;
     if(uses_group_deadlines)cancellation_edge(s.where);
      if(s.value->type!=Type::Void){
       auto result=unique("return");definitions<<pad()<<ctype(s.value->type)<<" "<<result<<" = "<<value<<";\n";
       if(s.value->type.is_move_only())clear_value(s.value->type,value);else retain(s.value->type,result);
       // A reached defer can fail an owning allocation. Keep the saved return
       // in the emergency registry until defers and lexical drops complete.
       auto returned=defer_stack.empty()?Owner{s.value->type,result,{}}:
                                        track(s.value->type,result);
       run_defers();finish();drops();
       if(!returned.cleanup.empty())
        definitions<<pad()<<"on1x_cleanup_pop(&"<<returned.cleanup<<");\n";
       leave_call();definitions<<pad()<<"return "<<result<<";\n";
       }else{run_defers();finish();drops();leave_call();definitions<<pad()<<"return;\n";}
     }else{
      if(uses_group_deadlines)cancellation_edge(s.where);
      run_defers();finish();drops();leave_call();definitions<<pad()<<"return;\n";
     }
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
      if(uses_csp){definitions<<pad()<<"on1x_csp_yield("<<location(s.where)<<");\n";cancellation_edge(s.where);}
    --indent;definitions<<pad()<<"}\n";
    if(break_targets.contains(s.control_target))definitions<<pad()<<done<<": ;\n";
    loop_targets.pop_back();return false;
   }
   case Stmt::Kind::Break:case Stmt::Kind::Continue:{
    auto target=std::find_if(loop_targets.rbegin(),loop_targets.rend(),[&](const auto& t){return t.id==s.control_target;});
    if(target==loop_targets.rend())throw Error(s.where,"ON1X-CONTROL: missing C11 loop target");
    finish();
    if(uses_group_deadlines)cancellation_edge(s.where);
    for(std::size_t i=owners.size();i>target->keep;--i)for(auto owner=owners[i-1].rbegin();owner!=owners[i-1].rend();++owner)release(*owner);
    if(uses_group_deadlines)cancellation_edge(s.where,target->keep);
    definitions<<pad()<<"goto "<<(s.kind==Stmt::Kind::Break?target->done:target->latch)<<";\n";return true;
   }
   case Stmt::Kind::Range:throw Error(s.where,"ON1X-CONTROL: unlowered range in C11 emitter");
   case Stmt::Kind::Region:{
    auto done=unique("region_exit");bool used=region_exit_targets.contains(s.control_target);region_targets.push_back({s.control_target,owners.size(),done,{}});
    definitions<<pad()<<"{\n";bool terminated=body(s.body);definitions<<pad()<<"}\n";region_targets.pop_back();if(used)definitions<<pad()<<done<<": ;\n";return terminated&&!used;
   }
   case Stmt::Kind::Leave:{
    auto target=std::find_if(region_targets.rbegin(),region_targets.rend(),[&](const auto& r){return r.id==s.control_target;});if(target==region_targets.rend())throw Error(s.where,"missing canonical region target");finish();
    if(uses_group_deadlines)cancellation_edge(s.where);
    for(std::size_t i=owners.size();i>target->keep;--i)for(auto owner=owners[i-1].rbegin();owner!=owners[i-1].rend();++owner)release(*owner);
    if(uses_group_deadlines)cancellation_edge(s.where,target->keep);
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
  for(std::size_t i=0;i<parameters.size();++i){
   if(i||environment)output<<", ";
     output<<ctype(parameters[i].type);
     if(parameters[i].borrow==BorrowMode::Shared)output<<" const *";
     else if(parameters[i].borrow==BorrowMode::Exclusive)output<<" *";
  }
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
  void async_support(const AsyncFrame& frame){
   std::size_t slots=0;
   types<<"struct "<<frame.type<<" {\n  size_t state;\n";
   for(const auto& s:frame.block->body)
    if(s.kind==Stmt::Kind::Let)types<<"  int64_t slot_"<<slots++<<";\n";
   types<<"};\n";
   definitions<<"static void "<<frame.drop<<"(void *raw) { on1x_async_frame_free_v1(raw); }\n"
              <<"static on1x_async_step "<<frame.resume<<"(void *raw) {\n"
              <<"  struct "<<frame.type<<" *frame = raw;\n  switch (frame->state) {\n"
              <<"  case 0: {\n";
   scopes.clear();owners.clear();temporary_values.clear();indent=1;push();
   slots=0;
   for(const auto& s:frame.block->body){
    if(origins)body_origins[static_cast<std::size_t>(definitions.tellp())]=&s.where;
    if(s.kind==Stmt::Kind::Let){
     auto rhs=expr(*s.value);
     auto slot="frame->slot_"+std::to_string(slots++);
     definitions<<pad()<<slot<<" = "<<rhs<<";\n";
     scopes.back().emplace(s.name,slot);
    }else if(s.kind==Stmt::Kind::Expr){
     const auto& sleep=*s.value;
     auto delay=expr(*sleep.children[0]);
     const auto& next=sleep.children[1]->text;
     definitions<<pad()<<"frame->state = "<<next<<";\n"
                <<pad()<<"return (on1x_async_step){ON1X_ASYNC_SLEEP_MS, {.milliseconds = "<<delay<<"}};\n"
                <<"  }\n  case "<<next<<": {\n";
    }else{
     auto value=expr(*s.value);
     definitions<<pad()<<"return (on1x_async_step){ON1X_ASYNC_RETURN_I64, {.i64 = "<<value<<"}};\n";
    }
   }
   pop(false);indent=0;
   definitions<<"  }\n  default: return (on1x_async_step){0};\n  }\n}\n\n";
   if(origins)body_origins[static_cast<std::size_t>(definitions.tellp())]=nullptr;
  }
  void definition(const std::string& name,const Type& result,const std::vector<Param>& parameters,
                  const std::vector<Stmt>& statements,const Location& at,const Lambda* lambda=nullptr,bool private_function=false){
   scopes.clear();owners.clear();temporary_values.clear();indent=0;current_result=result;
  if(lambda){scopes.emplace_back();owners.emplace_back();}
  push();definitions<<((lambda||private_function)?"static ":"")<<ctype(result)<<" "<<name<<"(";
  std::string context;
  if(lambda){context=unique("context");definitions<<"on1x_env *"<<context;}
  if(parameters.empty()&&!lambda)definitions<<"void";
  for(std::size_t i=0;i<parameters.size();++i){
   if(i||lambda)definitions<<", ";
   auto symbol=unique(parameters[i].pattern?"pattern_argument":parameters[i].name);
     const bool borrowed=parameters[i].borrow!=BorrowMode::Value;
     scopes.back()[parameters[i].name]=borrowed?"(*"+symbol+")":symbol;
     definitions<<ctype(parameters[i].type)
                <<(parameters[i].borrow==BorrowMode::Shared?" const *":
                   parameters[i].borrow==BorrowMode::Exclusive?" *":" ")
                <<symbol;
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
    for(const auto& parameter:parameters)
     if(parameter.borrow==BorrowMode::Value&&needs_drop(parameter.type)){
      const auto binding=ref(parameter.name);
      // Copyable managed arguments arrive borrowed. A by-value slot can
      // itself be lent exclusively, so the slot must own its reference.
      if(!parameter.type.is_move_only())retain(parameter.type,binding);
      owners.back().push_back(track(parameter.type,binding));
     }
    for(const auto& parameter:parameters)if(parameter.pattern)
     bind_pattern(*parameter.pattern,parameter.type,ref(parameter.name));
   cancellation_edge(at);
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
     explicit Generator(const Unit& source,const C11Options& options)
       :unit(source),catalog(source),
           bounds(options.memgraph_bounds,options.memgraph_report!=nullptr),
           scratch_stack_bytes(options.memgraph_stack_bytes),origins(options.origins),
          source_prefixes(options.source_prefixes){
     for(const auto& f:unit.functions)verify_kernel_shape(f);
   uses_csp=csp::present(unit);
      for(const auto& f:unit.functions)if(f.external&&f.kernel_symbol.empty())identifiers.insert(f.name);
     for(const auto& f:unit.functions)if(f.kernel_link_only)identifiers.insert("on1x_kernel_scalar_v1_"+f.kernel_symbol);
     for(const auto& f:unit.functions)if(ipc_kernel_symbol(f.kernel_symbol))identifiers.insert(f.kernel_symbol);
  for(const auto& f:unit.functions)if(f.external)externs.emplace(f.name,&f);
   for(const auto& f:unit.functions)
    if(!f.prototype&&takes_borrows(f))borrow_functions.emplace(f.name,&f);
      for(const auto& f:unit.functions)functions.emplace(f.name,ipc_kernel_symbol(f.kernel_symbol)?f.kernel_symbol:
       f.kernel_link_only?"on1x_kernel_scalar_v1_"+f.kernel_symbol:
       f.kernel_symbol.empty()&&f.external?f.name:reserve("on1x_fn_"+(f.name.starts_with("@")?f.name.substr(1):f.name)));
    for(const auto& declaration:unit.nominals)
     for(const auto& field:declaration.fields)
      if(field.contains_unique())uses_unique=true;
    for(const auto& f:unit.functions){
     if(f.result.contains_unique())uses_unique=true;
     for(const auto& parameter:f.params)
      if(parameter.type.contains_unique())uses_unique=true;
       if(!f.prototype)collect(f.body);
    }
     if(!options.allow_stackless_timers&&!async_frames.empty())
      throw Error(async_frames.front().block->where,
                  "stackless timer frames require the direct C11 runtime profile","ON1X-CSP-UNSUPPORTED");
     ownership_cleanup=uses_unique;
   }
     std::string bounds_report() const {return bounds.report();}
    std::string run(){
       std::string kernel_headers,ipc_header;
      for(const auto& f:unit.functions){
       if(f.prototype)continue;
      if(ipc_kernel_symbol(f.kernel_symbol)){
       ipc_header="#include <on1x_kernel_ipc.h>\n";
       continue;
      }
     if(f.kernel_link_only){
      prototypes<<"/* foreign: "<<f.kernel_symbol<<"; unresolved until KernelBind review and ABI gates */\n";
      if(history_kernel_symbol(f.kernel_symbol)){
       prototypes<<"extern bool "<<functions.at(f.name)<<"(const double *, int64_t, int64_t, int64_t, double, ";
       if(f.kernel_symbol=="mulib_histprice_holt")prototypes<<"double, double, ";
       prototypes<<"int64_t, double *, double *);\n";
      }else if(pricing_kernel_symbol(f.kernel_symbol)){
       prototypes<<"extern bool "<<functions.at(f.name)<<"(";
       if(f.kernel_symbol=="mulib_priceopt_candidates")
        prototypes<<"const double *, int64_t, int64_t, int64_t, const double *, int64_t, int64_t, int64_t, ";
       else prototypes<<"int64_t, double, double, double, ";
       prototypes<<"double, double, double, double, double, double, double, int64_t, double *, double *, double *, uint64_t *);\n";
      }else prototypes<<"extern int64_t "<<functions.at(f.name)<<"(int64_t, int64_t"
                      <<(clamp_interval_symbol(f.kernel_symbol)?", int64_t":"")<<");\n";
      continue;
     }
     if(!f.kernel_symbol.empty()){
     kernel_headers="#include <limits.h>\n#include <mulib-clamp.h>\n"
      "#if !defined(ON1X_KERNEL_ABI_V1) || ON1X_KERNEL_ABI_V1 != 1 || !defined(__linux__) || !defined(__x86_64__)\n"
      "#error \"KernelBind C requires the verified native Linux x86-64 build profile\"\n#endif\n"
      "_Static_assert(CHAR_BIT == 8 && sizeof(int64_t) == 8 && _Alignof(int64_t) == 8 && sizeof(void *) == 8, \"KernelBind target ABI drift\");\n";
      const auto& symbol=f.kernel_symbol;
      const bool interval=clamp_interval_symbol(symbol);
     prototypes<<"/* unsafe_foreign: "<<symbol<<" */\n"
      <<"_Static_assert(_Generic(&"<<symbol<<", int64_t (*)(int64_t, int64_t"<<(interval?", int64_t":"")
      <<"): 1, default: 0), \"KernelBind prototype drift\");\n"
      <<"static int64_t "<<functions.at(f.name)<<"(int64_t a, int64_t b"<<(interval?", int64_t c":"")
      <<") { return "<<symbol<<"(a, b"<<(interval?", c":"")<<"); }\n";
     continue;
    }
    if(origins)proto_origins[static_cast<std::size_t>(prototypes.tellp())]=&f.where;
    prototypes<<(f.external?"extern ":takes_borrows(f)?"static ":"")
              <<ctype(f.result)<<" "<<functions.at(f.name)<<"(";
   if(f.external){
    auto flattened=extern_parameter_types(f);if(flattened.empty())prototypes<<"void";
    for(std::size_t i=0;i<flattened.size();++i){if(i)prototypes<<", ";prototypes<<ctype(flattened[i]);}
   }else parameter_types(prototypes,f.params);
   prototypes<<");\n";
   if(origins)proto_origins[static_cast<std::size_t>(prototypes.tellp())]=nullptr;
  }
  for(const auto& lambda:lambdas)lambda_support(lambda);
  for(const auto& frame:async_frames)async_support(frame);
       for(const auto& f:unit.functions)if(!f.external&&!f.prototype)
      definition(functions.at(f.name),f.result,f.params,f.body,f.where,
                 nullptr,takes_borrows(f));
   for(const auto& lambda:lambdas){const auto& e=*lambda.expression;definition(lambda.invoke,e.result,e.parameters,e.body,e.where,&lambda);}
  for(const auto& f:unit.functions)if(auto found=adapters.find(f.name);found!=adapters.end())adapter(f,found->second);
   emit_ownership_helpers();
   emit_unique_destructors();
  const auto& main=*std::find_if(unit.functions.begin(),unit.functions.end(),[](const auto& f){return f.name=="main";});
  // Only programs using the std.environ argv adapter need the host vector.
  const bool passes_argv=externs.contains("on1x_environ_arg")||externs.contains("on1x_environ_argc");
  if(passes_argv)prototypes<<"extern void on1x_environ_init(int, char **);\n";
  const auto start_main=[&]{
   definitions<<(passes_argv?"int main(int argc, char **argv) { on1x_environ_init(argc, argv); ":"int main(void) { ");
  };
   if(uses_csp){
    auto entry=reserve("on1x_csp_entry");
    definitions<<"static int64_t "<<entry<<"(on1x_env *context) { (void)context; ";
    if(main.result==Type::Void)definitions<<functions.at("main")<<"(); return 0;";
    else definitions<<"return "<<functions.at("main")<<"();";
    definitions<<" }\n";
    start_main();
    definitions<<"return (int)on1x_csp_run_v5("<<entry<<", NULL, "<<location(main.where)<<"); }\n";
   }
   else {
    start_main();
    // Keep even unused private definitions warning-clean without exporting
    // an unverified C caller interface.
    for(const auto& [name,unused]:borrow_functions){(void)unused;definitions<<"(void)"<<functions.at(name)<<"; ";}
    if(main.result==Type::Void)definitions<<functions.at("main")<<"(); return 0; }\n";
    else definitions<<"return (int)"<<functions.at("main")<<"(); }\n";
   }
   const std::string float_guard=uses_f64?
    "#if !defined(ON1X_F64_ABI_VERSION) || ON1X_F64_ABI_VERSION != 1\n#error \"unsupported On1x f64 target or runtime ABI\"\n#endif\n"
    "#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) || defined(__NO_SIGNED_ZEROS__) || defined(__ASSOCIATIVE_MATH__) || defined(__RECIPROCAL_MATH__)\n#error \"On1x f64 forbids fast/finite-only math and relaxed IEEE modes\"\n#endif\n\n":"";
   std::string output="/* Generated by on1xc. Do not edit. */\n#include <on1x_runtime.h>\n#include <stdint.h>\n#include <stdbool.h>\n#include <string.h>\n"
    "#if !defined(ON1X_CLOSURE_ABI_VERSION) || ON1X_CLOSURE_ABI_VERSION != 1\n#error \"incompatible On1x closure runtime ABI\"\n#endif\n\n"
    "#if !defined(ON1X_DATA_ABI_VERSION) || ON1X_DATA_ABI_VERSION != 1\n#error \"incompatible On1x data runtime ABI\"\n#endif\n\n"
      +(uses_value_data?std::string("#if !defined(ON1X_VALUE_DATA_ABI_VERSION) || ON1X_VALUE_DATA_ABI_VERSION != 1\n#error \"incompatible On1x value data runtime ABI\"\n#endif\n\n"):std::string{})
       +(uses_unique?std::string("#if !defined(ON1X_UNIQUE_ABI_VERSION) || ON1X_UNIQUE_ABI_VERSION != 1\n#error \"incompatible On1x unique ownership runtime ABI\"\n#endif\n\n"):std::string{})
       +(uses_shared?std::string("#include <on1x_container_shared.h>\n#if ON1X_CONTAINER_SHARED_ABI_VERSION != 1\n#error \"incompatible On1x shared container runtime ABI\"\n#endif\n\n"):std::string{})
      +(uses_regex?std::string("#if !defined(ON1X_REGEX_ABI_VERSION) || ON1X_REGEX_ABI_VERSION != 1\n#error \"incompatible On1x regex runtime ABI\"\n#endif\n\n"):std::string{})
      +(uses_f32?std::string("#if !defined(ON1X_F32_ABI_VERSION) || ON1X_F32_ABI_VERSION != 1\n#error \"unsupported On1x f32 target or runtime ABI\"\n#endif\n"):std::string{})
      +(uses_unsigned?std::string("#if !defined(ON1X_UNSIGNED_ABI_VERSION) || ON1X_UNSIGNED_ABI_VERSION != 1\n#error \"incompatible On1x unsigned runtime ABI\"\n#endif\n"):std::string{})
       +(uses_defer?std::string("#if !defined(ON1X_DEFER_ABI_VERSION) || ON1X_DEFER_ABI_VERSION != 1\n#error \"incompatible On1x defer runtime ABI\"\n#endif\n"):std::string{})
          +(uses_csp?std::string("#include <on1x_concurrency.h>\n#if !defined(ON1X_CSP_ABI_VERSION) || ON1X_CSP_ABI_VERSION != 5\n#error \"unsupported On1x cooperative fiber target or runtime ABI\"\n#endif\n"):std::string{})
          +(async_frames.empty()?std::string{}:std::string("#include <on1x_async_frame.h>\n#if !defined(ON1X_ASYNC_FRAME_ABI_VERSION) || ON1X_ASYNC_FRAME_ABI_VERSION != 1\n#error \"incompatible On1x stackless frame runtime ABI\"\n#endif\n"))
            +kernel_headers+ipc_header+float_guard+types.str()+"\n";
    if(scratch_layout_used)output+=memgraph::c11_scratch_layout();
    if(!heap_cell_type.empty())output+=memgraph::c11_heap_cells(heap_cell_type);
   const auto proto_begin=output.size();
   output+=prototypes.str()+"\n"+value_helpers.str()+helpers.str();
   const auto body_begin=output.size();
   output+=definitions.str();
   if(origins){
    origins->clear();
    std::size_t line=1,pos=0;
    auto record=[&](const auto& entries,std::size_t base){
     for(const auto& [offset,origin]:entries){
      auto end=base+offset;
      while(pos<end){if(output[pos]=='\n')++line;++pos;}
      if(pos==0||output[pos-1]=='\n')(*origins)[line]=origin?*origin:Location{};
     }
    };
    record(proto_origins,proto_begin);record(body_origins,body_begin);
   }
   return output;
 }
};
}
std::string emit_c(const Unit& unit,const std::string& source_name,const C11Options& options){
  (void)source_name;
  if(options.memgraph_stack_bytes&&*options.memgraph_stack_bytes>memgraph::max_stack_bytes)
   throw std::invalid_argument("MEMGraph scratch stack byte budget exceeds 65536");
 for(const auto& function:unit.functions)verify_kernel_shape(function);
 extensions::verify_evidence(unit);inherent::verify_elaborated(unit);cliches::verify_elaborated(unit);
 canonical::verify_control(unit);meta::require_core(unit);canonical::verify_values(unit);
 verify_nominals(unit);csp::verify(unit,true);
  Generator generator(unit,options);
 auto output=generator.run();
  if(options.memgraph_report)*options.memgraph_report=generator.bounds_report();
 return output;
}
std::string emit_c(const Unit& unit,const std::string& source_name){
 return emit_c(unit,source_name,{});
}
}
