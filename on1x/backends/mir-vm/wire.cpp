#include "wire.hpp"
#include "On1xVM.h"
#include <algorithm>
#include <array>
#include <bit>

namespace on1x::vm {
namespace {
using namespace mir;
constexpr std::string_view magic="O1VM\x01\r\n";
// Same aggregate table bound on both producer and consumer. Node identities
// have a separate MIR verification budget; repeated operands are not nodes.
constexpr std::size_t max_items=4194304;
constexpr std::array kinds={Type::Void,Type::Bool,Type::I64,Type::F64,Type::Tuple,
                            Type::F32,Type::U8,Type::U16,Type::U32,Type::U64};
[[noreturn]] void bad(std::string_view why,std::size_t offset=0){
 throw Error({"<o1vm>",offset,1,offset+1},"ON1X-VM: "+std::string(why),"ON1X-VM-VERIFY");
}
struct Writer {
 std::string data;std::size_t items=0;
 void raw(std::string_view s){
  if(s.size()>max_artifact_bytes-data.size())bad("artifact size limit");
  data.append(s);
 }
 void byte(std::uint8_t n){char c=std::bit_cast<char>(n);raw({&c,1});}
 void num(std::uint64_t n){while(n>=128){byte(static_cast<std::uint8_t>(n)|128);n>>=7;}byte(n);}
 void text(const std::string& s){num(s.size());raw(s);}
 void loc(const Location& l){text(l.file);num(l.offset);num(l.line);num(l.column);}
 template<class T,class F> void list(const std::vector<T>& v,F put){
  if(v.size()>max_items-items)bad("table length limit");
  items+=v.size();
  num(v.size());for(const auto& x:v)put(x);
 }
 void type(const Type& t){
  auto it=std::find(kinds.begin(),kinds.end(),t.kind());
  if(it==kinds.end())bad("unsupported register type");
  byte(static_cast<unsigned>(it-kinds.begin()));
  if(t.is_tuple())list(t.elements(),[&](const Type& e){type(e);});
 }
 void value(const Value& v){num(v.id);type(v.type);loc(v.where);}
 void ids(const std::vector<Id>& v){list(v,[&](Id id){num(id);});}
 void instruction(const Instruction& i){
  byte(1+static_cast<unsigned>(i.op));num(i.id);type(i.type);ids(i.operands);
  num(std::bit_cast<std::uint64_t>(i.immediate));num(i.target);num(i.effect_in);
  num(i.effect_out);text(i.primitive);loc(i.where);
 }
 void block(const Block& b){
  byte(On1xVM_opcode_Enter);num(b.id);num(b.effect);loc(b.where);
  list(b.parameters,[&](const auto& p){value(p);});
  list(b.instructions,[&](const auto& i){instruction(i);});
  const auto& t=b.exit;
  byte(t.kind==Exit::Return?On1xVM_opcode_Return:t.kind==Exit::Jump?On1xVM_opcode_Jump:On1xVM_opcode_Branch);
  num(t.id);num(t.value);num(t.effect);list(t.edges,[&](const Edge& e){num(e.block);ids(e.arguments);});loc(t.where);
 }
};
struct Reader {
 std::string_view data;
 std::size_t off=0,items=0;
 [[noreturn]] void fail(std::string_view why) const {bad(why,off);}
 unsigned byte(){if(off==data.size())fail("truncated operand");return static_cast<unsigned char>(data[off++]);}
 std::uint64_t num(){
  std::uint64_t n=0;
  for(unsigned k=0;k<=63;k+=7){
   auto b=byte();if(k==63&&b>1)fail("integer overflow");
   n|=std::uint64_t(b&127)<<k;
   if(!(b&128)){if(k&&!b)fail("noncanonical integer");return n;}
  }
  fail("invalid integer");
 }
 std::string text(){auto n=num();if(n>data.size()-off)fail("truncated string");
  auto s=data.substr(off,n);off+=n;return std::string(s);}
 Location loc(){auto f=text();auto o=num(),l=num(),c=num();
  if(o>SIZE_MAX||l>SIZE_MAX||c>SIZE_MAX)fail("source location out of range");
  return {std::move(f),std::size_t(o),std::size_t(l),std::size_t(c)};}
 template<class F> auto list(F get,std::size_t limit=65536){
  auto n=num();if(n>limit||n>data.size()-off||n>max_items-items)fail("table length limit");
  items+=n;std::vector<decltype(get())> v;
  // Do not reserve untrusted counts: truncated nested tables must be cheap.
  for(std::size_t k=0;k<n;++k)v.push_back(get());
  return v;
 }
 Type type(unsigned depth=0){auto t=byte();if(t>=kinds.size()||depth>32)fail("invalid or deep type");
  if(kinds[t]==Type::Tuple){auto fields=list([&]{return type(depth+1);},max_items);
   if(fields.size()<2)fail("invalid tuple arity");
   return Type::tuple(std::move(fields));}
  return kinds[t];}
 Value value(){return {num(),type(),loc()};}
 std::vector<Id> ids(){return list([&]{return num();},max_items);}
 Instruction instruction(){
  auto op=byte();if(!op||op>On1xVM_opcode_Numeric)fail("unknown instruction");
  Instruction i;i.id=num();i.type=type();i.op=static_cast<Op>(op-1);
  i.operands=ids();i.immediate=std::bit_cast<std::int64_t>(num());i.target=num();
  i.effect_in=num();i.effect_out=num();i.primitive=text();i.where=loc();return i;
 }
 Block block(){
  if(byte()!=On1xVM_opcode_Enter)fail("expected block entry");
  Block b;b.id=num();b.effect=num();b.where=loc();b.parameters=list([&]{return value();});
  b.instructions=list([&]{return instruction();});auto& t=b.exit;
  switch(byte()){case On1xVM_opcode_Return:t.kind=Exit::Return;break;
   case On1xVM_opcode_Jump:t.kind=Exit::Jump;break;case On1xVM_opcode_Branch:t.kind=Exit::Branch;break;
   default:fail("unknown control opcode");}
  t.id=num();t.value=num();t.effect=num();t.edges=list([&]{return Edge{num(),ids()};},2);t.where=loc();return b;
 }
 mir::Function function(){
  mir::Function f;f.id=num();f.name=text();f.result=type();f.where=loc();
  f.slots=list([&]{return value();});f.blocks=list([&]{return block();});return f;
 }
};
}
std::string compile(const mir::Module& m){
 mir::verify(m);Writer w;w.raw(magic);w.byte(m.version);w.num(m.id);
 w.list(m.functions,[&](const mir::Function& f){
  w.num(f.id);w.text(f.name);w.type(f.result);w.loc(f.where);
  w.list(f.slots,[&](const auto& s){w.value(s);});w.list(f.blocks,[&](const auto& b){w.block(b);});
 });return std::move(w.data);
}
mir::Module decode(std::string_view bytes){
 if(bytes.size()>max_artifact_bytes||!bytes.starts_with(magic))bad("invalid magic/version or oversized artifact");
 Reader r{bytes,magic.size()};mir::Module m;m.version=r.byte();m.id=r.num();
 m.functions=r.list([&]{return r.function();});if(r.off!=bytes.size())r.fail("trailing bytes");
 // Treat all persisted IDs, effects, calls, descriptor names, block arguments,
 // slot initialization and types as untrusted until the authoritative checker passes.
 mir::verify(m);return m;
}
void verify(std::string_view bytes){(void)decode(bytes);}
}
