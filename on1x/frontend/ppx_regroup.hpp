#pragma once
#include <on1x/meta.hpp>
#include <tuple>
#include <unordered_map>

namespace on1x::meta {
// Only the host can make these pairs. An unpaired (explicit PPX) tree has no
// precedence semantics other than its actual grouping.
using PpxSourcePairs = std::unordered_map<const Expr*, const Expr*>;
using PpxSourceLocation = std::tuple<std::string, std::size_t, std::size_t, std::size_t>;
inline auto ppx_source_location(const Location& at){
 return std::tie(at.file,at.offset,at.line,at.column);
}

class PpxRegroup {
 using Form=OperatorParse::Form;
 using Fixity=std::pair<unsigned,OperatorAssociativity>;
 using DeclarationRank=std::tuple<unsigned,unsigned,OperatorAssociativity>;
 std::map<std::string,DeclarationRank> original;
 std::map<PpxSourceLocation,Fixity> original_directives;
 std::size_t original_directive_count=0;
 std::map<std::string,DeclarationRank> changes;
 std::map<PpxSourceLocation,Fixity> directive_changes;

 static DeclarationRank rank(const OperatorHeader& h){return {h.precedence,h.tightness,h.binds};}
 Fixity fixity(const Expr& e,const OperatorParse& syntax) const{
  if(syntax.form==Form::Suffix)return {200,OperatorAssociativity::Left};
  auto result=Fixity{syntax.rank,syntax.binds};
  auto decl=changes.find(e.operator_identity);
  if(decl!=changes.end()){
   const auto [precedence,tightness,binds]=decl->second;
   result.first=precedence*10+(syntax.directive?syntax.rank%10:tightness);
   if(syntax.form==Form::Prefix||syntax.form==Form::Postfix)result.first+=100;
   if(!syntax.directive)result.second=binds;
  }
  if(syntax.directive){
   auto from=directive_changes.find(ppx_source_location(*syntax.directive));
   if(from!=directive_changes.end()){
    result.first=(result.first/10)*10+from->second.first;
    result.second=from->second.second;
   }
  }
  return result;
 }
 static ExprPtr& edge(Expr& e,Form form,bool right){
  if(right)return e.children[form==Form::Ternary?2:form==Form::Infix?1:0];
  return e.kind==Expr::Kind::Call?e.callee:e.children[0];
 }
 static const Expr* edge(const Expr& e,Form form,bool right){
  if(right)return e.children[form==Form::Ternary?2:form==Form::Infix?1:0].get();
  return e.kind==Expr::Kind::Call?e.callee.get():e.children[0].get();
 }
 static bool has_edge(Form form,bool right){
  if(right)return form==Form::Prefix||form==Form::Infix||form==Form::Ternary;
  return form!=Form::Prefix;
 }
 static const Expr* linked(const Expr& source,bool right){
  const auto& syntax=*source.operator_parse;
  const auto& stamp=right?syntax.right:syntax.left;
  if(!stamp||!has_edge(syntax.form,right))return nullptr;
  const auto* child=edge(source,syntax.form,right);
  // A macro may replace an operand, but cannot create a parser-authored link.
  return child&&!child->operator_grouped&&child->operator_parse==stamp?child:nullptr;
 }
 struct Piece {
  ExprPtr node;
  // No form on operands; their grouping (including any explicit PPX operators)
  // is preserved. Ternary middles reside in the operator node, not in the chain.
  std::optional<Form> form;
  std::size_t rank=0;
  OperatorAssociativity binds=OperatorAssociativity::Left;
 };
 struct Replay {
  std::vector<Piece> sequence;
  std::size_t cursor=0;
  Location where;
  [[noreturn]] void fail() const{
   throw Error(where,"PPX operator regrouping exceeds source expression bounds",
               "ON1X-OPERATOR-PPX");
  }
  ExprPtr parse(std::size_t minimum=0,std::size_t depth=0){
   if(depth>=max_syntax_depth||cursor==sequence.size())fail();
   auto& start=sequence[cursor++];
   auto lhs=std::move(start.node);
   if(start.form){
    if(*start.form!=Form::Prefix)fail();
    edge(*lhs,*start.form,true)=parse(start.rank,depth+1);
   }
   while(cursor<sequence.size()){
    auto& token=sequence[cursor];
    if(!token.form||*token.form==Form::Prefix||token.rank<minimum)break;
    ++cursor;
    auto operation=std::move(token.node);
    const auto form=*token.form;
    edge(*operation,form,false)=std::move(lhs);
    if(has_edge(form,true))
     edge(*operation,form,true)=parse(token.rank+(token.binds==OperatorAssociativity::Left),depth+1);
    lhs=std::move(operation);
   }
   return lhs;
  }
 };
 struct Transform {
  const PpxRegroup& ranks;
  const PpxSourcePairs& pairs;
  const Expr* paired(const Expr& e) const{
   auto found=pairs.find(&e);
   return found==pairs.end()?nullptr:found->second;
  }
  // Check the whole original island before moving any nodes. A changed token,
  // replacement of a participating sub-tree, or regrouped output edge is a
  // structural PPX edit, never a request to reinterpret an incomplete chain.
  bool intact(const Expr& e,const Expr& source,bool& changed,std::size_t& nodes) const{
   const auto& syntax=*source.operator_parse;
   if(++nodes>max_wire_nodes)return false;
   if(syntax.form!=Form::Suffix&&ranks.fixity(e,syntax)!=Fixity{syntax.rank,syntax.binds})
    changed=true;
   for(bool right:{false,true}){
    if(const auto* authored=linked(source,right)){
     const auto* child=edge(e,syntax.form,right);
     if(!child||paired(*child)!=authored||!intact(*child,*authored,changed,nodes))return false;
    }
   }
   return true;
  }
  // Only process independent operands inside a validated/opaque island. The
  // linked portion will be replayed together, or all left as explicit output.
  void operands(Expr& e,const Expr& source){
   const auto form=source.operator_parse->form;
   auto visit=[&](ExprPtr& part){
    if(!part)return;
    for(bool right:{false,true}){
     if(!has_edge(form,right)||&part!=&edge(e,form,right))continue;
     if(const auto* authored=linked(source,right);authored&&paired(*part)==authored){
      operands(*part,*authored);return;
     }
    }
    expression(part);
   };
   visit(e.callee);
   for(auto& child:e.children)visit(child);
   body(e.body);
  }
  void flatten(ExprPtr node,const Expr& source,Replay& play){
   const auto& syntax=*source.operator_parse;
   const auto [rank,binds]=ranks.fixity(*node,syntax);
   const auto form=syntax.form;
   auto emit=[&](ExprPtr part,bool right){
    if(const auto* authored=linked(source,right))flatten(std::move(part),*authored,play);
    else play.sequence.push_back({std::move(part),std::nullopt});
   };
   ExprPtr left,right;
   if(has_edge(form,false))left=std::move(edge(*node,form,false));
   if(has_edge(form,true))right=std::move(edge(*node,form,true));
   if(left)emit(std::move(left),false);
   play.sequence.push_back({std::move(node),form,rank,binds});
   if(right)emit(std::move(right),true);
  }
  void expression(ExprPtr& e){
   if(!e)return;
   if(const auto* source=paired(*e);source&&source->operator_parse){
    // Use this even if the rank does not change: boundary subexpressions may
    // have independent source provenance and distinct lexical fixity.
    bool changed=false;std::size_t nodes=0;
    const bool regroup=intact(*e,*source,changed,nodes)&&changed;
    operands(*e,*source);
    if(regroup){
     Replay play;play.where=e->where;
     play.sequence.reserve(2*nodes+1);
     flatten(std::move(e),*source,play);
     e=play.parse();
     if(play.cursor!=play.sequence.size())play.fail();
    }
   }else{
    expression(e->callee);
    for(auto& child:e->children)expression(child);
    body(e->body);
   }
  }
  void body(std::vector<Stmt>& statements){
   for(auto& statement:statements){
    expression(statement.value);
    body(statement.body);body(statement.otherwise);
   }
  }
 };
 // Replaying can change height even when node count is identical. Bound
 // downstream recursive walks by the frontend source limit, not PPX wire size.
 static void check_height(const Expr& e,std::size_t depth){
  if(depth>max_syntax_depth)
   throw Error(e.where,"PPX operator regrouping exceeds source expression bounds","ON1X-OPERATOR-PPX");
  if(e.callee)check_height(*e.callee,depth+1);
  for(const auto& child:e.children)check_height(*child,depth+1);
  check_height(e.body,depth+1);
 }
 static void check_height(const std::vector<Stmt>& statements,std::size_t depth){
  for(const auto& statement:statements){
   if(depth>max_syntax_depth)
    throw Error(statement.where,"PPX operator regrouping exceeds source expression bounds","ON1X-OPERATOR-PPX");
   if(statement.value)check_height(*statement.value,depth+1);
   check_height(statement.body,depth+1);check_height(statement.otherwise,depth+1);
  }
 }
 public:
 explicit PpxRegroup(const Unit& unit):original_directive_count(unit.fixity_directives.size()){
  for(const auto& d:unit.operators)original.emplace(d.identity,rank(d.header));
  for(const auto& d:unit.fixity_directives)
   original_directives.emplace(ppx_source_location(d.where),Fixity{d.tightness,d.binds});
 }
 void admit(const Unit& unit){
  for(const auto& d:unit.operators){
   if(auto from=original.find(d.identity);from!=original.end()&&from->second!=rank(d.header))
    changes.emplace(d.identity,rank(d.header));
  }
  for(std::size_t i=0;i<original_directive_count;++i){
   const auto& d=unit.fixity_directives[i];
   const auto key=ppx_source_location(d.where);
   if(auto from=original_directives.find(key);
      from!=original_directives.end()&&from->second!=Fixity{d.tightness,d.binds})
    directive_changes.emplace(key,Fixity{d.tightness,d.binds});
  }
 }
 void apply(Function& f,const PpxSourcePairs& pairs) const{
  if(changes.empty()&&directive_changes.empty())return;
  Transform transform{*this,pairs};
  transform.body(f.body);
  for(auto& attribute:f.attributes)transform.expression(attribute);
  check_height(f.body,1);
  for(const auto& attribute:f.attributes)check_height(*attribute,1);
 }
};
}
