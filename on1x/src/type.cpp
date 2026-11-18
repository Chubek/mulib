#include <on1x/type.hpp>
#include <stdexcept>
#include <utility>
#include <map>
#include <algorithm>

namespace on1x {
struct TypeShape { std::vector<Type> children; Type result; bool holes, functions, generics, foralls; std::vector<std::string> labels{}; std::size_t extent=0; std::string nominal{}; std::vector<Type> arguments{}; };

Type::Type(Kind kind):kind_(kind) {
  if(kind == Function || kind == Tuple || kind == Forall || kind == Code || kind==Named || kind==Own || is_data()) throw std::logic_error("composite type requires a shape");
}
Type::Type(Kind kind, std::shared_ptr<const TypeShape> shape)
  :kind_(kind),shape_(std::move(shape)) {}
Type Type::generic(std::size_t identity){Type type(Generic);type.variable_=identity;return type;}
Type Type::bound(std::size_t index){Type type(Bound);type.variable_=index;return type;}
Type Type::for_all(std::size_t variables,Type body){
 TypeFormOptions options;options.bound_variables=variables;options.allow_named=true;options.allow_foralls=false;
 auto form=check_type_form(body,options);
 if(form.status==TypeFormStatus::Limit)throw std::length_error(form.message);
 if(!variables||!body.is_function()||!form)throw std::logic_error("forall requires a closed, well-scoped function body");
 auto holes=body.has_holes(),generics=body.has_generics();
 Type type(Forall,std::make_shared<const TypeShape>(TypeShape{{},std::move(body),holes,true,generics,true}));
 type.variable_=variables;return type;
}
const Type& Type::body() const{if(!is_forall())throw std::logic_error("non-forall type has no body");return shape_->result;}
Type Type::code(Type body){
 auto holes=body.has_holes(),generics=body.has_generics(),foralls=body.has_foralls();
 return Type(Code,std::make_shared<const TypeShape>(TypeShape{{},std::move(body),holes,true,generics,foralls}));
}
const Type& Type::code_result() const{if(!is_code())throw std::logic_error("non-code type has no code result");return shape_->result;}
Type Type::function(std::vector<Type> parameters, Type result) {
 bool holes=result.has_holes(),generics=result.has_generics(),foralls=result.has_foralls();
 for(const auto& parameter:parameters){holes=holes||parameter.has_holes();generics=generics||parameter.has_generics();foralls=foralls||parameter.has_foralls();}
 return Type(Function,std::make_shared<const TypeShape>(TypeShape{std::move(parameters),std::move(result),holes,true,generics,foralls}));
}
Type Type::tuple(std::vector<Type> elements) {
 if(elements.size()<2)throw std::logic_error("tuple type requires at least two elements");
 bool holes=false,functions=false,generics=false,foralls=false;
 for(const auto& element:elements){holes=holes||element.has_holes();functions=functions||element.has_functions();generics=generics||element.has_generics();foralls=foralls||element.has_foralls();}
 return Type(Tuple,std::make_shared<const TypeShape>(TypeShape{std::move(elements),Type::Invalid,holes,functions,generics,foralls}));
}
Type Type::data(Kind kind,std::vector<Type> elements,std::vector<std::string> labels,std::size_t extent){
 if(kind!=Record&&kind!=Sum&&kind!=Array&&kind!=Slice)throw std::logic_error("invalid data type kind");
 if(kind==Record||kind==Sum){
  if(labels.size()!=elements.size()||labels.size()>256||(kind==Sum&&labels.empty()))throw std::logic_error("invalid labeled data shape");
  std::map<std::string,Type> ordered;
  for(std::size_t i=0;i<labels.size();++i)if(labels[i].empty()||!ordered.emplace(labels[i],elements[i]).second)throw std::logic_error("duplicate/empty data label");
  elements.clear();labels.clear();for(auto& [label,type]:ordered){labels.push_back(label);elements.push_back(type);}
  extent=0;
 }else if(elements.size()!=1||!labels.empty()||extent>256)throw std::logic_error("invalid array/slice extent or element arity");
 bool holes=false,functions=false,generics=false,foralls=false;
 for(const auto& element:elements){holes|=element.has_holes();functions|=element.has_functions();generics|=element.has_generics();foralls|=element.has_foralls();}
 return Type(kind,std::make_shared<const TypeShape>(TypeShape{std::move(elements),Type::Invalid,holes,functions,generics,foralls,std::move(labels),extent}));
}
const std::vector<std::string>& Type::labels() const{if(!is_data())throw std::logic_error("non-data type has no labels");return shape_->labels;}
Type Type::named(std::string name,std::vector<Type> arguments){
 if(name.empty())throw std::logic_error("empty named type reference");
 bool holes=false,functions=false,generics=false,foralls=false;
 for(const auto& a:arguments){holes|=a.has_holes();functions|=a.has_functions();generics|=a.has_generics();foralls|=a.has_foralls();}
  return Type(Named,std::make_shared<const TypeShape>(TypeShape{{},Type::Invalid,holes,functions,generics,foralls,{},0,std::move(name),std::move(arguments)}));
}
Type Type::own(Type target){
  if(target.kind()!=Named)throw std::logic_error("own target must be a named type reference");
  auto holes=target.has_holes(),generics=target.has_generics(),foralls=target.has_foralls();
  // The pointer itself is unique, not reference counted. Its target is dropped
  // by the generated owning-box destructor.
  return Type(Own,std::make_shared<const TypeShape>(TypeShape{{},std::move(target),holes,false,generics,foralls}));
}
Type Type::with_nominal(std::string name,std::vector<Type> arguments) const{
 if((kind_!=Record&&kind_!=Sum)||name.empty())throw std::logic_error("nominal identity requires a record/sum");
 auto shape=std::make_shared<TypeShape>(*shape_);shape->nominal=std::move(name);shape->arguments=std::move(arguments);
 for(const auto& a:shape->arguments){shape->holes|=a.has_holes();shape->generics|=a.has_generics();shape->foralls|=a.has_foralls();}
 return Type(kind_,std::move(shape));
}
const std::string& Type::nominal() const{static const std::string empty;return shape_?shape_->nominal:empty;}
const std::vector<Type>& Type::arguments() const{static const std::vector<Type> empty;return shape_?shape_->arguments:empty;}
const Type& Type::owned() const{if(!is_own())throw std::logic_error("non-own type has no target");return shape_->result;}
bool Type::has_names() const{
 if(kind_==Named)return true;
 if(!shape_)return false;
 for(const auto& c:shape_->children)if(c.has_names())return true;
 for(const auto& a:shape_->arguments)if(a.has_names())return true;
 return shape_->result.has_names();
}
std::size_t Type::extent() const{if(!is_data())throw std::logic_error("non-data type has no extent");return shape_->extent;}
bool Type::has_foralls() const{return shape_&&shape_->foralls;}
bool Type::has_generics() const{return kind_==Generic||(shape_&&shape_->generics);}
bool Type::has_holes() const { return kind_==Hole||(shape_&&shape_->holes); }
bool Type::has_functions() const { return is_reference()||(shape_&&shape_->functions); }
bool Type::is_move_only() const {
 if(is_own())return true;
 if(!is_tuple()&&!is_data())return false;
 for(const auto& field:elements())if(field.is_move_only())return true;
 return false;
}
bool Type::contains_unique() const {
 if(is_own())return true;
 if(!shape_)return false;
 for(const auto& child:shape_->children)if(child.contains_unique())return true;
 for(const auto& argument:shape_->arguments)if(argument.contains_unique())return true;
 return shape_->result.contains_unique();
}
const std::vector<Type>& Type::parameters() const {
 if(!is_function()) throw std::logic_error("non-function type has no parameters");
 return shape_->children;
}
const std::vector<Type>& Type::elements() const {
 if(!is_tuple()&&!is_data()) throw std::logic_error("non-product/data type has no elements");
 return shape_->children;
}
const Type& Type::result() const {
 if(!is_function()) throw std::logic_error("non-function type has no result");
 return shape_->result;
}
std::strong_ordering Type::operator<=>(const Type& other) const {
 if(auto order = kind_ <=> other.kind_; order != 0) return order;
 if(kind_==Generic||kind_==Bound||kind_==Forall){if(auto order=variable_ <=> other.variable_;order!=0)return order;}
 if(!shape_ || shape_ == other.shape_) return std::strong_ordering::equal;
 if(auto order=nominal()<=>other.nominal();order!=0)return order;
 if(auto order=arguments()<=>other.arguments();order!=0)return order;
 if(is_data()){
  if(auto order=extent()<=>other.extent();order!=0)return order;
  if(auto order=labels()<=>other.labels();order!=0)return order;
 }
 if(auto order = shape_->children <=> other.shape_->children; order != 0) return order;
  if(is_forall())return body() <=> other.body();
  if(is_code())return code_result() <=> other.code_result();
  if(is_own())return owned() <=> other.owned();
  return is_function()?result() <=> other.result():std::strong_ordering::equal;
}
bool Type::operator==(const Type& other) const { return (*this <=> other) == 0; }

TypeFormResult check_type_form(const Type& root,const TypeFormOptions& options){
 struct Item {
  const Type* type;
  std::size_t bound,depth;
  bool allow_generics,allow_foralls;
 };
 if(!options.max_nodes)
  return {TypeFormStatus::Limit,"type-form node budget exhausted",0};
 std::vector<Item> pending{{&root,options.bound_variables,0,
                            options.allow_generics,options.allow_foralls}};
 std::size_t nodes=0;
 auto malformed=[&](std::string message){
  return TypeFormResult{TypeFormStatus::Malformed,std::move(message),nodes};
 };
 auto limited=[&](std::string message){
  return TypeFormResult{TypeFormStatus::Limit,std::move(message),nodes};
 };
 auto add=[&](const Type& child,const Item& parent,bool generics,bool foralls){
  if(pending.size()>=options.max_nodes-nodes)return false;
  pending.push_back({&child,parent.bound,parent.depth+1,generics,foralls});
  return true;
 };
 while(!pending.empty()){
  auto item=pending.back();pending.pop_back();
  if(item.depth>options.max_depth)return limited("type-form depth budget exhausted");
  if(nodes++>=options.max_nodes)return limited("type-form node budget exhausted");
  const auto& type=*item.type;
  if(type.kind()==Type::Invalid)return malformed("invalid type sentinel");
  if(type.kind()==Type::Hole&&!options.allow_holes)return malformed("unresolved inference hole");
  if(type.kind()==Type::Generic&&!item.allow_generics)return malformed("free inferred type variable");
  if(type.kind()==Type::Bound){
   if(type.generic_identity()>=item.bound)return malformed("unbound quantified type variable");
   continue;
  }
   if(type.kind()==Type::Named&&!options.allow_named)return malformed("unresolved named type");
   if(type.is_own()){
    const auto& target=type.owned();
    if(target.kind()!=Type::Named)return malformed("own target is not a named type");
    // The nominal target identity is the finite checked representation of an
    // owning edge. Its arguments are ordinary type forms and must still pass
    // the current trust-boundary policy.
     for(const auto& argument:target.arguments())
      if(!add(argument,item,item.allow_generics,item.allow_foralls))
       return limited("type-form node budget exhausted");
    continue;
   }
   if(type.is_forall()){
   if(!item.allow_foralls||!type.generic_identity()||!type.body().is_function())
    return malformed("invalid or nested forall type");
   if(type.generic_identity()>options.max_nodes)
    return limited("type-form quantified-variable budget exhausted");
   Item binder=item;binder.bound=type.generic_identity();
   if(!add(type.body(),binder,false,false))
    return limited("type-form node budget exhausted");
   continue;
  }
  for(const auto& argument:type.arguments())
   if(!add(argument,item,item.allow_generics,item.allow_foralls))
    return limited("type-form node budget exhausted");
  if(type.is_function()){
   for(const auto& parameter:type.parameters())
    if(!add(parameter,item,item.allow_generics,item.allow_foralls))
     return limited("type-form node budget exhausted");
   if(!add(type.result(),item,item.allow_generics,item.allow_foralls))
    return limited("type-form node budget exhausted");
  }else if(type.is_code()){
   if(!add(type.code_result(),item,item.allow_generics,item.allow_foralls))
    return limited("type-form node budget exhausted");
   }else if(type.is_tuple()||type.is_data()){
   for(const auto& element:type.elements())
    if(!add(element,item,item.allow_generics,item.allow_foralls))
     return limited("type-form node budget exhausted");
  }
 }
 return {TypeFormStatus::Valid,{},nodes};
}

std::string type_name(const Type& type) {
 if(!type.nominal().empty()){
  std::string name=type.nominal();
  if(!type.arguments().empty()){name+="<";for(std::size_t i=0;i<type.arguments().size();++i){if(i)name+=", ";name+=type_name(type.arguments()[i]);}name+=">";}
  return name;
 }
  switch(type.kind()) {
  case Type::Fiber:return "ifiber<i64>";case Type::Channel:return "channel<i64>";
  case Type::Sender:return "sender<i64>";case Type::Receiver:return "receiver<i64>";
   case Type::CspGroup:return "<taskgroup>";case Type::ExternalFiber:return "xfiber";
   case Type::ByteSender:return "sender<u8>";case Type::ByteReceiver:return "receiver<u8>";
  case Type::I64:return "i64";
   case Type::F64:return "f64";
   case Type::F32:return "f32";
   case Type::U8:return "u8";case Type::U16:return "u16";
   case Type::U32:return "u32";case Type::U64:return "u64";
 case Type::Bool:return "bool";
 case Type::String:return "string";case Type::Char:return "char";
 case Type::Regex:return "regex";
 case Type::RegexCaptures:return "captures";
 case Type::Void:return "void";
 case Type::Json:return "json";
 case Type::JsonView:return "json_view";
  case Type::SExpr:return "sexpr";case Type::Buffer:return "buffer";
  case Type::Own:return "own<"+type_name(type.owned())+">";
 case Type::Code:return "code<"+type_name(type.code_result())+">";
 case Type::Hole:return "_";
 case Type::Generic:return "'a"+std::to_string(type.generic_identity());
 case Type::Bound:return "a"+std::to_string(type.generic_identity());
 case Type::Forall:{
  std::string name="forall(";
  for(std::size_t i=0;i<type.generic_identity();++i){if(i)name+=", ";name+="a"+std::to_string(i);}
  return name+") "+type_name(type.body());
 }
  case Type::Tuple:{
  std::string name="(";
  for(std::size_t i=0;i<type.elements().size();++i){
   if(i)name+=", ";
   name+=type_name(type.elements()[i]);
  }
   return name+")";
  }
   case Type::Array:case Type::Slice:return std::string(type.kind()==Type::Array?"array<":"slice<")+type_name(type.elements()[0])+", "+std::to_string(type.extent())+">";
  case Type::Record:case Type::Sum:{
   std::string name=type.kind()==Type::Record?"record{":"sum{";
   for(std::size_t i=0;i<type.elements().size();++i){if(i)name+=", ";name+=type.labels()[i]+":"+type_name(type.elements()[i]);}
   return name+"}";
  }
  case Type::Function:{
  std::string name = "fn(";
  for(std::size_t i=0;i<type.parameters().size();++i) {
   if(i) name += ", ";
   name += type_name(type.parameters()[i]);
  }
  return name + ") -> " + type_name(type.result());
 }
 default:return "<invalid>";
 }
}
bool has_type_holes(const Type& type) {
 return type.has_holes();
}
namespace {
enum class ForallMatch { Concrete, Open, Rigid };
bool match_forall(const Type& schema,const Type& instance,ForallMatch mode){
 if(!schema.is_forall())return false;
 TypeFormOptions schema_options;schema_options.allow_named=true;
 if(!check_type_form(schema,schema_options))return false;
 TypeFormOptions instance_options;
 instance_options.allow_generics=mode!=ForallMatch::Concrete;
 instance_options.allow_named=true;instance_options.allow_foralls=false;
 if(!check_type_form(instance,instance_options))return false;
 std::map<std::size_t,Type> substitutions;
 auto match=[&](auto&& self,const Type& pattern,const Type& actual)->bool{
  if(pattern.kind()==Type::Bound){
   if(pattern.generic_identity()>=schema.generic_identity()||actual.has_foralls()||actual.has_holes())return false;
   if(mode==ForallMatch::Rigid){
    if(actual.kind()!=Type::Generic)return false;
    for(const auto& [variable,type]:substitutions)if(variable!=pattern.generic_identity()&&type==actual)return false;
   }
   auto [found,inserted]=substitutions.emplace(pattern.generic_identity(),actual);
   return inserted||found->second==actual;
  }
    if(pattern.kind()!=actual.kind()||pattern.nominal()!=actual.nominal()||pattern.arguments().size()!=actual.arguments().size())return false;
    for(std::size_t i=0;i<pattern.arguments().size();++i)if(!self(self,pattern.arguments()[i],actual.arguments()[i]))return false;
    if(pattern.is_code())return self(self,pattern.code_result(),actual.code_result());
    if(pattern.is_own())return self(self,pattern.owned(),actual.owned());
  if(pattern.is_function()){
   if(pattern.parameters().size()!=actual.parameters().size()||!self(self,pattern.result(),actual.result()))return false;
   for(std::size_t i=0;i<pattern.parameters().size();++i)if(!self(self,pattern.parameters()[i],actual.parameters()[i]))return false;
   return true;
  }
   if(pattern.is_tuple()||pattern.is_data()){
    if(pattern.is_data()&&(pattern.labels()!=actual.labels()||pattern.extent()!=actual.extent()))return false;
   if(pattern.elements().size()!=actual.elements().size())return false;
   for(std::size_t i=0;i<pattern.elements().size();++i)if(!self(self,pattern.elements()[i],actual.elements()[i]))return false;
   return true;
  }
  return pattern==actual;
 };
 return match(match,schema.body(),instance);
}
}
bool forall_instance(const Type& schema,const Type& instance){
 return match_forall(schema,instance,ForallMatch::Concrete);
}
bool forall_open_instance(const Type& schema,const Type& instance){
 return match_forall(schema,instance,ForallMatch::Open);
}
bool forall_proof(const Type& schema,const Type& instance){
 return match_forall(schema,instance,ForallMatch::Rigid);
}
}
