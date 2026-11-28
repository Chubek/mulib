#include <on1x/regex.hpp>
#include <on1x/regex_scan.h>
#include <algorithm>
#include <array>
#include <string_view>
#ifdef ON1X_WITH_FIRESTONE_REGEX
#include <liblr/ThompsonConstruction.hpp>
#include <liblr/UTF8Automaton.hpp>
#endif

namespace on1x::regex {
namespace {
[[noreturn]] void fail(Location at,const char* text,std::size_t inside = 0,const char* id = "ON1X-REGEX") {
 at.offset+=inside;at.column+=inside;
 throw Error(std::move(at),std::string(id)+": "+text,id);
}
#ifdef ON1X_WITH_FIRESTONE_REGEX
namespace lr = firestone::lr;
constexpr std::size_t max_states = 512,max_nfa_states = 2048,max_groups = 16;

std::string byte_literal(unsigned byte) {
 constexpr std::string_view digits = "0123456789abcdef";
 return std::string{"\\x"}+digits[byte>>4]+digits[byte&15];
}

// Turn the pinned UTF-8 scalar DFA into a portable byte-regex atom. This DFA
// is acyclic, including its terminal accept state; every path is one scalar.
std::string scalar_atom() {
 const auto dfa=lr::utf8_scalar_dfa();
 const auto& states=dfa.states();
 auto translate=[&](auto&& self,lr::StateId id) -> std::string {
  const auto& row=states[id];
  if(row.acceptance)return {};
  std::map<lr::StateId,std::string> by_target;
  for(unsigned lo=0;lo<lr::byte_count;){
   auto target=row.transitions[lo];unsigned hi=lo;
   while(hi+1<lr::byte_count&&row.transitions[hi+1]==target)++hi;
   if(target!=lr::no_state){
    auto& range=by_target[target];
    range+=byte_literal(lo);
    if(hi!=lo)range+='-'+byte_literal(hi);
   }
   lo=hi+1;
  }
  std::string result;
  for(const auto& [target,ranges]:by_target){
   if(!result.empty())result+='|';
   result+='['+ranges+']'+self(self,target);
  }
  return '('+result+')';
 };
 return translate(translate,dfa.start());
}

// A small Unicode profile adapter: token boundaries are scalars, so a
// quantifier never attaches to just the final byte of a UTF-8 literal. Keep
// the byte-expression grammar and Thompson construction in liblr.
class Profile {
 using Width=std::optional<std::size_t>;
 std::string_view input;
 const Location& at;
 std::size_t pos=0;
 std::string bytes;
 std::vector<std::size_t> origins;
 std::vector<Span> groups;
 bool capturable=true,has_group=false,root_alternate=false;
 bool here(char c)const{return pos<input.size()&&input[pos]==c;}
 [[noreturn]] void error(const char* text,std::size_t offset)const{fail(at,text,offset+1);}
 void emit(std::string_view s,std::size_t offset){
  if(bytes.size()+s.size()>8192)fail(at,"regex translation exceeds 8192 bytes",offset+1,"ON1X-REGEX-LIMIT");
  bytes+=s;origins.insert(origins.end(),s.size(),offset);
 }
 void take(){
  emit(input.substr(pos,1),pos);
  ++pos;
 }
 void escaped(){
  auto begin=pos++;
  if(pos==input.size())error("unfinished regex escape",begin);
  auto c=input[pos++];
  if(c=='/')emit("/",begin);
  else if(c=='x'){
   auto hex=[](unsigned char ch){
    return (ch>='0'&&ch<='9')?ch-'0':(ch>='A'&&ch<='F')?ch-'A'+10:(ch>='a'&&ch<='f')?ch-'a'+10:-1;
   };
   if(input.size()-pos<2||hex(input[pos])<0||hex(input[pos+1])<0)error("expected two hex digits",begin);
   auto value=16*hex(input[pos])+hex(input[pos+1]);
   if(!value||value>=128)error("regex hex escapes require non-NUL ASCII; write Unicode literally",begin);
   pos+=2;emit(input.substr(begin,4),begin);
  }else{
   if(std::string_view(R"(nrt\.^$|()[]*+?{}-)").find(c)==std::string_view::npos)
    error("unsupported regex escape",begin);
   emit(input.substr(begin,2),begin);
  }
 }
 void character_class(){
  auto begin=pos;take();
  if(here('^'))error("negated regex classes are not supported",pos);
  while(pos<input.size()&&!here(']')){
   if(here('\\'))escaped();
   else{
    if(static_cast<unsigned char>(input[pos])>=128)error("regex classes and ranges are ASCII-only",pos);
    take();
   }
  }
  if(!here(']'))error("unterminated regex character class",begin);
  take();
 }
 Width sequence(unsigned depth){
  Width width=0;
  while(pos<input.size()&&!here(')')&&!here('|')){
   auto begin=pos;bool group=here('(');
   Width part=1;
   if(group){
    has_group=true;
    if(depth>0)capturable=false;
    if(depth==32)fail(at,"regex group nesting exceeds 32 levels",begin+1,"ON1X-REGEX-LIMIT");
    take();part=alternate(depth+1);
    if(!here(')'))error("expected ')' in regex",pos);
    take();
   }else if(here('['))character_class();
   else if(here('\\'))escaped();
   else if(here('.')){
    static const auto scalar=scalar_atom();
    emit(scalar,pos++);
   }else if(static_cast<unsigned char>(input[pos])>=128){
    // Input already validated; the decoder supplies this atom's byte width.
    auto scalar=lr::decode_utf8_prefix(input.substr(pos));
    emit("(",pos);
    for(std::size_t i=0;i<scalar->length;++i)
     emit(byte_literal(static_cast<unsigned char>(input[pos+i])),pos+i);
    emit(")",pos);pos+=scalar->length;
   }else{
    if(std::string_view("*+?]^${}").find(input[pos])!=std::string_view::npos)
     error("unsupported or misplaced regex metacharacter",pos);
    take();
   }
   const bool quantified=here('*')||here('+')||here('?');
   if(quantified){
    take();
    if(part!=Width{0})part.reset();
   }
   if(group&&!depth){
    if(quantified||!width||!part||groups.size()>=max_groups)capturable=false;
    else groups.push_back({*width,*width+*part});
   }
   if(width&&part)*width+=*part;else width.reset();
  }
  return width;
 }
 Width alternate(unsigned depth){
  auto width=sequence(depth);
  while(here('|')){
   if(!depth)root_alternate=true;
   take();
   if(sequence(depth)!=width)width.reset();
  }
  return width;
 }
public:
 Profile(std::string_view input,const Location& at):input(input),at(at){}
 lr::NFA run(Program& out){
  if(auto bad=lr::utf8_error_offset(input))error("invalid UTF-8 in regex literal",*bad);
  (void)alternate(0);
  if(pos!=input.size())error("unexpected ')' in regex",pos);
  out.capturable=capturable&&!(has_group&&root_alternate);
  if(out.capturable)out.groups=std::move(groups);
  try{return lr::compile_regex(bytes);}
  catch(const lr::RegexError& e){
   fail(at,e.what(),1+(e.offset()<origins.size()?origins[e.offset()]:pos));
  }
 }
};

// Like liblr::determinize, with a cap checked before creating each subset.
// The public liblr routine has no state/work limit and can grow exponentially.
void bounded_dfa(const lr::NFA& nfa,Program& result,const Location& at,std::size_t& work_left){
 const auto& source=nfa.states();
 if(source.size()>max_nfa_states)fail(at,"regex NFA exceeds 2048 states",0,"ON1X-REGEX-LIMIT");
 auto charge=[&](std::size_t amount){
  if(amount>work_left)fail(at,"regex determinization work budget exhausted",0,"ON1X-REGEX-LIMIT");
  work_left-=amount;
 };
 using Subset=std::vector<lr::StateId>;
 std::map<Subset,std::size_t> ids;
 std::vector<const Subset*> pending;
 std::vector<bool> seen(source.size());
 auto intern=[&](Subset subset){
  auto found=ids.find(subset);
  if(found!=ids.end())return found->second;
  if(ids.size()==max_states)fail(at,"regex DFA exceeds 512 states",0,"ON1X-REGEX-LIMIT");
  auto [item,added]=ids.emplace(std::move(subset),ids.size());
  (void)added;pending.push_back(&item->first);
  return item->second;
 };
 auto close=[&](Subset seeds){
  Subset subset;
  auto enter=[&](lr::StateId id){if(!seen[id]){seen[id]=true;subset.push_back(id);}};
  charge(seeds.size());
  for(auto seed:seeds)enter(seed);
  for(std::size_t i=0;i<subset.size();++i){
   charge(source[subset[i]].epsilon.size()+1);
   for(auto next:source[subset[i]].epsilon)enter(next);
  }
  for(auto id:subset)seen[id]=false;
  std::sort(subset.begin(),subset.end());
  return subset;
 };
 intern(close({nfa.start()}));
 for(std::size_t i=0;i<pending.size();++i){
  std::array<Subset,lr::byte_count> targets;
  State state;
  for(auto id:*pending[i]){
   state.accepts=state.accepts||source[id].acceptance.has_value();
   charge(1+source[id].edges.size()*lr::byte_count);
   for(const auto& edge:source[id].edges)
    for(unsigned byte=0;byte<lr::byte_count;++byte)
     if(edge.bytes[byte])targets[byte].push_back(edge.target);
  }
  for(unsigned byte=0;byte<lr::byte_count;++byte){
   if(targets[byte].empty())continue;
   auto target=intern(close(std::move(targets[byte])));
   if(!state.edges.empty()&&state.edges.back().last+1==byte&&state.edges.back().target==target)
    state.edges.back().last=byte;
   else state.edges.push_back({byte,byte,target});
  }
  result.states.push_back(std::move(state));
 }
}
#endif
}

const Program& Cache::get(const std::string& literal,const Location& at){
 if(auto hit=patterns.find(literal);hit!=patterns.end())return hit->second;
 if(literal.size()<2||literal.front()!='/'||literal.back()!='/'||
    on1x_regex_literal_width(literal.data(),literal.size())!=literal.size()||
    literal.find('\0')!=std::string::npos)
  fail(at,"invalid regex literal shape");
 if(literal.size()>max_pattern_bytes+2)fail(at,"regex literal exceeds 256 bytes",0,"ON1X-REGEX-LIMIT");
#ifdef ON1X_WITH_FIRESTONE_REGEX
 Program program;
 auto nfa=Profile(std::string_view(literal).substr(1,literal.size()-2),at).run(program);
 bounded_dfa(nfa,program,at,work_left);
 if(patterns.size()==1024||program.states.size()>8192-total_states)
  fail(at,"regex program exceeds 1024 patterns or 8192 DFA states",0,"ON1X-REGEX-LIMIT");
 for(const auto& state:program.states){
  if(state.edges.size()>65536-total_edges)
   fail(at,"regex program exceeds 65536 DFA byte ranges",0,"ON1X-REGEX-LIMIT");
  total_edges+=state.edges.size();
 }
 total_states+=program.states.size();
 return patterns.emplace(literal,std::move(program)).first->second;
#else
 fail(at,"regex requires the LibStone Firestone compiler SDK",0,"ON1X-REGEX-SDK");
#endif
}
void check_capture(const Program& program,const Location& at){
 if(!program.capturable)
  fail(at,"capture requires up to 16 unnested, unquantified groups of fixed scalar width at fixed scalar offsets; no top-level alternation with groups",0,"ON1X-REGEX-CAPTURE");
}
}
