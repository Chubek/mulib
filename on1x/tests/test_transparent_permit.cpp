// Standalone source-side checks; requires neither the driver nor a stdlib.
#include <on1x/module_signature.hpp>
#include <stdexcept>
using namespace on1x;

int main(){
 ModuleSignature signature;
 signature.module="Acme.Vault";
 signature.seal=SignatureSeal::Transparent;
 signature.types.push_back({{"interface"},"Token",0,{}});
 Unit provider;
 NominalDeclaration record;
 record.name="Token";record.labels={"value"};record.fields={Type::I64};
 provider.nominals.push_back(record);
 auto permit=[&](const char* verify_path,const char* use_path){
  auto verified=verify_module_signature(signature,provider,*signature.module,verify_path);
  return admit_executable_opaque_signature(signature,verified,{&provider},*signature.module,use_path);
 };
 if(!permit("/left","/left").empty())throw std::runtime_error("transparent identity was sealed");
 auto rejects=[&](const char* id,const char* use_path){
  try{permit("/left",use_path);}catch(const Error& error){if(error.diagnostic_id==id)return;throw;}
  throw std::runtime_error("unexpected permit");
 };
 rejects("ON1X-INTERFACE-MODULE","/right");
 for(const Type& field:std::vector<Type>{Type::String,Type::named("Token"),Type::tuple({Type::I64,Type::Bool})}){
  provider.nominals.front().fields={field};rejects("ON1X-INTERFACE-UNSUPPORTED","/left");
 }
 // Every nested nominal must be an explicitly exported record from the same
 // provider. Equal shape or a hidden local helper is not sufficient.
 provider.nominals.front().fields={Type::named("Payload")};
 record.name="Payload";record.labels={"number"};record.fields={Type::I64};
 provider.nominals.push_back(record);
 rejects("ON1X-INTERFACE-UNSUPPORTED","/left");
 signature.types.push_back({{"interface"},"Payload",0,{}});
 if(!permit("/left","/left").empty())throw std::runtime_error("nested transparent identity was sealed");
 rejects("ON1X-INTERFACE-MODULE","/right");
 provider.nominals.back().fields={Type::String};
 rejects("ON1X-INTERFACE-UNSUPPORTED","/left");
 provider.nominals.back().fields={Type::named("Token")};
 rejects("ON1X-INTERFACE-UNSUPPORTED","/left");
 provider.nominals.back().fields={Type::I64};
 provider.nominals.front().fields={Type::named("Payload",{Type::Bool})};
 rejects("ON1X-INTERFACE-UNSUPPORTED","/left");
 provider.nominals.front().fields={Type::named("Payload")};
 provider.nominals.back().kind=Type::Sum;
 rejects("ON1X-INTERFACE-UNSUPPORTED","/left");
 // Expansion is bounded even for a well-formed acyclic declaration graph.
 signature.types.clear();provider.nominals.clear();
 for(std::size_t i=0;i<70;++i){
  record.name="Level"+std::to_string(i);record.kind=Type::Record;
  record.fields={i==69?Type::I64:Type::named("Level"+std::to_string(i+1))};
  provider.nominals.push_back(record);
  signature.types.push_back({{"interface"},record.name,0,{}});
 }
 rejects("ON1X-INTERFACE-LIMIT","/left");
}
