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
  std::vector<Type> elements{Type::I64,Type::Bool};
  auto pair=Type::tuple(elements);elements[0]=Type::String;
  require(pair.elements()[0]==Type::I64,"tuple type does not own its element storage");
  require(pair==Type::tuple({Type::I64,Type::Bool}),"tuple equality is not structural");
  require(pair!=Type::tuple({Type::Bool,Type::I64}),"tuple order was erased");
  require(pair!=Type::tuple({Type::I64,Type::Bool,Type::String}),"tuple arity was erased");
  require(pair!=first,"tuple and function type constructors were conflated");
  auto product=Type::tuple({pair,first});
  require(on1x::type_name(product)=="((i64, bool), fn(i64) -> i64)","incorrect nested tuple diagnostic");
  require(!pair.has_functions()&&product.has_functions()&&first.has_functions(),"incorrect value-ownership metadata");
  require(Type::tuple({Type::Hole,pair}).has_holes(),"tuple inference hole was missed");
  require(!product.has_holes(),"concrete tuple has an inference hole");
  require(Type::generic(1)!=Type::generic(2),"quantified type identities were erased");
  require(Type::function({Type::generic(1)},Type::I64).has_generics(),"generic type metadata was missed");
  auto universal=Type::for_all(1,Type::function({Type::bound(0)},Type::bound(0)));
  require(universal.has_functions()&&universal.has_foralls()&&!universal.has_holes(),"forall metadata was missed");
   require(on1x::forall_instance(universal,first)&&!on1x::forall_proof(universal,first),"monomorphic implementation passed as a universal proof");
  require(on1x::forall_proof(universal,Type::function({Type::generic(7)},Type::generic(7))),"rigid universal proof was rejected");
  std::map<Type,int> unique;
  unique.emplace(first,1);unique.emplace(same,2);unique.emplace(different,3);
  require(unique.size()==2,"equivalent function types have different ordering identities");
  std::vector<Type> types={Type::Invalid,Type::I64,Type::Bool,Type::String,Type::Void,
                          Type::Json,Type::JsonView,first,same,different,nested,
                           Type::function({},Type::Void),Type::function({first},Type::Bool),pair,product,
                            Type::tuple({Type::I64,Type::String}),Type::function({pair},product),
                            universal,Type::bound(0),Type::bound(1),Type::generic(1),Type::generic(2)};
  for(const auto& a:types)for(const auto& b:types) {
   require((a==b)==((a<=>b)==0),"inconsistent type equality/ordering");
   require((a<b)==(b>a),"asymmetric type ordering");
   for(const auto& c:types)
    require(!(a<b&&b<c)||a<c,"non-transitive type ordering");
  }
  bool rejected=false;
  try { Type invalid(Type::Function); } catch(const std::logic_error&) { rejected=true; }
  require(rejected,"signature-free function type was admitted");
  rejected=false;
  try { Type invalid(Type::Tuple); } catch(const std::logic_error&) { rejected=true; }
  require(rejected,"shape-free tuple type was admitted");
  for(const auto& invalid:std::vector<std::vector<Type>>{{},{Type::I64}}){
   rejected=false;try{(void)Type::tuple(invalid);}catch(const std::logic_error&){rejected=true;}
   require(rejected,"invalid tuple arity was admitted");
  }
  rejected=false;try{(void)pair.parameters();}catch(const std::logic_error&){rejected=true;}
  require(rejected,"tuple exposed function parameters");
  rejected=false;try{(void)pair.result();}catch(const std::logic_error&){rejected=true;}
  require(rejected,"tuple exposed a function result");
  rejected=false;try{(void)first.elements();}catch(const std::logic_error&){rejected=true;}
  require(rejected,"function exposed tuple elements");
  std::cout<<"Structural monotypes: PASS\n";
 } catch(const std::exception& error) {
  std::cerr<<error.what()<<'\n';return 1;
 }
}
