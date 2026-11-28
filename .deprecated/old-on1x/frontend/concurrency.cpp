#include <on1x/concurrency.hpp>
#include <on1x/clone.hpp>
#include <on1x/numeric.hpp>
#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <type_traits>

namespace on1x::csp {
Type send_result(){return Type::data(Type::Sum,{Type::I64,Type::data(Type::Record,{})},{"Err","Ok"});}
Type receive_result(Type payload){return Type::data(Type::Sum,{Type::data(Type::Record,{}),std::move(payload)},{"None","Some"});}
Type try_send_result(){
 return Type::data(Type::Sum,
  {Type::data(Type::Record,{}),Type::data(Type::Record,{}),Type::data(Type::Record,{})},
  {"Closed","Sent","WouldBlock"});
}
Type try_receive_result(Type payload){
 return Type::data(Type::Sum,
  {Type::data(Type::Record,{}),Type::data(Type::Record,{}),std::move(payload)},
  {"Closed","Empty","Value"});
}
Type split_result(const Type& channel){
 if(channel.kind()!=Type::Channel||!channel.sender_cardinality()||
    !channel.receiver_cardinality())
  throw std::logic_error("split requires an exact channel descriptor type");
 std::vector<Type> endpoints;
 endpoints.reserve(channel.sender_cardinality()+channel.receiver_cardinality());
 for(std::size_t i=0;i<channel.sender_cardinality();++i)
  endpoints.push_back(Type::sender(channel.csp_payload()));
 for(std::size_t i=0;i<channel.receiver_cardinality();++i)
  endpoints.push_back(Type::receiver(channel.csp_payload()));
 return Type::tuple(std::move(endpoints));
}
namespace {
constexpr std::size_t max_payload_bytes=32768;
std::size_t align8(std::size_t value){
 return value>std::numeric_limits<std::size_t>::max()-7
  ?std::numeric_limits<std::size_t>::max():(value+7)&~std::size_t{7};
}
std::pair<std::size_t,std::string> payload_size(const Type& type,std::size_t depth=0){
 if(depth>32)return {0,"payload nesting exceeds 32 levels"};
 switch(type.kind()){
 case Type::Bool:case Type::U8:return {1,{}};
 case Type::U16:return {2,{}};
 case Type::F32:case Type::U32:case Type::Char:return {4,{}};
 case Type::I64:case Type::F64:case Type::U64:return {8,{}};
 case Type::Tuple:case Type::Record:{
  std::size_t size=type.elements().empty()?1:0;
  for(const auto& field:type.elements()){
   auto child=payload_size(field,depth+1);if(!child.second.empty())return child;
   auto padded=align8(child.first);
   if(padded>max_payload_bytes-size)return {0,"payload exceeds 32768-byte storage bound"};
   size+=padded;
  }
  return {std::max<std::size_t>(1,size),{}};
 }
 case Type::Sum:{
  std::size_t size=0;
  for(const auto& field:type.elements()){
   auto child=payload_size(field,depth+1);if(!child.second.empty())return child;
   size=std::max(size,align8(child.first));
  }
  if(size>max_payload_bytes-8)return {0,"payload exceeds 32768-byte storage bound"};
  return {size+8,{}};
 }
 case Type::Array:case Type::Slice:{
  auto child=payload_size(type.elements().front(),depth+1);
  if(!child.second.empty())return child;
  auto count=std::max<std::size_t>(1,type.extent());
  if(child.first>max_payload_bytes/count)
   return {0,"payload exceeds 32768-byte storage bound"};
  auto size=child.first*count;
  if(type.kind()==Type::Slice){
   size=align8(size);
   if(size>max_payload_bytes-16)
    return {0,"payload exceeds 32768-byte storage bound"};
   size+=16;
  }
  return {size,{}};
 }
 case Type::String:return {0,"string payload contains borrowed pointer storage"};
 case Type::SExpr:case Type::Buffer:case Type::Function:case Type::Code:
  return {0,"managed or callable payload requires a retain/drop protocol"};
 case Type::Own:return {0,"unique payload transfer requires task-local cleanup state"};
 case Type::Json:case Type::JsonView:case Type::Regex:case Type::RegexCaptures:
  return {0,"resource, view or runtime-pointer payload is unsupported"};
 case Type::Channel:case Type::Sender:case Type::Receiver:case Type::Fiber:
 case Type::ExternalFiber:case Type::CspGroup:
  return {0,"concurrency capabilities cannot be channel payloads"};
 case Type::Void:return {0,"void is not a channel payload"};
 case Type::Invalid:case Type::Hole:case Type::Generic:case Type::Bound:
 case Type::Forall:case Type::Named:
  return {0,"channel payload must be a closed resolved value type"};
 }
 return {0,"unknown channel payload representation"};
}
}
std::string payload_diagnostic(const Type& type){
 auto result=payload_size(type);
 if(!result.second.empty())return result.second;
 if(!result.first||result.first>max_payload_bytes)
  return "payload exceeds 32768-byte storage bound";
 return {};
}
bool payload_supported(const Type& type){return payload_diagnostic(type).empty();}
bool contains(const Type& t){
 if(t.is_csp())return true;
 if(t.is_function()){if(contains(t.result()))return true;for(const auto& p:t.parameters())if(contains(p))return true;}
 if(t.is_tuple()||t.is_data())for(const auto& p:t.elements())if(contains(p))return true;
 if(t.is_code()&&contains(t.code_result()))return true;
 if(t.is_forall()&&contains(t.body()))return true;
 for(const auto& a:t.arguments())if(contains(a))return true;
 return false;
}
namespace {
using E=Expr::Kind;using S=Stmt::Kind;
[[noreturn]] void fail(const Location& at,const std::string& text){throw Error(at,"ON1X-CSP: "+text,"ON1X-CSP");}
struct Budget {
 std::size_t nodes=262144,states=4*1024*1024;
 void tick(const Location& at,std::size_t depth){if(depth>520||!nodes--)throw Error(at,"concurrency verification depth/node budget exhausted","ON1X-CSP-LIMIT");}
 void state(std::size_t n,const Location& at){if(n>states)throw Error(at,"concurrency flow-state budget exhausted","ON1X-CSP-LIMIT");states-=n;}
};
bool operation(const Expr& e){return e.kind==E::Csp||e.kind==E::SourceCsp;}
// A bitset, not a boolean: after a branch, a capability may have been consumed.
// Such a value is neither usable nor (for a fiber) safely droppable.
constexpr unsigned live=1,dead=2;
using State=std::map<std::size_t,unsigned>;
using Flow=std::optional<State>;
class Verify {
 struct Local {Type type;bool mut,capture;std::size_t id;};
 struct Target {std::size_t id,keep;std::vector<State> exits,continues;};
   bool canonical;Budget budget;std::size_t serial=0,groups=0;
 std::vector<std::map<std::string,Local>> scopes;
 std::vector<Target> loops,regions;
 std::set<std::string> capture_uses;
  void source(const Location& at){
  if(at.file.empty()||at.file.size()>4096||at.file.find('\0')!=std::string::npos||!at.line||!at.column||
     at.offset>max_source_bytes||at.line>max_source_bytes+1||at.column>max_source_bytes+1)fail(at,"invalid concurrency source point");
 }
 Flow merge(Flow a,const Flow& b,const Location& at){
  if(!a)return b;
  if(!b)return a;
  budget.state(a->size()+b->size(),at);
  for(const auto& [id,status]:*b)(*a)[id]|=status;
  return a;
 }
 const Local& local(const std::string& name,const Location& at){
  for(auto s=scopes.rbegin();s!=scopes.rend();++s)if(auto f=s->find(name);f!=s->end()){
   if(f->second.capture)capture_uses.insert(name);
   return f->second;
  }
  fail(at,"missing local/capture: "+name);
 }
  std::string argument(const Expr& e){
   const auto& text=e.text;std::string value;
   if(text.size()<2||text.front()!='"'||text.back()!='"')fail(e.where,"malformed external argv string literal");
   value.reserve(text.size()-2);
   for(std::size_t i=1;i+1<text.size();++i){
    unsigned char c=static_cast<unsigned char>(text[i]);
    if(c=='"'||c=='\n'||c=='\r'||c=='\0')fail(e.where,"malformed external argv string literal");
    if(c=='\\'){
     if(++i+1>=text.size())fail(e.where,"malformed external argv string escape");
     c=static_cast<unsigned char>(text[i]);
     if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';
     else if(c=='0')fail(e.where,"external argv cannot contain NUL");
     else if(c!='"'&&c!='\\'&&c!='$')fail(e.where,"malformed external argv string escape");
    }
    value+=static_cast<char>(c);
   }
   return value;
  }
 void bind(const std::string& name,Type type,bool mut,State& state,const Location& at,bool capture=false){
  if(name.empty()||scopes.back().contains(name))fail(at,"empty/duplicate concurrency local");
  auto id=++serial;scopes.back().emplace(name,Local{type,mut,capture,id});
  if(type.is_csp())state[id]=live;
 }
 void pattern(const Pattern& p,Type t,State& state){
  if(p.is_tuple()){
   if(!t.is_tuple()||p.elements.size()!=t.elements().size())fail(p.where,"binding pattern/type mismatch");
   for(std::size_t i=0;i<p.elements.size();++i)pattern(p.elements[i],t.elements()[i],state);
  }else bind(p.name,t,false,state,p.where);
 }
 State project(State state,std::size_t keep,const Location& at){
  budget.state(state.size(),at);
  for(std::size_t i=keep;i<scopes.size();++i)for(const auto& [name,b]:scopes[i]){
    if((b.type==Type::Fiber||b.type==Type::ExternalFiber)&&(state[b.id]&live))fail(at,"fiber must be joined or cancelled on every normal exit: "+name);
   state.erase(b.id);
  }
  return state;
 }
 void capability(const Expr& e,Type expected,State& state,bool consume){
  if(e.kind!=E::Variable||e.reference!=Expr::Reference::Local||e.type!=expected)fail(e.where,"operation requires a named affine "+type_name(expected));
  const auto& b=local(e.text,e.where);
  if(b.type!=expected||b.mut||!state.contains(b.id)||state.at(b.id)!=live)fail(e.where,"moved, conditionally consumed or invalid capability: "+e.text);
  if(consume)state[b.id]=dead;
 }
 void lambda(const Expr& e,State& parent,bool launched,std::size_t depth){
  if(!e.type.is_function()||e.result!=e.type.result()||e.parameters.size()!=e.type.parameters().size()||contains(e.type))fail(e.where,"invalid concurrency closure signature");
  std::set<std::string> names;
  for(const auto& c:e.captures){
   if(!names.insert(c.name).second)fail(e.where,"duplicate closure capture");
   const auto& b=local(c.name,e.where);
   if(b.type!=c.type||b.mut)fail(e.where,"invalid/mutable launch or closure capture");
     if(launched&&!c.type.is_csp_endpoint()&&!payload_supported(c.type))
      fail(e.where,"launch captures support only immutable fixed-size values or scoped endpoints");
     if(contains(c.type)){
      if(!launched||!c.type.is_csp_endpoint()||parent[b.id]!=live)
       fail(e.where,"invalid or moved endpoint capture: "+c.name);
     parent[b.id]=dead;
   }
  }
  auto outer=std::move(scopes);auto outer_loops=std::move(loops),outer_regions=std::move(regions);
  auto used=std::move(capture_uses);auto saved_groups=groups;
  scopes.clear();loops.clear();regions.clear();capture_uses.clear();groups=launched?1:0;scopes.emplace_back();State state;
  for(const auto& c:e.captures)bind(c.name,c.type,false,state,e.where,true);
  for(std::size_t i=0;i<e.parameters.size();++i){
   const auto& p=e.parameters[i];if(p.type!=e.type.parameters()[i])fail(p.where,"closure parameter type mismatch");
   if(p.pattern)pattern(*p.pattern,p.type,state);else bind(p.name,p.type,false,state,p.where);
  }
  auto after=body(e.body,state,depth+1,false);
  if(after)(void)project(*after,0,e.where);
  if(names!=capture_uses)fail(e.where,"closure capture list does not match its body");
  scopes=std::move(outer);loops=std::move(outer_loops);regions=std::move(outer_regions);
  capture_uses=std::move(used);groups=saved_groups;
 }
 void frame_scalar(const Expr& e,const std::set<std::string>& slots,std::size_t depth){
  budget.tick(e.where,depth);
  if(e.type!=Type::I64||e.prototype||e.callee||!e.body.empty()||
     !e.parameters.empty()||!e.captures.empty()||!e.proofs.empty()||
     e.borrow!=BorrowMode::Value||e.move_out)
   fail(e.where,"stackless timer frame requires pure i64 arithmetic and immutable i64 locals");
  auto n=e.children.size();
  if(e.kind==E::Integer&&!n&&e.reference==Expr::Reference::Unresolved){
   (void)integer_literal_bits(e.text,e.where);return;
  }
  if(e.kind==E::Variable&&!n&&e.reference==Expr::Reference::Local&&slots.contains(e.text))return;
  if(e.reference==Expr::Reference::Unresolved&&
     ((e.kind==E::Unary&&n==1&&e.text=="-")||
      (e.kind==E::Binary&&n==2&&(e.text=="+"||e.text=="-"||e.text=="*"||e.text=="/"||e.text=="%")))){
   for(const auto& child:e.children){
    if(!child)fail(e.where,"missing stackless arithmetic operand");
    frame_scalar(*child,slots,depth+1);
   }
   return;
  }
  fail(e.where,"stackless timer frame requires pure i64 arithmetic and immutable i64 locals");
 }
 void timer_frame(const Expr& e,std::size_t depth){
  if(e.kind!=E::Lambda||e.type!=Type::function({},Type::I64)||e.result!=Type::I64||
     !e.parameters.empty()||!e.captures.empty()||!e.children.empty()||e.prototype||
     !e.type_parameters.empty()||!e.constraints.empty())
   fail(e.where,"stackless timer frame requires an i64 block with no captures");
  if(e.body.size()>128)fail(e.where,"stackless timer frame statement limit (128) exceeded");
  std::set<std::string> slots;
  std::size_t state=0;
  bool returned=false;
  for(const auto& s:e.body){
   budget.tick(s.where,depth);
   if(returned||s.prototype||!s.value||s.pattern||s.mutable_binding||
      s.ref_store||!s.body.empty()||!s.otherwise.empty())
    fail(s.where,"unsupported statement in stackless timer frame");
   const auto& value=*s.value;
   if(s.kind==S::Let){
    if(s.name.empty()||(s.declared!=Type::Invalid&&s.declared!=Type::I64))
     fail(s.where,"stackless frame slots must be simple immutable i64 bindings");
    frame_scalar(value,slots,depth+1);
    if(!slots.insert(s.name).second)fail(s.where,"duplicate stackless frame slot");
   }else if(s.name.empty()&&s.kind==S::Expr&&operation(value)&&
            value.text==(canonical?"frame_sleep":"await_timer")){
    shape(value);
    ++state;
    frame_scalar(*value.children[0],slots,depth+1);
    if(canonical){
     const auto& next=*value.children[1];
     if(next.kind!=E::Integer||next.text!=std::to_string(state))
      fail(value.where,"invalid stackless timer resume state");
     frame_scalar(next,slots,depth+1);
    }
   }else if(s.name.empty()&&s.kind==S::Return){
    frame_scalar(value,slots,depth+1);returned=true;
   }else fail(s.where,"stackless timer frame allows only immutable i64 let, await timer, and return");
  }
  if(!state||!returned)fail(e.where,"stackless timer frame requires an await timer and final i64 result");
 }
  void shape(const Expr& e){
  source(e.where);
  if((canonical&&e.kind!=E::Csp)||e.callee||!e.body.empty()||!e.parameters.empty()||!e.labels.empty()||
     !e.patterns.empty()||!e.captures.empty()||!e.proofs.empty()||e.syntax_tail||!e.syntax_definitions.empty()||
     e.projection||e.control_target||e.noreturn||e.statement_match||e.reference!=Expr::Reference::Unresolved)
   fail(e.where,"invalid/private concurrency expression shape");
     auto count=e.text=="channel"?3:e.text=="send"||e.text=="try_send"||
                e.text=="offer_send"||e.text=="frame_sleep"?2:1;
    if(e.text=="poll")count=static_cast<int>(e.children.size());
     if(e.text=="launch_exec"||e.text=="launch_capture")count=static_cast<int>(e.children.size());
    if(e.children.size()!=static_cast<std::size_t>(count))fail(e.where,"concurrency operand arity mismatch");
   for(const auto& c:e.children)if(!c)fail(e.where,"missing concurrency operand");
   if(e.text!="channel"&&e.result!=Type::Void)
    fail(e.where,"invalid concurrency payload annotation");
   if(e.text=="channel"&&!payload_supported(e.result))
    fail(e.where,"unsupported channel payload annotation: "+payload_diagnostic(e.result));
      Type result=Type::Invalid;
      if(e.text=="poll")result=e.type;
      else if(e.text=="offer_recv"||e.text=="offer_send"||e.text=="offer_after"||
              e.text=="await_timer"||e.text=="frame_sleep")result=Type::Void;
      else if(e.text=="launch"||e.text=="launch_async")result=Type::Fiber;
      else if(e.text=="launch_exec")result=Type::ExternalFiber;
      else if(e.text=="launch_capture")
       result=Type::tuple({Type::ExternalFiber,Type::sender(Type::U8),
                          Type::receiver(Type::U8),Type::receiver(Type::U8)});
      else if(e.text=="join")result=Type::I64;
      else if(e.text=="close"||e.text=="cancel")result=Type::Bool;
      else if(e.text=="send")result=send_result();
      else if(e.text=="try_send")result=try_send_result();
      else if(e.text=="recv"&&e.children[0]->type.kind()==Type::Receiver)
       result=receive_result(e.children[0]->type.csp_payload());
      else if(e.text=="try_recv"&&e.children[0]->type.kind()==Type::Receiver)
       result=try_receive_result(e.children[0]->type.csp_payload());
      else if(e.text=="split"&&e.children[0]->type.kind()==Type::Channel){
       const auto& descriptor=e.children[0]->type;
       if(!descriptor.sender_cardinality()||!descriptor.receiver_cardinality())
        fail(e.where,"split descriptor has unresolved endpoint cardinality");
       result=split_result(descriptor);
      }
      else if(e.text=="channel"){
       if(e.children[0]->kind!=E::Integer||e.children[1]->kind!=E::Integer)
        fail(e.where,"channel cardinalities must be integer literals");
       auto senders=integer_literal_bits(e.children[0]->text,e.children[0]->where);
       auto receivers=integer_literal_bits(e.children[1]->text,e.children[1]->where);
       if(senders<=0||receivers<=0||senders>256||receivers>256||
          senders+receivers>256)
        fail(e.where,"channel endpoint cardinality mismatch");
       result=Type::channel(e.result,static_cast<std::size_t>(senders),
                            static_cast<std::size_t>(receivers));
      }
    if(result==Type::Invalid||result!=e.type)fail(e.where,"unknown concurrency operation or result type");
  }
 bool expression(const Expr& e,State& state,std::size_t depth,bool acquire=false){
  budget.tick(e.where,depth);if(e.prototype)return true;
   if(operation(e)){
    shape(e);
    if(e.text=="poll"){
      if(e.children.empty()||e.children.size()>18)fail(e.where,"poll offer limit/arity mismatch");
      bool deadline=false;
     std::vector<Type> fields;std::vector<std::string> labels;std::set<std::size_t> seen;
     for(std::size_t i=0;i<e.children.size();++i){const auto& offer=*e.children[i];
       if(offer.kind==E::Boolean&&offer.text=="true"&&
          i+1==e.children.size()){
        const bool source_type=offer.type==Type::Invalid||offer.type==Type::Bool;
        if(!offer.children.empty()||offer.callee||offer.result!=Type::Void||
           (canonical?offer.type!=Type::Bool:!source_type))
         fail(offer.where,"invalid poll default marker");
        fields.push_back(Type::data(Type::Record,{}));labels.push_back("Default");break;
       }
       if(operation(offer)&&offer.text=="offer_after"){
        shape(offer);
        if(deadline||i+2<e.children.size()||(i+2==e.children.size()&&e.children.back()->kind!=E::Boolean))fail(offer.where,"poll deadline must be unique and follow channel offers");
        deadline=true;
        if(!expression(*offer.children[0],state,depth+1)||offer.children[0]->type!=Type::I64)fail(offer.where,"poll duration must be a completing i64");
        for(const auto id:seen)if(state.at(id)!=live)fail(offer.where,"poll deadline moved an offered endpoint");
        fields.push_back(Type::data(Type::Record,{}));labels.push_back("After");continue;
       }
      if(!operation(offer)||(offer.text!="offer_recv"&&offer.text!="offer_send"))fail(offer.where,"invalid poll offer");
      shape(offer);
      if(offer.children.empty()||offer.children.size()!=(offer.text=="offer_recv"?1u:2u))fail(offer.where,"poll operand arity mismatch");
      const auto& endpoint=*offer.children[0];
       bool receive=offer.text=="offer_recv";
        if(receive?endpoint.type.kind()!=Type::Receiver:endpoint.type.kind()!=Type::Sender)
         fail(offer.where,"poll endpoint direction/type mismatch");
       capability(endpoint,endpoint.type,state,false);
      const auto& binding=local(endpoint.text,endpoint.where);
       if(seen.size()>=16)fail(offer.where,"poll channel offer limit exceeded");
       if(!seen.insert(binding.id).second)fail(offer.where,"duplicate poll endpoint");
        Type payload=endpoint.type.csp_payload();
       if(!receive&&(!expression(*offer.children[1],state,depth+1)||offer.children[1]->type!=payload))fail(offer.where,"poll send requires a completing exact endpoint payload");
      for(const auto id:seen)if(state.at(id)!=live)fail(offer.where,"poll payload moved an offered endpoint");
       fields.push_back(receive?payload:Type::data(Type::Record,{}));
      labels.push_back((offer.text=="offer_recv"?"Recv":"Sent")+std::to_string(i));
      fields.push_back(Type::data(Type::Record,{}));labels.push_back("Closed"+std::to_string(i));
     }
     if((seen.empty()&&!deadline)||Type::data(Type::Sum,std::move(fields),std::move(labels))!=e.type)fail(e.where,"poll result/offer type mismatch");
     return true;
    }
     if(e.text=="offer_recv"||e.text=="offer_send"||e.text=="offer_after")fail(e.where,"poll offer outside a checked poll");
     if(e.text=="await_timer"||e.text=="frame_sleep")
      fail(e.where,"await timer is only permitted as a statement in launch async");
      if(e.text=="launch_exec"||e.text=="launch_capture"){
       if(!acquire||!groups||e.children.empty()||e.children.size()>9)fail(e.where,"external launch must bind its capabilities inside a taskgroup (1..9 literal argv items)");
       std::vector<std::string> arguments;std::size_t total=0;
       for(const auto& a:e.children){
        if(a->kind!=E::String||a->type!=Type::String)fail(a->where,"external argv must be bounded constant strings");
         auto value=argument(*a);
         if(value.size()>4096)fail(a->where,"external argv must be bounded NUL-free constant strings");
        if(value.size()+1>std::numeric_limits<std::size_t>::max()-total)fail(a->where,"external argv byte count overflow");
        total+=value.size()+1;arguments.push_back(std::move(value));
       }
       if(arguments[0].empty()||arguments[0][0]!='/')fail(e.children[0]->where,"external executable path must be absolute");
       if(total>16384)fail(e.where,"external command exceeds 16384 decoded bytes");
       return true;
      }
    const auto& operand=*e.children[0];
   if(e.text=="launch_async"){
    if(!acquire||!groups)fail(e.where,"launch async must bind an ifiber<i64> inside a taskgroup");
    timer_frame(operand,depth+1);
   }else if(e.text=="launch"){
    if(!acquire||!groups||operand.kind!=E::Lambda||!operand.parameters.empty()||operand.result!=Type::I64)fail(e.where,"launch must bind an ifiber<i64> inside a taskgroup");
    lambda(operand,state,true,depth+1);
   }else if(e.text=="channel"){
    if(!acquire||!groups)fail(e.where,"channel must bind a descriptor inside a taskgroup");
    for(std::size_t i=0;i<3;++i){
     const auto& c=*e.children[i];
     if(c.kind!=E::Integer||c.type!=Type::I64||!c.children.empty())fail(c.where,"channel cardinality/capacity must be checked i64 literals");
     auto n=integer_literal_bits(c.text,c.where);
      if(i<2?n<=0||n>256:n<0||n>256)
       fail(c.where,"channel requires positive endpoint cardinalities and capacity in 0..256");
     }
     auto senders=integer_literal_bits(e.children[0]->text,e.children[0]->where);
     auto receivers=integer_literal_bits(e.children[1]->text,e.children[1]->where);
     if(senders+receivers>256)fail(e.where,"channel endpoint cardinality total exceeds 256");
    }else if(e.text=="split"){
     if(!acquire)fail(e.where,"split must bind all directional endpoints");
     if(operand.type.kind()!=Type::Channel)fail(e.where,"split requires a channel descriptor");
     capability(operand,operand.type,state,true);
   }else{
      auto t=operand.type;
      if(e.text=="join"&&t!=Type::Fiber&&t!=Type::ExternalFiber)fail(e.where,"join requires a fiber handle");
      if(e.text=="cancel"&&t!=Type::Fiber&&t!=Type::ExternalFiber)fail(e.where,"cancel requires a fiber handle");
       if((e.text=="recv"||e.text=="try_recv")&&t.kind()!=Type::Receiver)
        fail(e.where,"receive requires a receiver capability");
       if((e.text=="send"||e.text=="try_send"||e.text=="close")&&t.kind()!=Type::Sender)
        fail(e.where,"send/close requires a sender capability");
       capability(operand,t,state,e.text=="join"||e.text=="cancel");
      if(e.text=="send"||e.text=="try_send"){
       auto payload=t.csp_payload();
       if(!expression(*e.children[1],state,depth+1)||e.children[1]->type!=payload)fail(e.where,"send payload type mismatch");
     if(state.at(local(operand.text,operand.where).id)!=live)fail(e.where,"send payload moved its endpoint");
    }
   }
   return true;
  }
  if(contains(e.type))fail(e.where,"concurrency capabilities cannot be copied, aggregated or exported");
  if(e.kind==E::Variable&&e.reference==Expr::Reference::Local){
   const auto& b=local(e.text,e.where);if(b.type!=e.type)fail(e.where,"local type mismatch");return true;
  }
  if(e.kind==E::Variable&&e.reference==Expr::Reference::Intrinsic&&(e.text=="io_stdin"||e.text=="tool_read_file"))
   fail(e.where,"ON1X-CSP-UNSUPPORTED: blocking input/file intrinsics are unavailable in concurrent units");
  if(e.kind==E::Lambda){lambda(e,state,false,depth);return true;}
  if(e.kind==E::Region){
   if(e.children.size()!=1||!e.children[0])fail(e.where,"malformed value region");
   scopes.emplace_back();auto after=body(e.body,state,depth+1,false);
   if(after&&expression(*e.children[0],*after,depth+1))state=project(*after,scopes.size()-1,e.where);
   else after.reset();
   scopes.pop_back();return after.has_value();
  }
  if(e.kind==E::Binary&&(e.text=="&&"||e.text=="||")){
   if(e.children.size()!=2||!e.children[0]||!e.children[1])fail(e.where,"malformed short-circuit expression");
   if(!expression(*e.children[0],state,depth+1))return false;
   auto right=state;if(expression(*e.children[1],right,depth+1))state=*merge(state,right,e.where);return true;
  }
  if(e.kind==E::PatternMatch||e.kind==E::Match||e.kind==E::Loop||e.kind==E::Propagate)fail(e.where,"value lowering must precede concurrency admission");
  if(e.callee&&!expression(*e.callee,state,depth+1))return false;
  for(const auto& c:e.children){if(!c)fail(e.where,"missing operand");if(!expression(*c,state,depth+1))return false;}
  if(!e.body.empty())fail(e.where,"unexpected expression body in concurrency admission");
  return true;
 }
  Flow body(const std::vector<Stmt>& b,State initial,std::size_t depth,bool scoped=true){
   if(scoped)scopes.emplace_back();
   Flow state=std::move(initial);
   for(const auto& s:b){
    budget.tick(s.where,depth);if(s.prototype)continue;if(!state)fail(s.where,"unreachable statement in concurrency admission");
    budget.state(state->size(),s.where);
   if(s.kind==S::TaskGroup||s.kind==S::CspScope){
    source(s.where);
     if((canonical&&s.kind!=S::CspScope)||s.pattern||!s.name.empty()||s.declared!=Type::Invalid||
        s.mutable_binding||!s.otherwise.empty()||s.control_target||!s.label.empty()||s.label_context||s.context||s.inclusive||s.unsafe_block)fail(s.where,"invalid taskgroup shape");
     if(s.value&&(!expression(*s.value,*state,depth+1)||s.value->type!=Type::I64))
      fail(s.value->where,"taskgroup duration must be a completing i64");
    ++groups;state=body(s.body,*state,depth+1);--groups;continue;
   }
   if(s.kind==S::Let&&s.value&&contains(s.value->type)){
    const auto& e=*s.value;
     if(s.mutable_binding||!operation(e))fail(s.where,"affine bindings must originate from launch/channel/split, never an alias");
    if(s.declared!=Type::Invalid&&s.declared!=e.type)fail(s.where,"affine binding annotation mismatch");
      if(e.text=="split"){
       if(!s.pattern||!s.pattern->is_tuple()||
          s.pattern->elements.size()!=e.type.elements().size())
        fail(s.where,"split pattern must bind every declared directional endpoint");
       for(const auto& p:s.pattern->elements)
        if(p.kind!=Pattern::Kind::Name||!p.elements.empty())
         fail(p.where,"split endpoints require simple names");
      }else if(e.text=="launch_capture"){
       if(!s.pattern||!s.pattern->is_tuple()||s.pattern->elements.size()!=4)fail(s.where,"captured shell launch requires exactly (process,stdin,stdout,stderr)");
       for(const auto& p:s.pattern->elements)if(p.kind!=Pattern::Kind::Name||!p.elements.empty())fail(p.where,"captured process capabilities require four simple names");
      }else if((e.text!="channel"&&e.text!="launch"&&e.text!="launch_async"&&e.text!="launch_exec")||s.pattern)fail(s.where,"invalid affine acquisition binding");
    (void)expression(e,*state,depth+1,true);
      if(s.pattern)pattern(*s.pattern,e.type,*state);else bind(s.name,e.type,false,*state,s.where);
    continue;
   }
   if(contains(s.declared))fail(s.where,"concurrency storage/assignment/annotation is outside an affine acquisition");
   if(s.kind==S::Expr&&s.value&&operation(*s.value)&&s.value->text=="send")fail(s.where,"send outcome must be handled explicitly (match or let); statement error effects are not yet supported");
   auto header=state;
     if(s.value&&s.kind!=S::Template&&!expression(*s.value,*state,depth+1)){state.reset();continue;}
   switch(s.kind){
   case S::Let:if(!s.value)fail(s.where,"missing binding value");if(s.pattern)pattern(*s.pattern,s.value->type,*state);else bind(s.name,s.value->type,s.mutable_binding,*state,s.where);break;
   case S::Storage:bind(s.name,s.declared,false,*state,s.where);break;
   case S::Assign:case S::Initialize:if(contains(local(s.name,s.where).type))fail(s.where,"affine reassignment is unsupported");break;
   case S::Return:(void)project(*state,0,s.where);state.reset();break;
   case S::Trap:state.reset();break;
   case S::If:{auto a=body(s.body,*state,depth+1),n=body(s.otherwise,*state,depth+1);state=merge(a,n,s.where);break;}
   case S::Block:state=body(s.body,*state,depth+1);break;
   case S::Template:state=body(s.body,*state,depth+1,false);break;
   case S::Region:{
    regions.push_back({s.control_target,scopes.size(),{},{}});
    auto after=body(s.body,*state,depth+1);auto exits=std::move(regions.back().exits);regions.pop_back();
    for(const auto& exit:exits)after=merge(after,exit,s.where);
    state=std::move(after);break;
   }
   case S::While:{
    loops.push_back({s.control_target,scopes.size(),{},{}});
    auto after=body(s.body,*state,depth+1);
    for(const auto& c:loops.back().continues)after=merge(after,c,s.where);
    if(after){
     after=body(s.otherwise,*after,depth+1);
     if(after&&*after!=*header)fail(s.where,"repeatable loop backedge changes an enclosing affine capability");
    }
    auto exits=std::move(loops.back().exits);loops.pop_back();
    if(s.value&&s.value->kind==E::Boolean&&s.value->text=="true")state.reset();
    for(const auto& exit:exits)state=merge(state,exit,s.where);
    break;
   }
   case S::Break:case S::Continue:case S::Leave:{
    auto& targets=s.kind==S::Leave?regions:loops;
    auto t=std::find_if(targets.rbegin(),targets.rend(),[&](const auto& p){return p.id==s.control_target;});
    if(t==targets.rend())fail(s.where,"missing affine flow target");
    auto exit=project(*state,t->keep,s.where);
    (s.kind==S::Continue?t->continues:t->exits).push_back(std::move(exit));state.reset();break;
   }
   case S::Range:fail(s.where,"unlowered range in concurrency admission");
   case S::Defer:fail(s.where,"defer lowering must precede concurrency admission");
   case S::Expr:case S::PushDefer:break;
   case S::TaskGroup:case S::CspScope:break;
   }
  }
  if(scoped){if(state)state=project(*state,scopes.size()-1,b.empty()?Location{"<taskgroup>"}:b.back().where);scopes.pop_back();}
  return state;
 }
public:
 explicit Verify(bool c):canonical(c){}
 void run(const Unit& u){
  for(const auto& f:u.functions){
   if(f.prototype)continue;
   if(f.external)fail(f.where,"ON1X-CSP-UNSUPPORTED: raw foreign effects are unavailable in concurrent units");
   if(contains(f.result))fail(f.where,"concurrency handles cannot escape through function results");
   scopes.emplace_back();State state;
   for(const auto& p:f.params){
    if(contains(p.type))fail(p.where,"concurrency handles cannot cross ordinary function signatures");
    if(p.pattern)pattern(*p.pattern,p.type,state);else bind(p.name,p.type,false,state,p.where);
   }
   auto after=body(f.body,state,0,false);if(after)(void)project(*after,0,f.where);
   scopes.pop_back();
  }
 }
};
template<class F>void walk(Unit& u,F apply){
 Budget budget;
 auto expr=[&](auto&& self,Expr& e,std::size_t depth)->void{
  budget.tick(e.where,depth);apply(e);
  if(e.callee)self(self,*e.callee,depth+1);
  for(auto& c:e.children){if(!c)fail(e.where,"missing expression child");self(self,*c,depth+1);}
  auto body=[&](auto&& visit,std::vector<Stmt>& b,std::size_t d)->void{for(auto& s:b){budget.tick(s.where,d);apply(s);if(s.value&&s.kind!=S::Template)self(self,*s.value,d+1);visit(visit,s.body,d+1);visit(visit,s.otherwise,d+1);}};body(body,e.body,depth+1);
 };
 auto body=[&](auto&& visit,std::vector<Stmt>& b,std::size_t d)->void{for(auto& s:b){budget.tick(s.where,d);apply(s);if(s.value&&s.kind!=S::Template)expr(expr,*s.value,d+1);visit(visit,s.body,d+1);visit(visit,s.otherwise,d+1);}};
 for(auto& f:u.functions)if(!f.prototype)body(body,f.body,0);
}
std::optional<Location> unique_ownership(const Unit& unit){
 Budget budget;std::optional<Location> found;
 auto note=[&](const Type& type,const Location& at){
  if(!found&&type.contains_unique())found=at;
 };
 auto pattern=[&](auto&& self,const Pattern& p,std::size_t depth)->void{
  budget.tick(p.where,depth);note(p.type,p.where);
  for(const auto& child:p.elements)self(self,child,depth+1);
 };
 auto expression=[&](auto&& self,const Expr& e,std::size_t depth)->void{
  budget.tick(e.where,depth);note(e.type,e.where);note(e.result,e.where);
  for(const auto& capture:e.captures)note(capture.type,e.where);
  for(const auto& p:e.patterns)pattern(pattern,p,depth+1);
  if(e.callee)self(self,*e.callee,depth+1);
  for(const auto& child:e.children)if(child)self(self,*child,depth+1);
  auto body=[&](auto&& visit,const std::vector<Stmt>& statements,std::size_t level)->void{
   for(const auto& s:statements){
    budget.tick(s.where,level);note(s.declared,s.where);
    if(s.pattern)pattern(pattern,*s.pattern,level+1);
    if(s.value&&s.kind!=S::Template)self(self,*s.value,level+1);
    visit(visit,s.body,level+1);visit(visit,s.otherwise,level+1);
   }
  };
  body(body,e.body,depth+1);
 };
 auto body=[&](auto&& visit,const std::vector<Stmt>& statements,std::size_t depth)->void{
  for(const auto& s:statements){
   budget.tick(s.where,depth);note(s.declared,s.where);
   if(s.pattern)pattern(pattern,*s.pattern,depth+1);
   if(s.value&&s.kind!=S::Template)expression(expression,*s.value,depth+1);
   visit(visit,s.body,depth+1);visit(visit,s.otherwise,depth+1);
  }
 };
 for(const auto& f:unit.functions)if(!f.prototype){
  note(f.result,f.where);
  for(const auto& p:f.params)note(p.type,p.where);
  body(body,f.body,0);
 }
 return found;
}
}
bool present(const Unit& u){
 Budget budget;bool found=false;
 auto expr=[&](auto&& self,const Expr& e,std::size_t depth)->void{
  budget.tick(e.where,depth);if(operation(e)||contains(e.type)||contains(e.result))found=true;
  if(e.callee)self(self,*e.callee,depth+1);
  for(const auto& c:e.children){if(!c)fail(e.where,"missing expression child");self(self,*c,depth+1);}
  auto body=[&](auto&& visit,const std::vector<Stmt>& b,std::size_t d)->void{for(const auto& s:b){budget.tick(s.where,d);if(s.kind==S::TaskGroup||s.kind==S::CspScope||contains(s.declared))found=true;if(s.value&&s.kind!=S::Template)self(self,*s.value,d+1);visit(visit,s.body,d+1);visit(visit,s.otherwise,d+1);}};body(body,e.body,depth+1);
 };
 auto body=[&](auto&& visit,const std::vector<Stmt>& b,std::size_t d)->void{for(const auto& s:b){budget.tick(s.where,d);if(s.kind==S::TaskGroup||s.kind==S::CspScope||contains(s.declared))found=true;if(s.value&&s.kind!=S::Template)expr(expr,*s.value,d+1);visit(visit,s.body,d+1);visit(visit,s.otherwise,d+1);}};
 for(const auto& f:u.functions)if(!f.prototype){if(contains(f.result))found=true;for(const auto& p:f.params)if(contains(p.type))found=true;body(body,f.body,0);}
 return found;
}
void verify(const Unit& u,bool canonical){
 if(!present(u))return;
 if(auto at=unique_ownership(u))
  fail(*at,"unique ownership cannot cross cooperative concurrency until cleanup state is task-local");
 Verify(canonical).run(u);
}
void lower(Unit& u){
 if(!present(u))return;
 verify(u,false);std::size_t budget=262144;auto copy=clone_unit(u,budget);
 walk(copy,[]<class T>(T& node){
  if constexpr(std::is_same_v<T,Expr>){
   if(node.kind==E::SourceCsp&&node.text=="poll"&&!node.children.empty()){
    auto& marker=*node.children.back();
    if(marker.kind==E::Boolean&&marker.text=="true")
     marker.type=Type::Bool;
   }
   if(node.kind==E::SourceCsp&&node.text=="launch_async"&&node.children.size()==1&&node.children[0]){
    std::size_t state=0;
    for(auto& s:node.children[0]->body){
     if(s.kind!=S::Expr||!s.value||s.value->kind!=E::SourceCsp||s.value->text!="await_timer")continue;
     auto next=std::make_unique<Expr>();next->kind=E::Integer;next->type=Type::I64;
     next->where=s.value->where;next->text=std::to_string(++state);
     s.value->text="frame_sleep";s.value->children.push_back(std::move(next));
    }
   }
   if(node.kind==E::SourceCsp)node.kind=E::Csp;
  }
  else if(node.kind==S::TaskGroup)node.kind=S::CspScope;
 });
 verify(copy,true);u=std::move(copy);
}
}
