#include <on1x/clone.hpp>
#include <on1x/meta.hpp>
#include "../src/ppx_internal.hpp"
#include <iostream>

using namespace on1x;
using namespace on1x::meta;

namespace {
void require(bool result,const char* message){if(!result)throw std::runtime_error(message);}
template<class Test>
void reject(Test test,std::string_view diagnostic){
 try{test();}
 catch(const Error& error){require(std::string_view(error.what()).find(diagnostic)!=std::string_view::npos,error.what());return;}
 throw std::runtime_error("expected PPX rejection: "+std::string(diagnostic));
}
Unit copy(const Unit& unit){std::size_t budget=262144;return clone_unit(unit,budget);}
}

int main(){
 try{
  const Location source{"/source/acme/main.on1x",11,2,1};
  auto unit=parse(scan("fn main(){} ",source.file));
  const auto legacy=write_sexpr(encode(unit));
  unit.module=ModuleHeader{source,"acme.tools"};
  unit.functions[0].visibility={DeclarationVisibility::Shared,1};
  Import request;
  request.where=source;request.kind=Import::Kind::Program;request.module="acme.other";
  request.names={{"value","renamed",false},{"other","",false}};request.selective=true;
  unit.module_imports.push_back(request);
  request.quoted=true;request.module="legacy.on1x";request.selective=false;request.names.clear();
  unit.module_imports.push_back(request);
  request.kind=Import::Kind::Module;request.quoted=false;request.module="acme.ops";
  request.relative=1;request.selective=true;request.names={{"%%","",true}};
  unit.module_imports.push_back(request);
  auto packet=encode_source_module_metadata(unit);
  require(write_sexpr(packet)==
   "(SourceModuleMetadata 1 (SourceModuleHeader (\"acme\" \"tools\") (Loc \"/source/acme/main.on1x\" 11 2 1)) "
   "((SourceProgramImport (SourceNamedTarget (\"acme\" \"other\")) true (SourceNoAlias) "
   "((SourceValueSelector \"value\" (SourceAlias \"renamed\")) (SourceValueSelector \"other\" (SourceNoAlias))) "
   "(Loc \"/source/acme/main.on1x\" 11 2 1)) "
   "(SourceProgramImport (SourceQuotedTarget \"legacy.on1x\") false (SourceNoAlias) () "
   "(Loc \"/source/acme/main.on1x\" 11 2 1)) "
   "(SourceModuleImport (SourceNamedTarget (\"acme\" \"ops\")) 1 true (SourceNoAlias) ((SourceOperatorSelector \"%%\")) "
   "(Loc \"/source/acme/main.on1x\" 11 2 1))) "
   "((SourceDeclarationVisibility (SourceFunctionKind) \"main\" (SourceShared 1) (Loc \"/source/acme/main.on1x\" 0 1 1))))\n",
   "source metadata must contain spellings, selections and 1-based visibility");
  require(write_sexpr(encode(unit))==legacy,"module metadata changed the Unit-v1 wire");
  reject([&]{(void)decode(packet);},"expected Unit");
  auto invalid=packet;invalid.items[4].items[0].items[3].items[1]=symbol("0");
  reject([&]{base_schema().validate(invalid,"SourceModuleMetadata");},"expected Positive");

  PpxOptions options;
  options.programs={PpxRegistration{{"<test-ppx>"},"identity","/bin/cat"}};
  reject([&]{rewrite(unit,options);},"ON1X-PPX-SCOPE");
  require(write_sexpr(encode_source_module_metadata(unit))==write_sexpr(packet),"failed PPX changed the source");

  unit.module_imports.clear();
  unit.requires_module_boundary=true;
  reject([&]{rewrite(unit,options);},"ON1X-PPX-SCOPE");
  unit.requires_module_boundary=false;
#if defined(__unix__) || defined(__APPLE__)
  rewrite(unit,options);
  require(unit.module&&unit.module->name=="acme.tools"&&unit.functions[0].visibility.elevation==1,
          "Unit-v1 identity lost host metadata");
#endif
  auto original=copy(unit);
  auto wire=encode(unit);
  auto& declarations=wire.items[2].items;
  declarations[0].items.back().items[1]=string("/forged/other.on1x");
  auto generated=declarations[0];
  generated.items[1]=string("invented");
  declarations.push_back(std::move(generated));
  auto result=decode_rewrite(wire,original);
  require(result.functions[0].where.file==source.file&&
          result.functions[0].visibility.kind==DeclarationVisibility::Shared,
          "wire location replaced host declaration provenance");
  require(result.functions[1].where.file=="<ppx-generated>"&&
          result.functions[1].visibility.kind==DeclarationVisibility::Unspecified,
          "synthesized declaration inherited source provenance or visibility");
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
