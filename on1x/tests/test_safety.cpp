#include <on1x/safety.hpp>
#include <iostream>

namespace c=moonstone::chisel;
namespace s=on1x::safety;
c::Instruction event(c::Op op,c::Id place=1,c::Id token=0){return {op,place,token,{"external-client.on1x",3,4,3,4}};}
c::Function linear(std::initializer_list<c::Instruction> events){return {{{0,events,{}}},0};}
template<class F>void rejects(F f,const std::string& expected){try{f();}catch(const on1x::Error& e){if(std::string(e.what()).find(expected)!=std::string::npos)return;throw;}throw std::runtime_error("missing rejection: "+expected);}
int main(){try{
 using O=c::Op;
 auto create=event(O::Create),drop=event(O::Drop),use=event(O::Use),borrow=event(O::BorrowShared,1,10),end=event(O::EndBorrow,1,10);
 s::verify(linear({create,borrow,event(O::Use,1,10),end,event(O::Mutate),drop}));
 s::verify(linear({create,event(O::BorrowUnique,1,11),event(O::Use,1,11),event(O::EndBorrow,1,11),drop}));
 s::verify(linear({create,event(O::Move)}));
 rejects([&]{s::verify(linear({create,drop,use}));},"CHISEL-INVALID-LIFE");
 rejects([&]{s::verify(linear({create,drop,drop}));},"CHISEL-INVALID-LIFE");
 rejects([&]{s::verify(linear({create,borrow,event(O::Mutate),end,drop}));},"CHISEL-MUTATE-LOAN");
 rejects([&]{s::verify(linear({create,borrow,drop}));},"CHISEL-DROP-BORROWED");
 rejects([&]{s::verify(linear({create,borrow,event(O::Move)}));},"CHISEL-MOVE-BORROWED");
 rejects([&]{s::verify(linear({create,event(O::BorrowUnique,1,11),use}));},"owner access during exclusive");
 rejects([&]{s::verify(linear({create,borrow,end,event(O::Use,1,10),drop}));},"inactive or mismatched");
 rejects([&]{s::verify(linear({create}));},"live at normal exit");
 rejects([&]{s::verify(linear({create,event(O::Escape)}));},"unsupported escape");
 rejects([&]{s::verify(linear({create,event(static_cast<O>(999))}));},"unknown operation");
 rejects([&]{s::verify(linear({create,borrow,end,borrow,end,drop}));},"multiple definitions");
 c::Function joins{{{0,{create},{1,2}},{1,{borrow},{3}},{2,{}, {3}},{3,{event(O::Use,1,10),end,drop},{}}},0};
 rejects([&]{s::verify(joins);},"path-dependent loan");
 c::Function reinit{{{0,{create},{1,2}},{1,{drop},{3}},{2,{}, {3}},{3,{event(O::Reinitialize),drop},{}}},0};
 rejects([&]{s::verify(reinit);},"possibly-live resource");
 c::Function transferred{{{0,{create},{1,2}},{1,{event(O::Move)},{3}},{2,{drop},{3}},{3,{}, {}}},0};
 s::verify(transferred);
 c::Function loop{{{0,{}, {1}},{1,{}, {2,3}},{2,{create,borrow,end,drop},{1}},{3,{}, {}}},0};s::verify(loop);
 auto broken=loop;broken.blocks[2].successors={0};rejects([&]{s::verify(broken);},"entry backedge");
 broken=loop;broken.blocks.push_back({99,{}, {}});rejects([&]{s::verify(broken);},"unreachable");
 rejects([&]{s::verify(loop,{1,4194304});},"nodes exhausted");
 rejects([&]{s::verify(loop,{4096,0});},"work exhausted");
 rejects([&]{s::verify(loop,{4096,200});},"Chisel");
 auto unit=on1x::parse(on1x::scan("fn main(){let r=json_parse_or_die(\"{}\");println(json_size(r));}","source.on1x"));on1x::check(unit);
 auto before=on1x::emit_c(unit,"source.on1x");auto report=s::check(unit);
  if(report!=s::check(unit)||before!=on1x::emit_c(unit,"source.on1x"))throw std::runtime_error("nondeterministic/mutating admission");
 auto foreign=on1x::parse(on1x::scan("extern fn typed(x:bool,y:f64)->f64;fn main(){println(typed(true,1.5));}","typed.on1x"));on1x::check(foreign);
 std::map<std::string,s::AdmittedExtern> admitted{{"typed",{{on1x::Type::Bool,on1x::Type::F64},on1x::Type::F64}}};
 s::check(foreign,{},admitted);
 auto bad=admitted;bad.at("typed").parameters[1]=on1x::Type::String;
 rejects([&]{s::check(foreign,{},bad);},"outside the scalar profile");
 bad=admitted;bad.at("typed").result=on1x::Type::Json;
 rejects([&]{s::check(foreign,{},bad);},"outside the scalar profile");
 auto& call=*foreign.functions.back().body.front().value->children.front();
 call.children[1]->type=on1x::Type::I64;
 rejects([&]{s::check(foreign,{},admitted);},"call parameter type drift");
 call.children[1]->type=on1x::Type::F64;call.type=on1x::Type::Bool;
  rejects([&]{s::check(foreign,{},admitted);},"call result drift");
  auto identity=std::string(64,'a'),name="on1x_ffi_"+identity+"_text";
  auto evidence="on1x.admitted-extern/v4\nidentity "+identity+"\nextern "+name+" (borrowed_string) -> static_string\n";
  auto text_admission=s::parse_admission(evidence);
  if(!text_admission.at(name).immutable_strings)throw std::runtime_error("missing immutable string profile");
  auto text_unit=on1x::parse(on1x::scan("extern fn "+name+"(x:string)->string;fn main(){println("+name+"(\"text\"));}","strings.on1x"));on1x::check(text_unit);
  s::check(text_unit,{},text_admission);
  auto legacy=text_admission;legacy.at(name).immutable_strings=false;
  rejects([&]{s::check(text_unit,{},legacy);},"outside the scalar profile");
  text_admission.at(name).result=on1x::Type::I64;
  rejects([&]{s::check(text_unit,{},text_admission);},"admitted extern result drift");
 std::cout<<"Chisel resource lifetimes, external protocol models, joins, loops, cleanup and budgets PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
