#include <on1x/type.hpp>
#include <stdexcept>
#include <utility>

namespace on1x {
struct FunctionType { std::vector<Type> parameters; Type result; bool holes; };

Type::Type(Kind kind):kind_(kind) {
 if(kind == Function) throw std::logic_error("function type requires a signature");
}
Type::Type(std::shared_ptr<const FunctionType> signature)
 :kind_(Function),signature_(std::move(signature)) {}
Type Type::function(std::vector<Type> parameters, Type result) {
 bool holes=result.has_holes();
 for(const auto& parameter:parameters)holes=holes||parameter.has_holes();
 return Type(std::make_shared<const FunctionType>(FunctionType{std::move(parameters),std::move(result),holes}));
}
bool Type::has_holes() const { return kind_==Hole||(signature_&&signature_->holes); }
const std::vector<Type>& Type::parameters() const {
 if(!signature_) throw std::logic_error("non-function type has no parameters");
 return signature_->parameters;
}
const Type& Type::result() const {
 if(!signature_) throw std::logic_error("non-function type has no result");
 return signature_->result;
}
std::strong_ordering Type::operator<=>(const Type& other) const {
 if(auto order = kind_ <=> other.kind_; order != 0) return order;
 if(!is_function() || signature_ == other.signature_) return std::strong_ordering::equal;
 if(auto order = parameters() <=> other.parameters(); order != 0) return order;
 return result() <=> other.result();
}
bool Type::operator==(const Type& other) const { return (*this <=> other) == 0; }

std::string type_name(const Type& type) {
 switch(type.kind()) {
 case Type::I64:return "i64";
 case Type::Bool:return "bool";
 case Type::String:return "string";
 case Type::Void:return "void";
 case Type::Json:return "json";
 case Type::JsonView:return "json_view";
 case Type::Hole:return "_";
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
}
