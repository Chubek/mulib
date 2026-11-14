#include <on1x/type.hpp>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
 if(!condition)throw std::runtime_error(message);
}
}

int main() {
 try {
  using on1x::Type;
  std::vector<Type> parameters{Type::I64};
  auto first=Type::function(parameters,Type::I64);
  parameters[0]=Type::Bool;
  auto same=Type::function({Type::I64},Type::I64);
  auto different=Type::function({Type::Bool},Type::I64);
  require(first==same,"function equality must be structural");
  require(first!=different,"function parameter types were erased");
  require(first.parameters()[0]==Type::I64,"type does not own its parameter storage");
  auto nested=Type::function({first,Type::String},same);
  require(on1x::type_name(nested)=="fn(fn(i64) -> i64, string) -> fn(i64) -> i64",
          "incorrect structural type diagnostic");
  require(on1x::type_name(Type::function({},Type::Void))=="fn() -> void",
          "incorrect zero-argument function type");
  require(on1x::type_name(Type::Hole)=="_","incorrect inference-hole spelling");
  require(on1x::has_type_holes(Type::function({Type::Hole},Type::I64)),"parameter hole was missed");
  require(on1x::has_type_holes(Type::function({},Type::function({},Type::Hole))),"nested result hole was missed");
  require(!on1x::has_type_holes(nested),"concrete type incorrectly contains holes");
  std::map<Type,int> unique;
  unique.emplace(first,1);unique.emplace(same,2);unique.emplace(different,3);
  require(unique.size()==2,"equivalent function types have different ordering identities");
  std::vector<Type> types={Type::Invalid,Type::I64,Type::Bool,Type::String,Type::Void,
                          Type::Json,Type::JsonView,first,same,different,nested,
                          Type::function({},Type::Void),Type::function({first},Type::Bool)};
  for(const auto& a:types)for(const auto& b:types) {
   require((a==b)==((a<=>b)==0),"inconsistent type equality/ordering");
   require((a<b)==(b>a),"asymmetric type ordering");
   for(const auto& c:types)
    require(!(a<b&&b<c)||a<c,"non-transitive type ordering");
  }
  bool rejected=false;
  try { Type invalid(Type::Function); } catch(const std::logic_error&) { rejected=true; }
  require(rejected,"signature-free function type was admitted");
  std::cout<<"Structural monotypes: PASS\n";
 } catch(const std::exception& error) {
  std::cerr<<error.what()<<'\n';return 1;
 }
}
