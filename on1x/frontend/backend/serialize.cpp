#include <on1x/backend/mir.hpp>
#include <on1x/meta.hpp>
#include <charconv>
#include <sstream>

namespace on1x::mir {
namespace {
using meta::SExpr;using meta::list;using meta::symbol;using meta::string;
template<class T>SExpr number(T n){return symbol(std::to_string(n));}
SExpr location(const Location& at){return list({symbol("at"),string(at.file),number(at.offset),number(at.line),number(at.column)});}
SExpr encode_type(const Type& t){
 if(!t.is_tuple())return symbol(type_name(t));
 std::vector<SExpr> fields;for(const auto& element:t.elements())fields.push_back(encode_type(element));
 return list({symbol("tuple"),list(std::move(fields))});
}
SExpr value(const Value& v){return list({symbol("value"),number(v.id),encode_type(v.type),location(v.where)});}
template<class T,class F>SExpr sequence(const std::vector<T>& values,F encode){std::vector<SExpr> out;for(const auto& v:values)out.push_back(encode(v));return list(std::move(out));}
SExpr ids(const std::vector<Id>& values){return sequence(values,[](Id id){return number(id);});}
std::string_view exit_name(Exit kind){return kind==Exit::Return?"return":kind==Exit::Jump?"jump":"branch";}
SExpr instruction(const Instruction& i){
 auto out=list({symbol("instruction"),number(i.id),symbol(std::string(op_name(i.op))),encode_type(i.type),ids(i.operands),number(i.immediate),number(i.target),number(i.effect_in),number(i.effect_out),location(i.where)});
 if(i.op==Op::Numeric)out.items.push_back(string(i.primitive));
 return out;
}
SExpr block(const Block& b){
 const auto& t=b.exit;
 auto edges=sequence(t.edges,[](const Edge& e){return list({symbol("edge"),number(e.block),ids(e.arguments)});});
 return list({symbol("block"),number(b.id),number(b.effect),location(b.where),sequence(b.parameters,value),sequence(b.instructions,instruction),
  list({symbol("exit"),number(t.id),symbol(std::string(exit_name(t.kind))),number(t.value),number(t.effect),std::move(edges),location(t.where)})});
}
SExpr directive(const Directive& d){
 return list({symbol("directive"),string(d.backend),string(d.target),string(d.kind),string(d.action),
              number(d.function),symbol(d.speculative?"true":"false")});
}
[[noreturn]] void bad(const SExpr& n,const std::string& why){throw Error(n.where,"ON1X-MIR-DECODE: "+why);}
const std::vector<SExpr>& items(const SExpr& n){if(n.kind!=SExpr::Kind::List)bad(n,"expected list");return n.items;}
const std::string& atom(const SExpr& n){if(n.kind!=SExpr::Kind::Symbol)bad(n,"expected symbol");return n.text;}
const std::string& text(const SExpr& n){if(n.kind!=SExpr::Kind::String)bad(n,"expected string");return n.text;}
void shape(const SExpr& n,std::string_view tag,std::size_t count){if(items(n).size()!=count||atom(n.items[0])!=tag)bad(n,"expected "+std::string(tag)+" with "+std::to_string(count-1)+" fields");}
template<class T>T integer(const SExpr& n){
 const auto& s=atom(n);T result{};auto [p,ec]=std::from_chars(s.data(),s.data()+s.size(),result);
 if(ec!=std::errc{}||p!=s.data()+s.size())bad(n,"invalid/out-of-range integer");
 return result;
}
Type type(const SExpr& n,std::size_t depth=0){
 if(depth>32)bad(n,"type depth budget exhausted");
 if(n.kind==SExpr::Kind::List){
  shape(n,"tuple",2);std::vector<Type> elements;
  for(const auto& child:items(n.items[1]))elements.push_back(type(child,depth+1));
  if(elements.size()<2)bad(n,"tuple type requires at least two fields");
  return Type::tuple(std::move(elements));
 }
  auto s=atom(n);for(auto kind:{Type::I64,Type::Bool,Type::Void,Type::F64,Type::F32,Type::U8,Type::U16,Type::U32,Type::U64})if(s==type_name(kind))return kind;bad(n,"unsupported type: "+s);
}
Location decode_location(const SExpr& n){shape(n,"at",5);return {text(n.items[1]),integer<std::size_t>(n.items[2]),integer<std::size_t>(n.items[3]),integer<std::size_t>(n.items[4])};}
Value decode_value(const SExpr& n){shape(n,"value",4);return {integer<Id>(n.items[1]),type(n.items[2]),decode_location(n.items[3])};}
std::vector<Id> decode_ids(const SExpr& n){std::vector<Id> out;for(const auto& v:items(n))out.push_back(integer<Id>(v));return out;}
Instruction decode_instruction(const SExpr& n){
  if(items(n).size()!=10&&items(n).size()!=11)bad(n,"invalid instruction shape");
  Instruction i;shape(n,"instruction",n.items.size());i.id=integer<Id>(n.items[1]);
  bool found=false;for(unsigned k=0;k<=static_cast<unsigned>(Op::Numeric);++k)if(atom(n.items[2])==op_name(static_cast<Op>(k))){i.op=static_cast<Op>(k);found=true;break;}
  if(!found)bad(n,"unknown opcode");
  if(n.items.size()!=(i.op==Op::Numeric?11u:10u))bad(n,"numeric descriptor field mismatch");
  if(i.op==Op::Numeric)i.primitive=text(n.items[10]);
 i.type=type(n.items[3]);i.operands=decode_ids(n.items[4]);i.immediate=integer<std::int64_t>(n.items[5]);
 i.target=integer<Id>(n.items[6]);i.effect_in=integer<Id>(n.items[7]);i.effect_out=integer<Id>(n.items[8]);i.where=decode_location(n.items[9]);return i;
}
Block decode_block(const SExpr& n){
 shape(n,"block",7);Block b;b.id=integer<Id>(n.items[1]);b.effect=integer<Id>(n.items[2]);b.where=decode_location(n.items[3]);
 for(const auto& p:items(n.items[4]))b.parameters.push_back(decode_value(p));
 for(const auto& i:items(n.items[5]))b.instructions.push_back(decode_instruction(i));
 const auto& t=n.items[6];shape(t,"exit",7);b.exit.id=integer<Id>(t.items[1]);auto k=atom(t.items[2]);
 if(k=="return")b.exit.kind=Exit::Return;else if(k=="jump")b.exit.kind=Exit::Jump;else if(k=="branch")b.exit.kind=Exit::Branch;else bad(t,"unknown terminator");
 b.exit.value=integer<Id>(t.items[3]);b.exit.effect=integer<Id>(t.items[4]);b.exit.where=decode_location(t.items[6]);
 for(const auto& e:items(t.items[5])){shape(e,"edge",3);b.exit.edges.push_back({integer<Id>(e.items[1]),decode_ids(e.items[2])});}
 return b;
}
Directive decode_directive(const SExpr& n){
 shape(n,"directive",7);
 const auto& speculative=atom(n.items[6]);
 if(speculative!="true"&&speculative!="false")bad(n.items[6],"expected directive boolean");
 return {text(n.items[1]),text(n.items[2]),text(n.items[3]),text(n.items[4]),integer<Id>(n.items[5]),speculative=="true"};
}
}
std::string serialize(const Module& module){
 // This codec has no representation for nominal declarations or owner places.
 verify(module);
 auto functions=sequence(module.functions,[](const Function& f){return list({symbol("function"),number(f.id),string(f.name),encode_type(f.result),location(f.where),sequence(f.slots,value),sequence(f.blocks,block)});});
 auto tree=list({symbol("on1x.mir"),number(module.version),number(module.id),std::move(functions)});
 if(module.version==directive_format_version)tree.items.push_back(sequence(module.directives,directive));
 return meta::write_sexpr(tree);
}
Module deserialize(std::string_view text,const std::string& filename){
 auto tree=meta::read_sexpr(text,filename);
 if(items(tree).size()<2||atom(tree.items[0])!="on1x.mir")bad(tree,"expected on1x.mir with a format version");
 Module m;m.version=integer<unsigned>(tree.items[1]);
 // The in-memory nominal-owner profile does not extend the public text wire.
 if(m.version<legacy_format_version||m.version>directive_format_version)bad(tree,"unsupported format version");
 shape(tree,"on1x.mir",m.version==directive_format_version?5:4);m.id=integer<Id>(tree.items[2]);
 for(const auto& n:items(tree.items[3])){
  shape(n,"function",7);Function f;f.id=integer<Id>(n.items[1]);f.name=::on1x::mir::text(n.items[2]);f.result=type(n.items[3]);f.where=decode_location(n.items[4]);
  for(const auto& s:items(n.items[5]))f.slots.push_back(decode_value(s));
  for(const auto& b:items(n.items[6]))f.blocks.push_back(decode_block(b));
  m.functions.push_back(std::move(f));
 }
 if(m.version==directive_format_version)
  for(const auto& n:items(tree.items[4]))m.directives.push_back(decode_directive(n));
 verify(m);return m;
}
bool structurally_equal(const Module& a,const Module& b){return serialize(a)==serialize(b);}
std::string cfg_dot(const Module& module){
 verify(module);std::ostringstream out;out<<"digraph on1x_mir {\n";
 for(const auto& f:module.functions){
  out<<"  subgraph cluster_"<<f.id<<" {\n    label=\"function "<<f.id<<"\";\n";
  for(const auto& b:f.blocks){
   out<<"    b"<<b.id<<" [label=\"b"<<b.id<<"("<<b.parameters.size()<<")\\n"<<exit_name(b.exit.kind)<<"\"];\n";
   for(std::size_t n=0;n<b.exit.edges.size();++n)out<<"    b"<<b.id<<" -> b"<<b.exit.edges[n].block<<" [label=\""<<n<<"\"];\n";
  }
  out<<"  }\n";
 }
 return out.str()+"}\n";
}
}
