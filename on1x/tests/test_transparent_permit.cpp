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
}
