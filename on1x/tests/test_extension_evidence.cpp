#include <on1x/extensions.hpp>
#include <on1x/loader.hpp>
#include <on1x/clone.hpp>
#include <on1x/backend/mir.hpp>
#include <iostream>
#include <functional>

using namespace on1x;
namespace {
void require(bool good,const std::string& message){if(!good)throw std::runtime_error(message);}
void rejects(const std::function<void()>& f){
 try{f();}catch(const Error&){return;}
 throw std::runtime_error("invalid evidence accepted");
}
Unit copy(const Unit& unit){std::size_t budget=262144;return clone_unit(unit,budget);}
}
int main(int argc,char** argv){
 try{
  require(argc==3,"expected compiler and Category example");
  auto options=default_loader_options(argv[1]);
  auto unit=load_unit(argv[2],options).unit;
  check(unit);extensions::verify_evidence(unit);
  require(unit.extension_evidence.size()==1,"missing host evidence");
  auto report=meta::read_sexpr(extensions::evidence_plan(unit),"<plan>");
  auto plan=report.items.at(4).items.at(0).items.at(9);
  const auto& batch=unit.extension_evidence.front();
  extensions::admit_evidence_plan(unit,batch,plan);
  auto snapshot=extensions::evidence_snapshot(unit,batch);
  auto foreign=batch;
  rejects([&]{(void)extensions::evidence_snapshot(unit,foreign);});
  auto second=copy(unit);check(second);
  require(meta::write_sexpr(snapshot)==meta::write_sexpr(extensions::evidence_snapshot(second,second.extension_evidence.front())),"rechecking changed evidence");
  auto mutation=[&](const std::function<void(Unit&)>& change){
   auto candidate=copy(unit);change(candidate);
   rejects([&]{extensions::verify_evidence(candidate);});
   rejects([&]{(void)emit_c(candidate,"evidence");});
   rejects([&]{(void)mir::lower(candidate);});
  };
  mutation([](auto& u){u.extension_evidence.clear();});
  mutation([](auto& u){u.source_digests.clear();});
  mutation([](auto& u){u.extension_evidence.push_back(u.extension_evidence.front());});
  mutation([](auto& u){u.extension_evidence.front().contracts.clear();});
  mutation([](auto& u){u.extension_evidence.front().contracts.front().members.front().scheme=Type::for_all(1,Type::function({Type::bound(0),Type::bound(0)},Type::I64));});
  mutation([](auto& u){u.extension_evidence.front().implementations.front().arguments={Type::Bool};});
  mutation([](auto& u){u.extension_evidence.front().implementations.front().bindings.clear();});
  mutation([](auto& u){u.extension_evidence.front().implementations.front().bindings.front().function="main";});
  mutation([](auto& u){u.extension_evidence.front().implementations.front().where.file="foreign";});
  mutation([](auto& u){
   auto extra=u.extension_evidence.front().implementations.front();extra.name="Duplicate";
   u.extension_evidence.front().implementations.push_back(std::move(extra));
  });
  mutation([](auto& u){
   const auto name=u.extension_evidence.front().implementations.front().bindings.front().selector;
   for(auto& f:u.functions)if(f.name==name)f.body.front().value->callee->text="Different_eq";
  });
  mutation([](auto& u){
   const auto name=u.extension_evidence.front().implementations.front().bindings.front().selector;
   for(auto& f:u.functions)if(f.name==name)std::swap(f.body.front().value->children[0],f.body.front().value->children[1]);
  });
  mutation([](auto& u){
   const auto name=u.extension_evidence.front().implementations.front().bindings.front().selector;
   for(auto& f:u.functions)if(f.name==name)f.body.front().value->children[0].reset();
  });
  mutation([](auto& u){
   const auto name=u.extension_evidence.front().implementations.front().bindings.front().function;
   for(auto& f:u.functions)if(f.name==name)f.result=Type::I64;
  });
  auto bad_plan=[&](const std::function<void(meta::SExpr&)>& change){
   auto bad=plan;change(bad);rejects([&]{extensions::admit_evidence_plan(unit,batch,bad);});
  };
  bad_plan([](auto& p){p.items[2]=meta::string(std::string(64,'0'));});
  bad_plan([](auto& p){p.items[3].items.clear();});
  bad_plan([](auto& p){p.items[3].items.push_back(p.items[3].items.front());});
  bad_plan([](auto& p){std::swap(p.items[3].items[0],p.items[3].items[1]);});
  bad_plan([](auto& p){p.items[3].items.front().items[3]=meta::string("forged");});
  bad_plan([](auto& p){p.items[3].items.front().items[5]=meta::encode_type(Type::I64);});
  bad_plan([](auto& p){p.items[1]=meta::symbol("2");});
  auto stale=copy(unit);stale.source_digests.begin()->second=std::string(64,'f');
  rejects([&]{extensions::admit_evidence_plan(stale,stale.extension_evidence.front(),plan);});
  std::cout<<"concrete evidence: independent type, provenance, dispatch, snapshot and plan rejection PASS\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
