#pragma once
#include <on1x/ast.hpp>
#include <string_view>

namespace on1x {
// The parser needs at most one binding before, and one after, an operand.
// Binary and ternary infix and unary postfix share the latter syntax slot.
class OperatorScope {
 struct Slots {
  std::optional<OperatorBinding> prefix,trailing;
 };
 std::map<std::string,Slots,std::less<>> bindings;
public:
 bool add(const OperatorBinding& binding,const Location& at){
  auto& slots=bindings[binding.header.lexeme];
  auto& slot=binding.header.position==OperatorPosition::Prefix?slots.prefix:slots.trailing;
  if(slot){
   if(slot->identity==binding.identity)return false;
   throw Error(at,"conflicting operator '"+binding.header.lexeme+
                  "' (previous declaration at "+slot->header.where.file+":"+
                  std::to_string(slot->header.where.line)+":"+
                  std::to_string(slot->header.where.column)+")",
               "ON1X-OPERATOR-CONFLICT");
  }
  slot=binding;
  return true;
 }
 const OperatorBinding* find(std::string_view lexeme,OperatorPosition position)const{
  auto it=bindings.find(lexeme);
  if(it==bindings.end())return nullptr;
  const auto& slot=position==OperatorPosition::Prefix?it->second.prefix:it->second.trailing;
  return slot&&slot->header.position==position?&*slot:nullptr;
 }
 const OperatorBinding* trailing(std::string_view lexeme)const{
  auto it=bindings.find(lexeme);
  return it==bindings.end()||!it->second.trailing?nullptr:&*it->second.trailing;
 }
 const OperatorBinding* reference(std::string_view lexeme,const Location& at)const{
  auto it=bindings.find(lexeme);
  if(it==bindings.end())return nullptr;
  const auto& slots=it->second;
  if(slots.prefix&&slots.trailing)
   throw Error(at,"ambiguous operator reference '"+std::string(lexeme)+"'",
               "ON1X-OPERATOR-AMBIGUOUS");
  return slots.prefix?&*slots.prefix:&*slots.trailing;
 }
 void apply(const FixityDirective& directive){
  auto it=bindings.find(directive.lexeme);
  if(it==bindings.end()||!it->second.trailing||
     it->second.trailing->header.position!=OperatorPosition::Infix)
   throw Error(directive.where,"fixity directive requires a visible infix operator: '"+directive.lexeme+"'",
               "ON1X-OPERATOR-FIXITY");
  if(directive.tightness>9)
   throw Error(directive.where,"operator tightness must be in 0..9","ON1X-OPERATOR-FIXITY");
  auto& header=it->second.trailing->header;
  header.tightness=directive.tightness;
  header.binds=directive.binds;
 }
 template<class F> void each(F&& f)const{
  for(const auto& [lexeme,slots]:bindings){
   (void)lexeme;
   if(slots.prefix)f(*slots.prefix);
   if(slots.trailing)f(*slots.trailing);
  }
 }
};

inline void record_operator_scope(Unit& unit,const OperatorScope& scope,const std::string& key){
 auto& recorded=unit.operator_scopes[key];
 recorded.clear();
 scope.each([&](const OperatorBinding& binding){recorded.push_back(binding);});
}

 // Injectively encode physical declaration origin and, when present, its
 // host-assigned macro instance as a hidden symbol. Canonical file names
 // come from the loader; direct parsing can use a synthetic file name.
inline std::string operator_identity(const OperatorHeader& header){
 static constexpr std::string_view hex="0123456789abcdef";
 std::string result="@operator_";
 result.reserve(result.size()+2*header.where.file.size()+24);
 for(unsigned char ch:header.where.file){
  result+=hex[ch>>4];result+=hex[ch&15];
 }
  result+='_';result+=std::to_string(header.where.offset);
  if(header.macro_instance){result+="_m";result+=std::to_string(header.macro_instance);}
 return result;
}

struct OperatorDiscovery {
 std::vector<Import> imports;
 std::vector<OperatorHeader> operators;
};
OperatorDiscovery discover_operators(const std::vector<Token>&);
Unit parse_with_operators(const std::vector<Token>&,const std::vector<std::string>& enabled,
                          std::vector<Docstring>,OperatorScope);
}
