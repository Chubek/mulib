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
   require(on1x::type_name(Type::F64)=="f64"&&Type(Type::F64)!=Type::I64,"f64 was conflated with i64");
   require(!Type(Type::F64).has_functions()&&!Type(Type::F64).has_holes(),"f64 must be a trivial ground value");
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
   auto code=Type::code(Type::I64);
   require(code!=Type::function({},Type::I64)&&code!=Type::code(Type::Bool),"code type constructor/result was erased");
   require(on1x::type_name(Type::code(code))=="code<code<i64>>"&&code.has_functions(),"code type shape/ownership metadata was missed");
    require(Type::code(Type::Hole).has_holes()&&Type::code(Type::generic(2)).has_generics(),"code result variables were missed");
    require(Type(Type::SExpr).has_functions()&&Type::tuple({Type::SExpr,Type::Buffer}).has_functions(),"managed data ownership was missed");
    auto packet=Type::data(Type::Record,{Type::Bool,Type::U16},{"accepted","sequence"});
    auto descriptor=Type::channel(packet,2,3);
    auto wildcard_descriptor=Type::channel(packet);
    auto sender=Type::sender(packet),receiver=Type::receiver(packet);
    require(descriptor.is_csp()&&!descriptor.is_csp_endpoint()&&
            sender.is_csp_endpoint()&&receiver.is_csp_endpoint(),
            "directional CSP type classification was lost");
    require(descriptor.csp_payload()==packet&&descriptor.sender_cardinality()==2&&
            descriptor.receiver_cardinality()==3,
            "channel payload/cardinality shape was lost");
    require(descriptor!=wildcard_descriptor&&sender!=receiver&&
            sender==Type::sender(packet),
            "directional CSP structural equality was erased");
    require(on1x::type_name(descriptor)=="channel<record{accepted:bool, sequence:u16}>[2,3]"&&
            on1x::type_name(sender)=="sender<record{accepted:bool, sequence:u16}>",
            "typed CSP diagnostic spelling is incorrect");
    require(Type::sender(Type::Hole).has_holes()&&
            Type::receiver(Type::generic(9)).has_generics(),
            "CSP payload variable metadata was missed");
    auto universal=Type::for_all(1,Type::function({Type::bound(0)},Type::bound(0)));
    require(universal.has_functions()&&universal.has_foralls()&&!universal.has_holes(),"forall metadata was missed");
     require(on1x::forall_instance(universal,first)&&!on1x::forall_proof(universal,first),"monomorphic implementation passed as a universal proof");
    require(on1x::forall_proof(universal,Type::function({Type::generic(7)},Type::generic(7))),"rigid universal proof was rejected");
    auto open_argument=Type::named("Box",{Type::generic(9)});
    auto open_instance=Type::function({open_argument},open_argument);
    require(on1x::forall_open_instance(universal,open_instance)&&
            !on1x::forall_instance(universal,open_instance)&&
            !on1x::forall_proof(universal,open_instance),
            "open predicative forall matching was not kept distinct");
   using on1x::TypeFormOptions;
   using on1x::TypeFormStatus;
   auto form=on1x::check_type_form(Type::I64);
   require(form.status==TypeFormStatus::Valid,"ground type form was rejected");
   require(on1x::check_type_form(Type::Invalid).status==TypeFormStatus::Malformed,
           "invalid type sentinel was admitted");
   require(on1x::check_type_form(Type::function({Type::Invalid},Type::I64)).status==TypeFormStatus::Malformed,
           "nested invalid type sentinel was admitted");
   require(on1x::check_type_form(Type::Hole).status==TypeFormStatus::Malformed,
           "unresolved type hole was admitted");
   TypeFormOptions source_form;source_form.allow_holes=true;source_form.allow_named=true;
   require(static_cast<bool>(on1x::check_type_form(
             Type::function({Type::Hole},Type::named("T")),source_form)),
           "source annotation form rejected holes or names");
   require(on1x::check_type_form(Type::generic(4)).status==TypeFormStatus::Malformed,
           "raw inferred generic was admitted");
   TypeFormOptions prototype_form;prototype_form.allow_generics=true;
   require(static_cast<bool>(on1x::check_type_form(Type::generic(4),prototype_form)),
           "explicit prototype generic was rejected");
   require(on1x::check_type_form(Type::bound(0)).status==TypeFormStatus::Malformed,
           "free bound variable was admitted");
   TypeFormOptions bound_form;bound_form.bound_variables=1;
   require(on1x::check_type_form(Type::bound(0),bound_form)&&
           on1x::check_type_form(Type::bound(1),bound_form).status==TypeFormStatus::Malformed,
           "quantified-variable scope was not enforced");
    require(static_cast<bool>(on1x::check_type_form(universal)),
            "closed forall form was rejected");
    require(on1x::check_type_form(descriptor)&&
            on1x::check_type_form(sender)&&on1x::check_type_form(receiver),
            "well-formed structural CSP type was rejected");
    auto csp_universal=Type::for_all(
      1,Type::function({Type::sender(Type::bound(0))},
                       Type::receiver(Type::bound(0))));
    require(on1x::forall_instance(
              csp_universal,
              Type::function({Type::sender(Type::U32)},Type::receiver(Type::U32))),
            "forall matching did not traverse directional payloads");
   TypeFormOptions one_node;one_node.max_nodes=1;
   require(on1x::check_type_form(first,one_node).status==TypeFormStatus::Limit,
           "type-form node budget was not enforced");
   TypeFormOptions shallow;shallow.max_depth=0;
   require(on1x::check_type_form(Type::code(Type::I64),shallow).status==TypeFormStatus::Limit,
           "type-form depth budget was not enforced");
   require(!on1x::forall_instance(universal,Type::function({Type::Invalid},Type::Invalid)),
           "malformed forall instance was admitted");
   bool malformed_forall=false;
   try{(void)Type::for_all(1,Type::function({Type::bound(1)},Type::bound(0)));}
   catch(const std::logic_error&){malformed_forall=true;}
   require(malformed_forall,"out-of-range quantified variable was admitted");
   malformed_forall=false;
   try{(void)Type::for_all(1,Type::function({Type::generic(1)},Type::bound(0)));}
   catch(const std::logic_error&){malformed_forall=true;}
   require(malformed_forall,"free inferred generic in forall was admitted");
   malformed_forall=false;
   try{(void)Type::for_all(1,Type::function({Type::Invalid},Type::bound(0)));}
   catch(const std::logic_error&){malformed_forall=true;}
   require(malformed_forall,"nested invalid type in forall was admitted");
   malformed_forall=false;
   try{(void)Type::for_all(1,Type::function({universal},Type::bound(0)));}
   catch(const std::logic_error&){malformed_forall=true;}
   require(malformed_forall,"nested forall was admitted");
  std::map<Type,int> unique;
  unique.emplace(first,1);unique.emplace(same,2);unique.emplace(different,3);
  require(unique.size()==2,"equivalent function types have different ordering identities");
   std::vector<Type> types={Type::Invalid,Type::I64,Type::F64,Type::Bool,Type::String,Type::Void,
                          Type::Json,Type::JsonView,first,same,different,nested,
                           Type::function({},Type::Void),Type::function({first},Type::Bool),pair,product,
                            Type::tuple({Type::I64,Type::String}),Type::function({pair},product),
                             universal,csp_universal,Type::bound(0),Type::bound(1),
                             Type::generic(1),Type::generic(2),code,Type::code(code),
                             Type::SExpr,Type::Buffer,packet,descriptor,
                             wildcard_descriptor,sender,receiver};
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
   for(auto kind:{Type::Channel,Type::Sender,Type::Receiver}){
    rejected=false;try{Type invalid(kind);}catch(const std::logic_error&){rejected=true;}
    require(rejected,"shape-free CSP type was admitted");
   }
   rejected=false;
   try{(void)Type::channel(Type::I64,256,1);}
   catch(const std::logic_error&){rejected=true;}
   require(rejected,"oversized endpoint cardinality was admitted");
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
