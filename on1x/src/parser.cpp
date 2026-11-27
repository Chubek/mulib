#include <on1x/ast.hpp>
#include <on1x/canonical/control.hpp>
#include <on1x/numeric.hpp>
#include <on1x/literals.hpp>
#include <on1x/module_signature.hpp>
#include <on1x/extensions.hpp>
#include <on1x/operator_scan.h>
#include <on1x/operator_scope.hpp>
#include "module_elevation_fault.hpp"
#include <charconv>
#include <cctype>
#include <algorithm>
#include <deque>
#include <set>
#include <map>
namespace on1x {
namespace {
class Parser {
 const std::vector<Token>& ts;
 std::vector<Docstring>* docstrings=nullptr;
  std::size_t pos=0;
  std::optional<std::size_t> split_at;
  Token remaining_token;
   const OperatorScope* operators=nullptr;
   const Parser* enclosing_parser=nullptr; // A string interpolation shares its authoring scope.
   std::vector<OperatorDeclaration>* declarations=nullptr;
   std::vector<FixityDirective>* directives=nullptr;
   std::size_t* operator_count=nullptr; // Shared by the complete source parse and interpolations.
   bool structural_operator_syntax=false;
  std::size_t lexical_depth=0;
  struct LocalOperator { OperatorBinding binding; std::size_t depth; };
  struct LocalOperators { std::deque<LocalOperator> prefix,trailing; };
  std::map<std::string,LocalOperators,std::less<>> local_operators;
  std::vector<std::pair<std::string,bool>> local_operator_undo;
  std::map<std::string,std::vector<FixityDirective>,std::less<>> fixity;
  std::vector<std::string> fixity_undo;
 std::size_t nesting=0;
 std::size_t lambdas=0;
 std::size_t iteration_names=0;
  std::map<std::string,std::size_t> bound_types;
  std::set<std::string> enabled_extensions,used_extensions;
 bool templating=false;
  bool syntax_atoms=false,literal_ellipsis=false,interpolate=true;
 std::string ellipsis="...";
 // Bound both syntactic recursion and the resulting expression tree. A flat
 // left-associative chain has shallow parser recursion but deep HIR traversal
 // (and recursive unique_ptr destruction) in all later frontend phases.
 struct Nesting {
  Parser& parser;
  explicit Nesting(Parser& p):parser(p){
   if(p.nesting>=max_syntax_depth)
    throw Error(p.peek().where,"syntax nesting exceeds "+std::to_string(max_syntax_depth)+"-level limit");
   ++p.nesting;
  }
  Nesting(const Nesting&)=delete;
  ~Nesting(){--parser.nesting;}
  };
  struct LexicalRegion {
   Parser& parser;
   std::size_t fixity_start,operator_start;
   explicit LexicalRegion(Parser& p):parser(p),fixity_start(p.fixity_undo.size()),
                                    operator_start(p.local_operator_undo.size()){++parser.lexical_depth;}
   LexicalRegion(const LexicalRegion&)=delete;
   ~LexicalRegion(){
    while(parser.fixity_undo.size()>fixity_start){
     auto it=parser.fixity.find(parser.fixity_undo.back());
     it->second.pop_back();
     if(it->second.empty())parser.fixity.erase(it);
     parser.fixity_undo.pop_back();
    }
    while(parser.local_operator_undo.size()>operator_start){
     const auto& [text,prefix]=parser.local_operator_undo.back();
     auto it=parser.local_operators.find(text);
     (prefix?it->second.prefix:it->second.trailing).pop_back();
     if(it->second.prefix.empty()&&it->second.trailing.empty())parser.local_operators.erase(it);
     parser.local_operator_undo.pop_back();
    }
    --parser.lexical_depth;
   }
  };
   // Macro operands are parsed once in their authoring scope. An operator
   // declaration in them is syntax data, not a callable unless spliced.
   struct OperatorSyntax {
    Parser& parser;
    bool saved;
    explicit OperatorSyntax(Parser& p):parser(p),saved(p.structural_operator_syntax){p.structural_operator_syntax=true;}
    ~OperatorSyntax(){parser.structural_operator_syntax=saved;}
   };
  struct Expression { ExprPtr node; std::size_t depth=1; };
 static Expression bounded(ExprPtr node,std::size_t depth){
  if(depth>max_syntax_depth)
   throw Error(node->where,"expression exceeds "+std::to_string(max_syntax_depth)+"-level syntax limit");
  node->syntax_depth=depth;
  return {std::move(node),depth};
 }
 static std::size_t body_expression_depth(const std::vector<Stmt>& body){
  std::vector<const Stmt*> pending;for(const auto& statement:body)pending.push_back(&statement);
  std::size_t depth=0;
  while(!pending.empty()){
   auto statement=pending.back();pending.pop_back();
   if(statement->value)depth=std::max(depth,statement->value->syntax_depth);
   for(const auto& child:statement->body)pending.push_back(&child);
   for(const auto& child:statement->otherwise)pending.push_back(&child);
  }
  return depth;
 }
 const Token& peek(std::size_t offset=0)const{if(!offset&&split_at==pos)return remaining_token;return ts.at(std::min(pos+offset,ts.size()-1));}
 bool at(std::string_view s)const{return peek().text==s;}
 bool accept(std::string_view s){if(at(s)){++pos;return true;}return false;}
  const Token& consume(std::string_view s){if(!at(s))throw Error(peek().where,"expected '"+std::string(s)+"', found '"+peek().text+"'","ON1X-PARSE-EXPECTED",std::string(s),peek().text);const auto& token=peek();++pos;return token;}
 static bool is_ident(const std::string& s){if(s.empty()||!(std::isalpha((unsigned char)s[0])||s[0]=='_'))return false;
  for(char c:s)if(!(std::isalnum((unsigned char)c)||c=='_'))return false;
  return true;}
  std::string ident(){if(templating&&accept("$"))return "$"+ident();
   auto t=peek();if(!is_ident(t.text))throw Error(t.where,"expected identifier");++pos;
     static const std::set<std::string> reserved={
      "fn","proc","let","var","if","else","while","return","true","false",
      "extern","import","forall","macro","rule","quote","splice","lift",
      "let_syntax","letrec_syntax","borrow","mut","ref"};
  if(reserved.count(t.text))throw Error(t.where,"reserved keyword used as identifier: "+t.text);
   return t.text;}
  void split_after_first(){
   if(peek().text.size()==1){++pos;return;}
   if(split_at!=pos)remaining_token=peek();
   remaining_token.text.erase(0,1);++remaining_token.where.offset;++remaining_token.where.column;
   split_at=pos;
  }
  static std::size_t angle_closes(std::string_view token){
   std::size_t n=0;while(n<token.size()&&token[n]=='>')++n;return n;
  }
  bool at_close_angle()const{return peek().text.starts_with('>');}
  void close_angle(){
   if(at_close_angle())split_after_first();
   else consume(">");
  }
  bool typed_data_header(const std::string& end)const{
   // `record`/`variant` remain ordinary value names in comparisons. Commit to
   // the additive typed constructor only after its complete `<type>` prefix.
   if(peek(1).text!="<")return false;
   std::size_t angles=1,groups=0;
   for(std::size_t i=2;pos+i<ts.size();++i){const auto& text=peek(i).text;
    if(text=="<"){if(++angles>max_syntax_depth)return false;}
    else if(auto closes=angle_closes(text);closes){
     if(closes>=angles)return !groups&&closes==angles&&closes==text.size()&&peek(i+1).text==end;
     angles-=closes;
    }
    else if(text=="("||text=="{"||text=="["){if(++groups>max_syntax_depth)return false;}
    else if(text==")"||text=="}"||text=="]"){if(!groups)return false;--groups;}
    else if(!is_ident(text)&&text!=","&&text!=":"&&text!="."&&text!="->"&&
            (text.empty()||!std::isdigit(static_cast<unsigned char>(text[0]))))return false;
   }
   return false;
  }
   std::string qualified_identifier(){
    auto where=peek().where;std::string name=ident();
    while(accept(".")){name+="."+ident();if(name.size()>4096)throw Error(where,"qualified name exceeds 4096 bytes");}
    return name;
   }
   static bool can_group_type(const Type& type){
    switch(type.kind()){
     case Type::Function: case Type::Forall: case Type::Tuple: case Type::Code:
     case Type::Record: case Type::Sum: case Type::Array: case Type::Slice:
     case Type::Own: case Type::Fiber: case Type::Channel: case Type::Sender: case Type::Receiver:
      return true;
     case Type::Named:
      return !type.arguments().empty();
     default:
      return false;
    }
   }
      Type type(){Nesting guard(*this);auto t=peek();
      if((at("ref")||at("borrow"))&&is_ident(peek(1).text))
       throw Error(t.where,
                   "borrow/ref modes are only valid on a named On1x "
                   "function parameter","ON1X-REF");
       if((at("ifiber")||at("channel")||at("sender")||at("receiver"))&&peek(1).text=="<"){
        ++pos;consume("<");auto payload=type();close_angle();
        if(t.text=="ifiber"){
         if(payload!=Type::I64)
         throw Error(t.where,"ON1X-CSP-UNSUPPORTED: internal fiber result must be i64");
        return Type::Fiber;
       }
        return t.text=="channel"?Type::channel(std::move(payload)):
               t.text=="sender"?Type::sender(std::move(payload)):
               Type::receiver(std::move(payload));
       }
      if(at("xfiber")&&peek(1).text!="<"){++pos;return Type::ExternalFiber;}
      if(accept("own")){
       consume("<");auto target=type();close_angle();
       if(target.kind()!=Type::Named)throw Error(t.where,"own<T> requires a nominal record or enum type reference","ON1X-OWN");
       return Type::own(std::move(target));
      }
    if((at("record")||at("sum"))&&peek(1).text=="{"){
     bool sum=accept("sum");if(!sum)consume("record");consume("{");
     std::vector<std::string> labels;std::vector<Type> elements;std::set<std::string> names;
     if(!at("}"))do{
      auto label=ident();if(!names.insert(label).second)throw Error(t.where,"duplicate data label: "+label);
      if(labels.size()>=256)throw Error(t.where,"data field limit exceeded");
      labels.push_back(label);consume(":");elements.push_back(type());
     }while(accept(","));
     consume("}");if(sum&&elements.empty())throw Error(t.where,"sum requires at least one constructor");
     return Type::data(sum?Type::Sum:Type::Record,std::move(elements),std::move(labels));
    }
    if((at("array")||at("slice"))&&peek(1).text=="<"){
     bool slice=accept("slice");if(!slice)consume("array");consume("<");auto element=type();consume(",");auto count=peek();++pos;
     std::size_t extent=0;auto [end,ec]=std::from_chars(count.text.data(),count.text.data()+count.text.size(),extent);
     if(ec!=std::errc{}||end!=count.text.data()+count.text.size()||extent>256)throw Error(count.where,"array/slice extent must be a decimal integer in 0..256");
      close_angle();
     return Type::data(slice?Type::Slice:Type::Array,{std::move(element)},{},extent);
    }
    if(accept("code")){consume("<");auto body=type();
      close_angle();
     return Type::code(std::move(body));}
    if(accept("forall")){
     if(!bound_types.empty())throw Error(t.where,"nested forall binders are not supported");
     consume("(");std::size_t count=0;
     do{auto at=peek().where;auto name=ident();if(!bound_types.emplace(name,count++).second)throw Error(at,"duplicate quantified type variable: "+name);}while(accept(","));
     consume(")");auto body=type();bound_types.clear();
     if(!body.is_function()||body.has_foralls()||body.has_holes())throw Error(t.where,"forall requires a fully specified function body");
     return Type::for_all(count,std::move(body));
    }
   if(accept("(")){
    auto first=type();
    if(!accept(",")){
     consume(")");
     if(!can_group_type(first))
      throw Error(t.where,"type grouping requires a complex type; remove these parentheses",
                  "ON1X-PARSE-TYPE");
     return first;
    }
    std::vector<Type> elements;elements.push_back(std::move(first));
    do{elements.push_back(type());}while(accept(","));
    consume(")");return Type::tuple(std::move(elements));
   }
   if(accept("fn")){
    consume("(");std::vector<Type> parameters;
    if(!at(")"))do{parameters.push_back(type());}while(accept(","));
    consume(")");consume("->");auto result=type();
    return Type::function(std::move(parameters),std::move(result));
   }
   if(at("proc"))throw Error(t.where,"'proc' is reserved for definitions; use 'fn(...) -> void' for a procedure type","ON1X-PARSE-TYPE");
    ++pos;
    if(auto found=bound_types.find(t.text);found!=bound_types.end())return Type::bound(found->second);
   if(t.text=="_")return Type::Hole;
     for(auto x:{Type::I64,Type::F64,Type::F32,Type::U8,Type::U16,Type::U32,Type::U64,Type::Bool,Type::String,Type::Char,Type::Void,Type::Json,Type::JsonView,Type::SExpr,Type::Buffer})if(t.text==type_name(x))return x;
    if(is_ident(t.text)){
     // Parameter names are not part of a function *type*. Preserve the prior
     // located diagnostic rather than interpreting `x: T` as a named type.
     if(at(":"))throw Error(t.where,"unknown type: "+t.text,"ON1X-PARSE-TYPE",{},t.text);
     std::string name=t.text;while(accept(".")){name+="."+ident();if(name.size()>4096)throw Error(t.where,"qualified type name exceeds 4096 bytes");}
     std::vector<Type> arguments;
     if(accept("<")){if(!at_close_angle())do{arguments.push_back(type());if(arguments.size()>32)throw Error(t.where,"nominal type argument limit exceeded");}while(accept(","));close_angle();}
     return Type::named(std::move(name),std::move(arguments));
    }
     throw Error(t.where,"unknown type: "+t.text,"ON1X-PARSE-TYPE",{},t.text);
  }
   ClicheConstraint cliche_constraint(){
    ClicheConstraint result;result.where=peek().where;result.name=qualified_identifier();
    consume("<");
     if(!at_close_angle())do{
     result.arguments.push_back(type());
     if(result.arguments.size()>32)throw Error(result.where,"cliche argument limit exceeded","ON1X-CLICHE-LIMIT");
    }while(accept(","));
    close_angle();
    if(result.arguments.empty())
     throw Error(result.where,"cliche constraints require an explicit Self argument","ON1X-CLICHE-SYNTAX");
    return result;
   }
   std::vector<TypeParameter> generic_parameters(std::vector<ClicheConstraint>& constraints){
    std::vector<TypeParameter> result;
    if(!accept("<"))return result;
     if(at_close_angle())throw Error(peek().where,"generic parameter list cannot be empty","ON1X-CLICHE-SYNTAX");
    do{
     TypeParameter parameter;parameter.where=peek().where;parameter.name=ident();
     if(result.size()>=32)throw Error(parameter.where,"generic parameter limit exceeded","ON1X-CLICHE-LIMIT");
     const auto index=bound_types.size();
     if(!bound_types.emplace(parameter.name,index).second)
      throw Error(parameter.where,"duplicate generic type parameter: "+parameter.name,"ON1X-CLICHE-SYNTAX");
     if(accept(":"))do{
      ClicheConstraint bound;bound.where=peek().where;bound.name=qualified_identifier();
      bound.arguments.push_back(Type::bound(index));
      if(accept("<")){
       if(!at_close_angle())do{
        bound.arguments.push_back(type());
        if(bound.arguments.size()>32)throw Error(bound.where,"cliche bound argument limit exceeded","ON1X-CLICHE-LIMIT");
       }while(accept(","));
       close_angle();
      }
      parameter.bounds.push_back(bound);constraints.push_back(std::move(bound));
     }while(accept("+"));
     result.push_back(std::move(parameter));
    }while(accept(","));
    close_angle();return result;
   }
   void where_constraints(std::vector<ClicheConstraint>& constraints){
    if(!accept("where"))return;
    do{
     constraints.push_back(cliche_constraint());
     if(constraints.size()>128)throw Error(constraints.back().where,"constraint limit exceeded","ON1X-CLICHE-LIMIT");
    }while(accept(","));
   }
   bool cliche_member_header()const{
    if(!is_ident(peek().text))return false;
    std::size_t i=1;
    while(peek(i).text=="."){
     if(!is_ident(peek(i+1).text))return false;
     i+=2;
    }
    if(peek(i).text!="<")return false;
    std::size_t angles=0,groups=0;
    for(;pos+i<ts.size();++i){
     const auto& token=peek(i).text;
     if(token=="<")++angles;
      else if(auto closes=angle_closes(token);closes){
       if(closes>=angles)return closes==angles&&closes==token.size()&&peek(i+1).text=="::"&&is_ident(peek(i+2).text);
       angles-=closes;
      }else if(token=="("||token=="["||token=="{")++groups;
      else if(token==")"||token=="]"||token=="}"){
       if(!groups)return false;
       --groups;
      }else if(token=="<eof>"||token==";")return false;
    }
    return false;
   }
  static int precedence(const std::string& op){if(op=="||")return 1;if(op=="&&")return 2;
  if(op=="=="||op=="!=")return 3;
   if(op=="<"||op==">"||op=="<="||op==">="||op=="<-")return 4;
  if(op=="+"||op=="-")return 5;
  if(op=="*"||op=="/"||op=="%")return 6;
    return -1;}
  // Keep precedence primary, tightness secondary, and all unary operators
  // tighter than the strongest binary/paired ternary operator.
  static constexpr int binding_rank(unsigned precedence,unsigned tightness=0){
   return static_cast<int>(precedence*10+tightness);
  }
  static constexpr int unary_rank=100;
  static bool custom_lexeme(std::string_view spelling){
   return !spelling.empty()&&on1x_operator_width(spelling.data(),spelling.size())==spelling.size();
  }
  static std::string operator_literal(const Token& token){
   const auto text=std::string_view(token.text);
   if(text.size()>=2&&text.front()=='"'&&text.back()=='"'){
    const auto spelling=text.substr(1,text.size()-2);
     if(on1x_operator_spelling(spelling.data(),spelling.size()))return std::string(spelling);
   }
    throw Error(token.where,"expected an unescaped quoted custom operator spelling of 2 to 6 characters","ON1X-OPERATOR-SPELLING");
  }
  static std::string operator_word(const Token& token){
   const auto text=std::string_view(token.text);
   if(text.size()>=2&&text.front()=='"'&&text.back()=='"')return std::string(text.substr(1,text.size()-2));
   throw Error(token.where,"expected a quoted operator attribute value","ON1X-OPERATOR-DECLARATION");
  }
  static unsigned operator_rank_value(const Token& token,bool quoted,const char* kind){
   auto text=std::string_view(token.text);
   if(quoted&&text.size()>=2&&text.front()=='"'&&text.back()=='"')text=text.substr(1,text.size()-2);
   unsigned result=0;
   const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),result);
   if(error!=std::errc{}||end!=text.data()+text.size()||result>9)
    throw Error(token.where,std::string(kind)+" must be a decimal integer in 0..9","ON1X-OPERATOR-FIXITY");
   return result;
  }
   bool fixity_header(bool module=false)const{
   if(!at("infix")&&!at("infixr"))return false;
   if(module)return true;
    const auto& next=peek(1).text;
    if(templating&&next=="$")return true;
   if(next=="-")return custom_lexeme(peek(3).text);
   return !next.empty()&&(std::isdigit(static_cast<unsigned char>(next[0]))||next[0]=='"');
  }
   void fixity_directive(){
    auto start=peek();++pos;auto rank=peek();
    if(templating&&rank.text=="$")
     throw Error(rank.where,"macro fixity directives require a literal tightness","ON1X-OPERATOR-FIXITY");
    const auto tightness=operator_rank_value(rank,false,"tightness");++pos;auto token=peek();
     if(templating&&token.text=="$")
      throw Error(token.where,"macro fixity directives require a literal operator spelling","ON1X-OPERATOR-FIXITY");
    if(!on1x_operator_spelling(token.text.data(),token.text.size()))
    throw Error(token.where,"expected a custom operator in fixity directive","ON1X-OPERATOR-FIXITY");
   ++pos;consume(";");
   FixityDirective result{start.where,token.text,tightness,
     start.text=="infixr"?OperatorAssociativity::Right:OperatorAssociativity::Left};
   const auto binding=operator_binding(result.lexeme,OperatorPosition::Infix);
    if(!binding)
     throw Error(token.where,"fixity directive requires a visible infix operator: "+token.text,"ON1X-OPERATOR-FIXITY");
    if(directives&&directives->size()>=2048)
     throw Error(start.where,"operator fixity limit exceeded","ON1X-OPERATOR-LIMIT");
    fixity[binding->identity].push_back(result);fixity_undo.push_back(binding->identity);
    if(directives)directives->push_back(std::move(result));
  }
  const FixityDirective* operator_fixity(std::string_view identity)const{
   const auto it=fixity.find(identity);
   if(it!=fixity.end())return &it->second.back();
   return enclosing_parser?enclosing_parser->operator_fixity(identity):nullptr;
  }
  const OperatorBinding* local_operator_binding(std::string_view text,bool prefix)const{
   const auto it=local_operators.find(text);
   if(it==local_operators.end())return nullptr;
   const auto& slot=prefix?it->second.prefix:it->second.trailing;
   return slot.empty()?nullptr:&slot.back().binding;
  }
  const OperatorBinding* operator_binding(std::string_view text,OperatorPosition position)const{
   if(position!=OperatorPosition::Prefix){
    auto binding=trailing_operator(text);
    return binding&&binding->header.position==position?binding:nullptr;
   }
   if(auto binding=local_operator_binding(text,true))return binding;
   if(enclosing_parser)return enclosing_parser->operator_binding(text,position);
   return operators?operators->find(text,OperatorPosition::Prefix):nullptr;
  }
  const OperatorBinding* trailing_operator(std::string_view text)const{
   if(auto binding=local_operator_binding(text,false))return binding;
   if(enclosing_parser)return enclosing_parser->trailing_operator(text);
   return operators?operators->trailing(text):nullptr;
  }
  Expression operator_reference(std::string spelling,const Location& where){
   const auto prefix=operator_binding(spelling,OperatorPosition::Prefix);
   const auto trailing=trailing_operator(spelling);
   if(prefix&&trailing)
    throw Error(where,"ambiguous operator reference '"+spelling+"'","ON1X-OPERATOR-AMBIGUOUS");
   const auto binding=prefix?prefix:trailing;
   if(!binding)throw Error(where,"unknown operator: "+spelling,"ON1X-OPERATOR-UNBOUND");
   auto e=literal(Expr::Kind::OperatorReference,where,std::move(spelling));
   e->operator_lexeme2=binding->header.lexeme2;
   e->operator_identity=binding->identity;e->operator_position=binding->header.position;
   return {std::move(e)};
  }
   static Expression operator_node(const OperatorBinding& binding,Expr::Kind kind,Location where,Expression first,
                                  Expression second={nullptr,1},Expression third={nullptr,1}){
   auto e=literal(kind,where,binding.header.lexeme);
   e->operator_identity=binding.identity;e->operator_lexeme2=binding.header.lexeme2;
   e->operator_position=binding.header.position;
   auto depth=first.depth;
   e->children.push_back(std::move(first.node));
   if(second.node){depth=std::max(depth,second.depth);e->children.push_back(std::move(second.node));}
   if(third.node){depth=std::max(depth,third.depth);e->children.push_back(std::move(third.node));}
    return bounded(std::move(e),depth+1);
   }
   using OperatorForm=OperatorParse::Form;
   static void stamp(Expr& e,OperatorForm form,unsigned rank=0,
                     OperatorAssociativity binds=OperatorAssociativity::Left,const FixityDirective* directive=nullptr){
    auto edge=[](const Expr& child){return child.operator_grouped?nullptr:child.operator_parse;};
    auto parse=std::make_shared<OperatorParse>();
    parse->form=form;parse->rank=rank;parse->binds=binds;
    if(directive)parse->directive=directive->where;
    if(form!=OperatorForm::Prefix)parse->left=edge(e.callee?*e.callee:*e.children.front());
    if(form==OperatorForm::Prefix||form==OperatorForm::Infix||form==OperatorForm::Ternary)
     parse->right=edge(*e.children.back());
    e.operator_parse=std::move(parse);
   }
   static bool repeated(std::string_view text,char c){
   return text.size()>1&&text.find_first_not_of(c)==std::string_view::npos;
  }
  bool regex_header()const{
    if(!at("match")&&!at("capture"))return false;
    if(precedence(peek(1).text)>=0&&peek(1).text!="-"&&peek(1).text!="/")return false;
    if(trailing_operator(peek(1).text)&&!operator_binding(peek(1).text,OperatorPosition::Prefix))return false;
   std::size_t parens=0,brackets=0,braces=0;
   for(std::size_t i=1;pos+i<ts.size();++i){
    const auto& text=peek(i).text;
    if(!parens&&!brackets&&!braces){
     if(text=="~")return i>1&&peek(i+1).text!="{";
     if((text=="with"&&i>1&&peek(i-1).text!=".")||text=="="||text==":="||text==";"||text==","||
        text==")"||text=="]"||text=="}"||text=="=>"||text=="<eof>")return false;
     if(text=="{"&&peek(1).text=="("&&i>1&&peek(i-1).text==")")return false;
    }
    if(text=="(")++parens;else if(text==")"){if(!parens)return false;--parens;}
    if(text=="[")++brackets;else if(text=="]"){if(!brackets)return false;--brackets;}
    if(text=="{")++braces;else if(text=="}"){if(!braces)return false;--braces;}
   }
   return false;
  }
  Expression regex_application(){
   auto keyword=peek();++pos;
   auto e=std::make_unique<Expr>();e->where=keyword.where;
   e->kind=keyword.text=="match"?Expr::Kind::RegexMatch:Expr::Kind::RegexCapture;
   e->children.push_back(expr().node);consume("~");
   // One postfix operand. Parenthesize binary scrutinees, and parenthesize
   // the whole capture expression when indexing its result.
    e->children.push_back(expr(unary_rank).node);
   return syntax_bounded(std::move(e));
  }
   bool legacy_match_arm_header(std::size_t i)const{
    // A block after a call to a binding named `match` is not necessarily an
    // arm list. Only the legacy `Tag(payload) =>` prefix distinguishes one.
    while(templating&&peek(i).text=="$")++i;
    if(!is_ident(peek(i).text)||peek(i+1).text!="(")return false;
    std::size_t parens=1;
    for(i+=2;pos+i<ts.size();++i){
     const auto& text=peek(i).text;
     if(text=="(")++parens;
     else if(text==")"&&!--parens)return peek(i+1).text=="=>";
     else if(text==";"||text=="<eof>")return false;
    }
    return false;
   }
   bool match_header()const{
   if(!at("match"))return false;
   if(peek(1).text=="let")return true;
    // `<-` is a receive prefix at the start of a discriminant, but a send
     // operator after a value. Do not classify the former as an infix header.
     if(precedence(peek(1).text)>=0&&peek(1).text!="-"&&peek(1).text!="<-")return false;
     if(trailing_operator(peek(1).text)&&!operator_binding(peek(1).text,OperatorPosition::Prefix))return false;
   // Existing keyword-spelled values remain ordinary operands/callees unless
   // a complete discriminant is followed by the match delimiter.
    std::size_t parens=0,brackets=0,braces=0,initial_call_end=0;
   for(std::size_t i=1;pos+i<ts.size();++i){const auto& text=peek(i).text;
    if(!parens&&!brackets&&!braces){
     if(text=="with"&&i>1&&precedence(peek(i-1).text)<0&&peek(i-1).text!="!"&&peek(i-1).text!=".")return true;
     if(text=="{"&&peek(1).text=="("&&i>1&&peek(i-1).text==")"){
      if(legacy_match_arm_header(i+1))return true;
      // Stop at the block boundary rather than finding a later `with`.
      if(i==initial_call_end+1)return false;
     }
     if(text==";"||text=="="||text==","||text==")"||text=="]"||text=="}"||text=="=>"||text=="<eof>")return false;
    }
     if(text=="(")++parens;else if(text==")"){
      if(!parens)return false;
      if(!--parens&&!initial_call_end&&peek(1).text=="(")initial_call_end=i;
     }
    if(text=="[")++brackets;else if(text=="]"){if(!brackets)return false;--brackets;}
    if(text=="{")++braces;else if(text=="}"){if(!braces)return false;--braces;}
   }
   return false;
  }
   static std::string source_string(const std::string& value){
    std::string out="\"";out.reserve(value.size()+2);
    for(unsigned char c:value){
     if(c=='"'||c=='\\'||c=='$'){out+='\\';out+=static_cast<char>(c);}
     else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else if(c=='\0')out+="\\0";
     else out+=static_cast<char>(c);
    }
    return out+'"';
   }
   static bool string_token(std::string_view s){
    if(s.starts_with("r!")||s.starts_with("b!"))s.remove_prefix(2);
    return s.starts_with('"')||s.starts_with("'''");
   }
   static ExprPtr literal(Expr::Kind kind,const Location& at,std::string text={}){
    auto e=std::make_unique<Expr>();e->kind=kind;e->where=at;e->text=std::move(text);return e;
   }
   Expression string_value(const Token& token){
    const std::string_view s=token.text;
    const bool raw=s.starts_with("r!"),bytes=s.starts_with("b!");
    const auto prefix=raw||bytes?2u:0u;
    const bool strip_indent=s[prefix]=='\'';
    const auto delim=strip_indent||s.substr(prefix).starts_with("\"\"\"")?3u:1u;
    auto i=prefix+delim;
    const auto end=s.size()-delim;
    auto e=literal(bytes?Expr::Kind::ByteBuffer:Expr::Kind::String,token.where);
    std::string chunk;
    auto chunk_at=token.where;
    auto at_index=token.where;
    std::size_t advanced=0,depth=1;
    auto location=[&](std::size_t index){
     while(advanced<index){
      if(s[advanced++]=='\n'){++at_index.line;at_index.column=1;}else ++at_index.column;
      ++at_index.offset;
     }
     return at_index;
    };
    auto flush=[&]{
     if(!chunk.empty()){
      e->children.push_back(literal(Expr::Kind::String,chunk_at,source_string(chunk)));
      chunk.clear();
     }
    };
    auto rebase=[](Location& at,const Location& origin){
     at.offset+=origin.offset;
     if(at.line==1)at.column+=origin.column-1;
     at.line+=origin.line-1;
    };
    bool line_start=true;
    while(i<end){
     auto c=s[i++];
     // Strip only source indentation, not whitespace decoded from a splice or escape.
     if(strip_indent&&line_start&&(c==' '||c=='\t'))continue;
     line_start=c=='\n';
     if(!raw&&c=='\\'){
      c=s[i++];
      if(bytes&&c=='x'){
       c=static_cast<char>(on1x_lit_hex(static_cast<unsigned char>(s[i]))*16+
                           on1x_lit_hex(static_cast<unsigned char>(s[i+1])));
       i+=2;
      }else c=literals::escape(c);
     }else if(interpolate&&!raw&&!bytes&&c=='$'&&i<end){
      if(s[i]=='$')++i;
      else if(s[i]=='{'||on1x_lit_ident_start(static_cast<unsigned char>(s[i]))){
       auto splice_at=location(i-1);flush();
       if(e->children.size()>=literals::max_format_parts-1)
        throw Error(splice_at,"too many string interpolation parts","ON1X-LITERAL-LIMIT");
       e->kind=Expr::Kind::Interpolated;
       if(s[i]=='{'){
        ++i;auto begin=location(i);auto n=on1x_lit_interpolation(s.data()+i,end-i,0);
        if(n==end-i)throw Error(splice_at,"unterminated string interpolation","ON1X-LITERAL-STRING");
        std::vector<Token> embedded;
        std::vector<Docstring> embedded_docs;
        try{embedded=scan(std::string(s.substr(i,n)),token.where.file,docstrings?&embedded_docs:nullptr);}
        catch(Error& error){rebase(error.where,begin);throw;}
        for(auto& t:embedded)rebase(t.where,begin);
        for(auto& doc:embedded_docs){
         rebase(doc.where,begin);docstrings->push_back(std::move(doc));
        }
         Parser sub(embedded,{});
          sub.enclosing_parser=this;sub.declarations=declarations;sub.directives=directives;
          sub.operator_count=operator_count;sub.structural_operator_syntax=structural_operator_syntax;
        sub.docstrings=docstrings;
        sub.enabled_extensions=enabled_extensions;
        sub.templating=templating;sub.syntax_atoms=syntax_atoms;
        sub.literal_ellipsis=literal_ellipsis;sub.ellipsis=ellipsis;
        sub.bound_types=bound_types;sub.nesting=nesting;sub.lambdas=lambdas;
        auto value=sub.expr();sub.consume("<eof>");
        lambdas=sub.lambdas;
        used_extensions.insert(sub.used_extensions.begin(),sub.used_extensions.end());
        depth=std::max(depth,value.depth+1);
        e->children.push_back(std::move(value.node));
        i+=n+1;
       }else{
        auto begin=i++;
        while(i<end&&on1x_lit_ident(static_cast<unsigned char>(s[i])))++i;
        e->children.push_back(literal(Expr::Kind::Variable,location(begin),std::string(s.substr(begin,i-begin))));
        depth=std::max(depth,std::size_t{2});
       }
       chunk_at=location(i);
       continue;
      }
     }
     if(bytes){
      constexpr const char* hex="0123456789abcdef";
      const auto value=static_cast<unsigned char>(c);
      e->text+=hex[value>>4];e->text+=hex[value&15];
     }else chunk+=c;
    }
    if(e->kind==Expr::Kind::Interpolated)flush();
    else if(!bytes)e->text=source_string(chunk);
    return bounded(std::move(e),depth);
   }
   static std::vector<std::string> shell_words(const Token& token){
    const auto& raw=token.text;bool triple=raw.starts_with("```");
    std::size_t begin=triple?3:1,end=raw.size()-(triple?3:1),total=0;
    std::vector<std::string> words;std::string word;char quote=0;bool active=false;
    auto finish=[&]{
     if(!active)return;
     if(word.size()>4096)throw Error(token.where,"ON1X-CSP-LIMIT: shell word exceeds 4096 bytes");
     if(words.size()>=9)throw Error(token.where,"ON1X-CSP-LIMIT: shell argv limit exceeded");
     total+=word.size()+1;if(total>16384)throw Error(token.where,"ON1X-CSP-LIMIT: shell command exceeds 16384 bytes");
     words.push_back(std::move(word));word.clear();active=false;
    };
    for(std::size_t i=begin;i<end;++i){unsigned char c=static_cast<unsigned char>(raw[i]);
     if(quote){
       if(c==static_cast<unsigned char>(quote)){quote=0;active=true;continue;}
       if(quote=='"'&&c=='\\'){
        if(++i>=end||raw[i]=='\n'||raw[i]=='\r')throw Error(token.where,"shell escape cannot quote a newline","ON1X-CSP-SHELL");
        word+=raw[i];active=true;continue;
      }
      if(quote=='"'&&c=='$')throw Error(token.where,"ON1X-CSP-UNSUPPORTED: shell environment expansion is not admitted");
      word+=static_cast<char>(c);active=true;continue;
     }
     if(std::isspace(c)){finish();continue;}
      if(c=='\''||c=='"'){quote=static_cast<char>(c);active=true;continue;}
      if(c=='\\'){
       if(++i>=end||raw[i]=='\n'||raw[i]=='\r')throw Error(token.where,"shell escape cannot quote a newline","ON1X-CSP-SHELL");
       word+=raw[i];active=true;continue;
     }
     if(c=='$')throw Error(token.where,"ON1X-CSP-UNSUPPORTED: shell environment expansion is not admitted");
     if(c==';'||c=='|'||c=='&'||c=='<'||c=='>')throw Error(token.where,"ON1X-CSP-UNSUPPORTED: shell operators, pipelines and redirections are not admitted");
     word+=static_cast<char>(c);active=true;
    }
     if(quote)throw Error(token.where,"unterminated quote in shell literal","ON1X-CSP-SHELL");
     finish();
     if(words.empty())throw Error(token.where,"shell literal requires a command","ON1X-CSP-SHELL");
     if(words[0]=="export"||(words[0].find('=')!=std::string::npos&&words[0].find('/')==std::string::npos))
      throw Error(token.where,"ON1X-CSP-UNSUPPORTED: shell assignment/export is not admitted");
     if(words[0].empty()||words[0][0]!='/')throw Error(token.where,"ON1X-CSP-UNSUPPORTED: shell executable path must be absolute");
     return words;
   }
   Expression shell_launch(bool captured){
    auto where=peek().where;consume("launch");if(captured)consume("capture");
    auto token=peek();if(token.text.empty()||token.text[0]!='`')throw Error(token.where,"expected backtick shell literal");++pos;
    auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SourceCsp;e->where=where;e->text=captured?"launch_capture":"launch_exec";
    for(auto& word:shell_words(token)){auto arg=std::make_unique<Expr>();arg->kind=Expr::Kind::String;arg->where=token.where;arg->text=source_string(word);e->children.push_back(std::move(arg));}
    return syntax_bounded(std::move(e));
   }
         Expression atom(){
           const Token t=peek();
           if(at("borrow")){
           ++pos;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Borrow;
           e->where=t.where;e->borrow=accept("mut")?BorrowMode::Exclusive:BorrowMode::Shared;
           auto source=std::make_unique<Expr>();source->kind=Expr::Kind::Variable;
           source->where=peek().where;source->text=ident();
           e->children.push_back(std::move(source));
           return bounded(std::move(e),2);
          }
         if(at("unsafe")&&peek(1).text=="{"){
          ++pos;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::UnsafeBlock;e->where=t.where;
          value_block(*e);
          auto depth=std::max(body_expression_depth(e->body),e->children[0]->syntax_depth)+1;
          return bounded(std::move(e),depth);
         }
         if(at("launch")&&peek(1).text=="capture"&&!peek(2).text.empty()&&peek(2).text[0]=='`')return shell_launch(true);
         if(at("launch")&&!peek(1).text.empty()&&peek(1).text[0]=='`')return shell_launch(false);
        if(at("await")&&peek(1).text=="timer"&&peek(2).text=="("){
         pos+=3;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SourceCsp;
         e->where=t.where;e->text="await_timer";e->children.push_back(expr().node);consume(")");
         return syntax_bounded(std::move(e));
        }
        const bool stackless=at("launch")&&peek(1).text=="async"&&peek(2).text=="{";
        if(stackless||(at("launch")&&peek(1).text=="{")){
         pos+=stackless?2:1;Expr arm;arm.where=t.where;value_block(arm);
        auto thunk=std::make_unique<Expr>();thunk->kind=Expr::Kind::Lambda;thunk->where=t.where;thunk->result=Type::I64;thunk->body=std::move(arm.body);
        if(arm.children[0]->kind!=Expr::Kind::Unit){Stmt ret;ret.kind=Stmt::Kind::Return;ret.where=arm.children[0]->where;ret.value=std::move(arm.children[0]);thunk->body.push_back(std::move(ret));}
         auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SourceCsp;e->where=t.where;e->text=stackless?"launch_async":"launch";thunk->syntax_depth=body_expression_depth(thunk->body)+1;e->children.push_back(std::move(thunk));return syntax_bounded(std::move(e));
         }
          if(at("launch")&&peek(1).text=="exec"&&peek(2).text=="("){
          pos+=3;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SourceCsp;e->where=t.where;e->text="launch_exec";
          if(!at(")"))do{
           if(e->children.size()>=9)throw Error(t.where,"ON1X-CSP-LIMIT: external argv limit exceeded");
           // `exec` is a constant argv profile, not a text evaluation
           // context. In particular, neither shell nor On1x expands '$'.
           const bool saved_interpolation=interpolate;
           interpolate=false;
           auto argument=expr().node;
           interpolate=saved_interpolation;
           if(argument->kind!=Expr::Kind::String)throw Error(argument->where,"ON1X-CSP-UNSUPPORTED: external argv requires constant string literals");
           e->children.push_back(std::move(argument));
          }while(accept(","));
          consume(")");if(e->children.empty())throw Error(t.where,"external launch requires an absolute executable path");
           return syntax_bounded(std::move(e));
          }
         if((at("try_recv")||at("try_send"))&&peek(1).text=="("){
          auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SourceCsp;
          e->where=t.where;e->text=t.text;++pos;consume("(");
          e->children.push_back(expr().node);
          if(e->text=="try_send"){consume(",");e->children.push_back(expr().node);}
          consume(")");return syntax_bounded(std::move(e));
         }
        if(at("poll")&&peek(1).text=="{")return polling(false);
       if(at("->")&&peek(1).text=="("){
        ++pos;consume("(");auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SourceCsp;e->text="channel";e->where=t.where;
        e->children.push_back(expr().node);consume(",");e->children.push_back(expr().node);consume(")");consume("{");
        std::set<std::string> fields;bool payload=false,mode=false;
        do{auto name=ident();if(!fields.insert(name).second)throw Error(t.where,"duplicate channel field: "+name);consume(":");
         if(name=="payload"){e->result=type();payload=true;}
         else if(name=="capacity")e->children.push_back(expr().node);
         else if(name=="mode"){auto value=ident();if(value!="uni")throw Error(t.where,"ON1X-CSP-UNSUPPORTED: duplex channel profile unavailable");mode=true;}
         else throw Error(t.where,"ON1X-CSP-UNSUPPORTED: unknown channel field: "+name);
        }while(accept(",")&&!at("}"));consume("}");
        if(!payload||!mode||e->children.size()!=3)throw Error(t.where,"channel requires payload, capacity and mode");
        return syntax_bounded(std::move(e));
       }
      if((at("loop")&&peek(1).text=="{")||(is_ident(peek().text)&&peek(1).text==":"&&peek(2).text=="loop"&&peek(3).text=="{")){
       auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Loop;e->where=t.where;
       if(!at("loop")||peek(1).text!= "{"){e->text=ident();consume(":");}
       consume("loop");e->body=block();auto depth=body_expression_depth(e->body)+1;return bounded(std::move(e),depth);
      }
     if(at("record")&&(peek(1).text=="{"||typed_data_header("{"))){
      ++pos;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Record;e->where=t.where;
       if(accept("<")){e->result=type();close_angle();if(e->result.kind()!=Type::Named)throw Error(t.where,"named record construction requires a named type");}
      consume("{");std::set<std::string> names;
     if(!at("}"))do{
      auto label=ident();if(!names.insert(label).second)throw Error(t.where,"duplicate record field: "+label);
      if(e->labels.size()>=256){throw Error(t.where,"data field limit exceeded");}e->labels.push_back(label);consume(":");e->children.push_back(expr().node);
     }while(accept(","));
     consume("}");return syntax_bounded(std::move(e));
    }
    if(at("array")&&peek(1).text=="["){
     ++pos;consume("[");auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Array;e->where=t.where;
     if(!at("]"))do{if(e->children.size()>=256)throw Error(t.where,"array extent limit exceeded");e->children.push_back(expr().node);}while(accept(","));
     consume("]");return syntax_bounded(std::move(e));
    }
     if(at("variant")&&peek(1).text=="<"&&((peek(2).text=="sum"&&peek(3).text=="{")||typed_data_header("("))){
      ++pos;consume("<");auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Variant;e->where=t.where;e->result=type();close_angle();consume("(");e->text=ident();consume(",");e->children.push_back(expr().node);consume(")");return syntax_bounded(std::move(e));
    }
    if(regex_header())return regex_application();
    if(match_header())return matching(false);
    if(at("let_syntax")||at("letrec_syntax"))return syntax_scope();
    if(accept("quote"))return quotation(t.where);
    if(accept("splice")||accept("lift")){
     auto e=std::make_unique<Expr>();e->kind=t.text=="splice"?Expr::Kind::Splice:Expr::Kind::Lift;e->where=t.where;consume("(");e->children.push_back(expr().node);consume(")");return syntax_bounded(std::move(e));
    }
    if((templating||syntax_atoms)&&at("syntax_literal")){
     ++pos;consume("(");auto token=peek();
     if(!is_ident(token.text)&&token.text!="...")throw Error(token.where,"syntax_literal requires one identifier/keyword token");
     ++pos;consume(")");return syntax_token(token);
    }
    if((templating||syntax_atoms)&&at("syntax_block")&&peek(1).text=="{"){
     ++pos;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SyntaxBlock;e->where=t.where;e->body=block();
     auto depth=body_expression_depth(e->body)+1;return bounded(std::move(e),depth);
    }
    if(templating&&at("escape")){
     ++pos;consume("(");bool saved=literal_ellipsis;literal_ellipsis=true;auto value=expr();literal_ellipsis=saved;consume(")");return value;
    }
    if((templating||syntax_atoms)&&(at("[")||(at("vector")&&peek(1).text=="["))){
     bool vector=accept("vector");consume("[");auto e=std::make_unique<Expr>();e->kind=vector?Expr::Kind::SyntaxVector:Expr::Kind::SyntaxList;e->where=t.where;
     syntax_sequence(*e,"]");if(vector&&e->syntax_tail)throw Error(t.where,"vector syntax cannot have a dotted tail");return syntax_bounded(std::move(e));
    }
    if((syntax_atoms||literal_ellipsis)&&
       (at("...")||at("else")||at("if")||at("let")||at("var")||at("return")||at("while")||at("extern")||at("import")||at("macro")||at("rule")||at("forall")||((at("fn")||at("proc"))&&peek(1).text!="("))){
     ++pos;return syntax_token(t);
    }
    if(templating&&accept("$")){
     auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SyntaxHole;e->where=t.where;e->text="$"+ident();return {std::move(e)};
    }
   if(at("operator")&&peek(1).text=="("){
    if(templating&&peek(2).text=="$"){
     ++pos;
     return {literal(Expr::Kind::Variable,t.where,"ast_operator")};
    }
    if(string_token(peek(2).text)){
     pos+=2;auto name=peek();++pos;consume(")");
     return operator_reference(operator_literal(name),name.where);
    }
   }
   if(accept("@"))return extension(t.where);
   if(accept("fn")||accept("proc")){
    if(++lambdas>2048)throw Error(t.where,"too many anonymous functions");
    auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Lambda;e->where=t.where;
    e->procedure=t.text=="proc";
    consume("(");
    if(!at(")"))do{
      auto p=parameter(e->parameters.size());
     e->parameters.push_back(std::move(p));
    }while(accept(","));
    consume(")");if(accept("->"))e->result=type();e->body=block();
    auto depth=body_expression_depth(e->body)+1;return bounded(std::move(e),depth);
   }
   if(accept("(")){
    auto first=expr();
     if(!accept(",")){consume(")");first.node->operator_grouped=true;return first;}
    auto tuple=std::make_unique<Expr>();tuple->kind=Expr::Kind::Tuple;tuple->where=t.where;
    auto depth=first.depth+1;tuple->children.push_back(std::move(first.node));
    do{auto element=expr();depth=std::max(depth,element.depth+1);tuple->children.push_back(std::move(element.node));}while(accept(","));
    consume(")");return bounded(std::move(tuple),depth);
   }
  if(t.text.size()>=2&&t.text.front()=='/'&&t.text.back()=='/'){
   ++pos;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::RegexLiteral;e->where=t.where;e->text=t.text;return {std::move(e)};
  }
  if(at("/"))throw Error(t.where,"expected a closed /.../ regex literal","ON1X-PARSE-REGEX");
  if(accept("true")||accept("false")){auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Boolean;e->where=t.where;e->text=t.text;return {std::move(e)};}
  if(string_token(t.text)){++pos;return string_value(t);}
  if(t.text.starts_with("#\\")||t.text.starts_with("b!#\\")){
   ++pos;
   if(t.text.starts_with("b!")){
    auto value=literals::character(std::string_view(t.text).substr(2),t.where);
    if(value>255)throw Error(t.where,"byte character literal is outside 0..255","ON1X-LITERAL-BYTE");
    return {literal(Expr::Kind::Integer,t.where,std::to_string(value)+"u8")};
   }
   (void)literals::character(t.text,t.where);
   return {literal(Expr::Kind::Character,t.where,t.text)};
  }
  if(!t.text.empty()&&std::isdigit((unsigned char)t.text[0])){
   ++pos;auto e=std::make_unique<Expr>();
   bool hexadecimal=t.text.starts_with("0x")||t.text.starts_with("0X");
   e->kind=(!hexadecimal&&numeric_literal_type(t.text).is_float())?Expr::Kind::Float:Expr::Kind::Integer;
   e->where=t.where;e->text=literals::radix_integer(t.text,t.where);return {std::move(e)};
  }
    if(cliche_member_header()){
     auto e=std::make_unique<Expr>();e->kind=Expr::Kind::ClicheMember;e->where=t.where;
     e->text=qualified_identifier();consume("<");
     do{
      e->cliche_arguments.push_back(type());
      if(e->cliche_arguments.size()>32)throw Error(t.where,"cliche use argument limit exceeded","ON1X-CLICHE-LIMIT");
     }while(accept(","));
     close_angle();consume("::");e->cliche_member=ident();
     if(accept("<")){
       if(!at_close_angle())do{
       e->member_type_arguments.push_back(type());
       if(e->member_type_arguments.size()>32)throw Error(t.where,"cliche member type-argument limit exceeded","ON1X-CLICHE-LIMIT");
      }while(accept(","));
      close_angle();
     }
     return {std::move(e)};
    }
     std::string name=ident();
     // Qualified macro calls have a syntax name, not a value receiver. Other
     // dotted paths are postfix projections until the loader resolves them.
     std::size_t path=0;
     while(peek(path).text=="."&&is_ident(peek(path+1).text)&&
           peek(path).where.offset==ts[pos+path-1].where.offset+ts[pos+path-1].text.size()&&
           peek(path+1).where.offset==peek(path).where.offset+1)path+=2;
     if(peek(path).text=="!")while(path){
      ++pos;name+="."+ident();path-=2;
      if(name.size()>4096)throw Error(t.where,"qualified name exceeds 4096 bytes");
     }
   auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Variable;e->where=t.where;e->text=name;
     if(accept("!")){
      e->kind=Expr::Kind::MacroCall;consume("(");
      OperatorSyntax syntax(*this);
      syntax_sequence(*e,")");return syntax_bounded(std::move(e));
   }
  return {std::move(e)};
 }
   Expression prefix(bool capture_colon,std::string_view stop){
    const Token t=peek();
    if(!stop.empty()&&t.text==stop)
     throw Error(t.where,"expected an operand before ternary delimiter '"+t.text+"'","ON1X-OPERATOR-TERNARY");
     if(auto binding=operator_binding(t.text,OperatorPosition::Prefix)){
      ++pos;
      const auto rank=unary_rank+binding_rank(binding->header.precedence,binding->header.tightness);
      auto operand=expr(rank,capture_colon,stop);
      auto result=operator_node(*binding,Expr::Kind::OperatorUnary,t.where,std::move(operand));
      stamp(*result.node,OperatorForm::Prefix,rank);
      return result;
    }
     // Prefix operations in this profile take a named endpoint/handle. In
    // particular, `if close { ... }` and `split!(...)` remain ordinary value
    // and macro syntax, not an attempt to consume the following block.
     if(at("<-")||((at("join")||at("split")||at("close")||at("cancel"))&&is_ident(peek(1).text)&&peek(1).text!="with")){
    ++pos;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SourceCsp;e->where=t.where;e->text=t.text=="<-"?"recv":t.text;
      auto operand=expr(unary_rank,capture_colon,stop);e->children.push_back(std::move(operand.node));
      stamp(*e,OperatorForm::Prefix,unary_rank);return bounded(std::move(e),operand.depth+1);
    }
   if(t.text=="!"||t.text=="-"||repeated(t.text,'!')){
    split_after_first();
   if(t.text=="-"&&literals::radix(peek().text)&&!numeric_literal_type(peek().text).is_unsigned()&&peek(1).text!="("){
    // Parse the magnitude with the negative i64 boundary before the ordinary
    // positive literal check. Other minus expressions keep postfix precedence.
    const auto& n=peek();
    auto value=literals::radix_integer(n.text,n.where,true);
     if(value=="-9223372036854775808"&&peek(1).text!="["&&!peek(1).text.starts_with('?')&&peek(1).text!="."){
     ++pos;return {literal(Expr::Kind::Integer,t.where,std::move(value))};
    }
   }
   if(t.text=="-" && (peek().text=="9223372036854775808"||peek().text=="0x8000000000000000"||peek().text=="0X8000000000000000") && peek(1).text!="(") {
    ++pos;auto literal=std::make_unique<Expr>();literal->kind=Expr::Kind::Integer;
    literal->where=t.where;literal->text="-9223372036854775808";return {std::move(literal)};
   }
    auto child=expr(unary_rank,capture_colon,stop);
    auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Unary;e->where=t.where;e->text=t.text=="-"?"-":"!";
    e->children.push_back(std::move(child.node));stamp(*e,OperatorForm::Prefix,unary_rank);
    return bounded(std::move(e),child.depth+1);
  }
    if(custom_lexeme(t.text))
     throw Error(t.where,"no visible prefix operator: "+t.text,"ON1X-OPERATOR-UNBOUND");
    return core_postfix(atom(),capture_colon,stop);
   }
   Expression core_postfix(Expression value,bool capture_colon,std::string_view stop){
      while(true){
      if(!stop.empty()&&at(stop))break;
     if(capture_colon&&value.node->kind==Expr::Kind::Variable&&at(":")){
      const auto& spelling=peek(1).text;
      if(!spelling.empty()&&std::all_of(spelling.begin(),spelling.end(),[](char c){return c>='0'&&c<='9';})){
       auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Index;e->where=consume(":").where;e->text=":";
       e->children.push_back(std::move(value.node));
       e->children.push_back(literal(Expr::Kind::Integer,peek().where,spelling));++pos;
        stamp(*e,OperatorForm::Suffix);value=syntax_bounded(std::move(e));continue;
      }
     }
      if(accept("::")){
       auto e=std::make_unique<Expr>();e->kind=Expr::Kind::InherentMember;
       e->where=ts[pos-1].where;e->text=ident();
        e->children.push_back(std::move(value.node));
        stamp(*e,OperatorForm::Suffix);value=bounded(std::move(e),value.depth+1);continue;
      }
      if(at(".")&&is_ident(peek(1).text)){
       const bool attached=peek().where.offset==ts[pos-1].where.offset+ts[pos-1].text.size()&&
                           peek(1).where.offset==peek().where.offset+1;
       // In macro syntax a separated dot still introduces a dotted-list tail.
       if((templating||syntax_atoms)&&!attached)break;
       auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Field;e->where=value.node->where;
       ++pos;e->text=ident();
       e->dot_path=attached&&!value.node->operator_grouped&&
                   (value.node->kind==Expr::Kind::Variable||value.node->dot_path);
       e->children.push_back(std::move(value.node));
       stamp(*e,OperatorForm::Suffix);value=bounded(std::move(e),value.depth+1);continue;
      }
       if(at("?")||(repeated(peek().text,'?')&&!trailing_operator(peek().text))){
       auto where=peek().where;split_after_first();
       auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Propagate;e->where=std::move(where);
       e->children.push_back(std::move(value.node));
       stamp(*e,OperatorForm::Suffix);value=bounded(std::move(e),value.depth+1);continue;
     }
    if(accept("[")){
     auto e=std::make_unique<Expr>();e->where=value.node->where;e->children.push_back(std::move(value.node));
      if(accept("]")){
       e->kind=Expr::Kind::Length;stamp(*e,OperatorForm::Suffix);value=syntax_bounded(std::move(e));continue;
      }
     if(!peek().text.empty()&&peek().text.front()=='"'&&peek(1).text=="]"){
      auto field=peek();++pos;consume("]");e->kind=Expr::Kind::Field;e->text=field.text.substr(1,field.text.size()-2);
      if(!is_ident(e->text))throw Error(field.where,"record projection requires an identifier label");
     }else{
      // At the top level of an index, ':' belongs to the existing slice
      // grammar. A parenthesized expression may still use capture shorthand.
       e->children.push_back(expr(0,false).node);
      if(accept(":")){e->kind=Expr::Kind::Slice;e->children.push_back(expr().node);}else e->kind=Expr::Kind::Index;
      consume("]");
     }
      stamp(*e,OperatorForm::Suffix);value=syntax_bounded(std::move(e));continue;
    }
    if(templating&&!literal_ellipsis&&accept(ellipsis)){if(++value.node->syntax_repeat>256)throw Error(value.node->where,"template ellipsis depth limit exceeded");continue;}
    if(!accept("("))break;
   auto call=std::make_unique<Expr>();call->kind=Expr::Kind::Call;call->where=value.node->where;
   call->callee=std::move(value.node);auto depth=value.depth+1;
   if(!at(")")&&!(templating&&at(".")))do{
      auto argument=expr();depth=std::max(depth,argument.depth+1);
      call->children.push_back(std::move(argument.node));
     }while(accept(",")&&!(templating&&at(".")));
    if(templating&&accept(".")){call->syntax_tail=expr().node;depth=std::max(depth,call->syntax_tail->syntax_depth+1);}
    consume(")");stamp(*call,OperatorForm::Suffix);value=bounded(std::move(call),depth);
  }
   return value;
  }
   ExprPtr syntax_argument(){
    if(custom_lexeme(peek().text)||(precedence(peek().text)>=0&&!at("<-"))||at("!")){
     const auto& next=peek(1).text;
     if(next==","||next==")"||next=="]"||next=="."){
      const auto token=peek();++pos;
      if(custom_lexeme(token.text))return operator_reference(token.text,token.where).node;
      return literal(token.text=="!"?Expr::Kind::Unary:Expr::Kind::Binary,token.where,token.text);
     }
    }
    if(!at("{")){bool saved=syntax_atoms;syntax_atoms=true;auto e=expr().node;syntax_atoms=saved;return e;}
   auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SyntaxBlock;e->where=peek().where;e->body=block();
   auto depth=body_expression_depth(e->body)+1;return bounded(std::move(e),depth).node;
  }
  Expression syntax_token(const Token& token){
   auto e=std::make_unique<Expr>();e->where=token.where;e->text=token.text;
   static const std::set<std::string> keywords={"fn","proc","let","var","if","else","while","return","extern","import","forall","macro","rule","quote","splice","lift","let_syntax","letrec_syntax","..."};
   e->kind=keywords.contains(token.text)?Expr::Kind::SyntaxKeyword:Expr::Kind::Variable;return {std::move(e)};
  }
  Expression syntax_bounded(ExprPtr e){
   std::size_t depth=1;for(const auto& c:e->children)depth=std::max(depth,c->syntax_depth+1);
   if(e->syntax_tail)depth=std::max(depth,e->syntax_tail->syntax_depth+1);
   return bounded(std::move(e),depth);
  }
  void syntax_sequence(Expr& e,const std::string& end){
   if(!at(end)&&!at(".")){
    do{e.children.push_back(syntax_argument());}while(accept(",")&&!at("."));
   }
   if(accept("."))e.syntax_tail=syntax_argument();
   consume(end);
  }
  Expression extension(const Location& where){
   auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Extension;e->where=where;e->text=ident();
   if(e->text.size()>128)throw Error(where,"PPX label exceeds 128 bytes");
   consume("(");
   if(!at(")"))do{e->children.push_back(syntax_argument());}while(accept(","));
   consume(")");
   if(e->children.empty()||e->children.front()->kind!=Expr::Kind::String)throw Error(where,"PPX extension requires a string name as its first argument");
   std::size_t depth=1;for(const auto& child:e->children)depth=std::max(depth,child->syntax_depth+1);
   return bounded(std::move(e),depth);
  }
  void macro_sequence(std::vector<MacroParameter>& out,std::unique_ptr<MacroParameter>& tail,const std::string& end,const Macro& macro){
   if(!at(end)&&!at("."))do{out.push_back(macro_pattern(macro));}while(accept(",")&&!at("."));
   if(accept(".")){tail=std::make_unique<MacroParameter>(macro_pattern(macro));if(tail->repeated)throw Error(tail->where,"a dotted tail cannot itself repeat");}
   consume(end);
  }
  MacroParameter macro_pattern(const Macro& macro){
   Nesting guard(*this);MacroParameter p;p.where=peek().where;
    if(accept("$")){
     auto name=ident();p.name="$"+name;
     if(accept(":"))p.category=ident();
     else if((name=="operator"||name=="customop"||name=="customoperator")&&
             (at(",")||at(")")||at("]")||at(".")||at(macro.ellipsis)))p.category=name;
     else consume(":");
    }
   else if(at("_")&&std::find(macro.literals.begin(),macro.literals.end(),"_")==macro.literals.end()){
    ++pos;p.kind=MacroParameter::Kind::Wildcard;if(accept(":"))p.category=ident();
   }else if(at("[")||at("(")||(at("vector")&&peek(1).text=="[")||
              ((at("ast_call")||at("ast_unary")||at("ast_binary")||at("ast_ternary")||at("ast_operator"))&&peek(1).text=="(")){
    auto form=peek().text;++pos;std::string end;
    if(form=="vector"){consume("[");end="]";p.kind=MacroParameter::Kind::Vector;}
    else if(form=="["){end="]";p.kind=MacroParameter::Kind::List;}
    else if(form=="("){end=")";p.kind=MacroParameter::Kind::Tuple;}
     else{
      consume("(");end=")";using Kind=MacroParameter::Kind;
      p.kind=form=="ast_call"?Kind::Call:form=="ast_unary"?Kind::Unary:
             form=="ast_binary"?Kind::Binary:form=="ast_ternary"?Kind::Ternary:Kind::OperatorReference;
     }
    macro_sequence(p.elements,p.tail,end,macro);
    if(p.kind==MacroParameter::Kind::Vector&&p.tail)throw Error(p.where,"vector patterns cannot have dotted tails");
   }else{
    bool saved=syntax_atoms;syntax_atoms=true;p.kind=MacroParameter::Kind::Literal;
    if(accept("-")){auto value=atom();if(value.node->kind!=Expr::Kind::Integer)throw Error(p.where,"negative pattern datum requires an integer");value.node->text="-"+value.node->text;p.literal=std::move(value.node);}
    else p.literal=atom().node;
    syntax_atoms=saved;
      if(p.literal->kind!=Expr::Kind::Variable&&p.literal->kind!=Expr::Kind::String&&p.literal->kind!=Expr::Kind::Integer&&p.literal->kind!=Expr::Kind::Float&&p.literal->kind!=Expr::Kind::Boolean&&p.literal->kind!=Expr::Kind::Character&&p.literal->kind!=Expr::Kind::SyntaxKeyword&&p.literal->kind!=Expr::Kind::OperatorReference)
     throw Error(p.where,"expected literal datum, wildcard, typed variable or structural pattern");
   }
   p.repeated=accept(macro.ellipsis);return p;
  }
  Macro macro_definition(){
   Macro m;m.where=ts[pos-1].where;m.name=ident();bool have_literals=false,have_ellipsis=false;
   while(at("literals")||at("ellipsis")){
    if(accept("ellipsis")){if(have_ellipsis)throw Error(peek().where,"duplicate ellipsis declaration");have_ellipsis=true;m.ellipsis=ident();}
    else{consume("literals");if(have_literals)throw Error(peek().where,"duplicate literal declaration");have_literals=true;consume("(");
     if(!at(")"))do{auto token=peek();if(!is_ident(token.text))throw Error(token.where,"literal declarations require identifiers/keywords");++pos;if(std::find(m.literals.begin(),m.literals.end(),token.text)!=m.literals.end())throw Error(token.where,"duplicate macro literal");m.literals.push_back(token.text);}while(accept(","));
     consume(")");}
   }
   if(std::find(m.literals.begin(),m.literals.end(),m.ellipsis)!=m.literals.end())throw Error(m.where,"ellipsis identifier cannot also be a literal");
   auto saved_ellipsis=ellipsis;ellipsis=m.ellipsis;consume("{");
   while(!accept("}")){
    MacroRule r;r.where=peek().where;consume("rule");consume("(");
    macro_sequence(r.parameters,r.tail,")",m);consume("=>");templating=true;
    if(accept("expression")){consume("{");r.expression=expr().node;consume("}");}
    else {consume("block");r.block=true;r.body=block();}
    templating=false;consume(";");m.rules.push_back(std::move(r));
    if(m.rules.size()>256)throw Error(m.where,"macro rule limit exceeded");
   }
   if(m.rules.empty())throw Error(m.where,"macro requires at least one rule");
   ellipsis=saved_ellipsis;return m;
  }
  Expression syntax_scope(){
   auto token=peek();++pos;
   if(templating)throw Error(token.where,"local syntax definitions cannot be generated by a macro template");
   auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SyntaxLet;e->where=token.where;e->text=token.text;consume("{");
    while(!accept("}")){
     consume("macro");e->syntax_definitions.push_back(macro_definition());
     if(e->syntax_definitions.size()>512)throw Error(token.where,"local macro definition limit exceeded");
    }
    OperatorSyntax syntax(*this);
    if(accept("expression")){consume("{");e->children.push_back(expr().node);consume("}");}
   else{consume("block");auto child=std::make_unique<Expr>();child->kind=Expr::Kind::SyntaxBlock;child->where=peek().where;child->body=block();child->syntax_depth=body_expression_depth(child->body)+1;e->children.push_back(std::move(child));}
   return syntax_bounded(std::move(e));
  }
   Expression expr(int minprec=0,bool capture_colon=true,std::string_view stop={}){
    Nesting guard(*this);auto lhs=prefix(capture_colon,stop);
    while(true){
     if(!stop.empty()&&at(stop))break;
     if(at("~")&&peek(1).text=="{"){
      if(minprec>binding_rank(3))break; // Equality/regex precedence.
      auto e=std::make_unique<Expr>();e->kind=Expr::Kind::PatternMatch;
      e->where=consume("~").where;e->sum_match=true;e->children.push_back(std::move(lhs.node));
       lhs=matching_arms(std::move(e));stamp(*lhs.node,OperatorForm::Postfix,binding_rank(3));continue;
     }
     auto op=peek().text;
     if(const auto binding=trailing_operator(op)){
      const auto& header=binding->header;
      auto rank=binding_rank(header.precedence,header.tightness);auto associativity=header.binds;
       const auto override=operator_fixity(binding->identity);
       if(override){
       rank=binding_rank(header.precedence,override->tightness);
       associativity=override->binds;
      }
      if(header.position==OperatorPosition::Postfix)rank+=unary_rank;
      if(rank<minprec)break;
      auto where=peek().where;++pos;
      if(header.position==OperatorPosition::Postfix){
        lhs=operator_node(*binding,Expr::Kind::OperatorUnary,std::move(where),std::move(lhs));
        stamp(*lhs.node,OperatorForm::Postfix,rank);
        lhs=core_postfix(std::move(lhs),capture_colon,stop);
       continue;
      }
      const auto rhs_rank=rank+(associativity==OperatorAssociativity::Left);
      if(header.arity==OperatorArity::Ternary){
       auto middle=expr(0,capture_colon,header.lexeme2);
       if(!accept(header.lexeme2))
        throw Error(peek().where,"expected ternary delimiter '"+header.lexeme2+"' for '"+op+"'","ON1X-OPERATOR-TERNARY",header.lexeme2,peek().text);
       auto rhs=expr(rhs_rank,capture_colon,stop);
        lhs=operator_node(*binding,Expr::Kind::OperatorTernary,std::move(where),std::move(lhs),std::move(middle),std::move(rhs));
        stamp(*lhs.node,OperatorForm::Ternary,rank,associativity,override);
      }else{
       auto rhs=expr(rhs_rank,capture_colon,stop);
        lhs=operator_node(*binding,Expr::Kind::OperatorBinary,std::move(where),std::move(lhs),std::move(rhs));
        stamp(*lhs.node,OperatorForm::Infix,rank,associativity,override);
      }
      continue;
     }
     const auto tier=precedence(op);
     if(tier<0){
      if(minprec==0&&stop.empty()&&custom_lexeme(op))
       throw Error(peek().where,"no visible trailing operator: "+op,"ON1X-OPERATOR-UNBOUND");
      break;
     }
     const auto prec=binding_rank(static_cast<unsigned>(tier));if(prec<minprec)break;
    auto where=peek().where;++pos;auto rhs=expr(prec+1,capture_colon,stop);
    auto e=std::make_unique<Expr>();e->kind=op=="<-"?Expr::Kind::SourceCsp:Expr::Kind::Binary;e->where=where;e->text=op=="<-"?"send":op;
   auto depth=std::max(lhs.depth,rhs.depth)+1;
    e->children.push_back(std::move(lhs.node));e->children.push_back(std::move(rhs.node));
    stamp(*e,OperatorForm::Infix,prec);lhs=bounded(std::move(e),depth);
  }return lhs;
 }
   std::vector<Stmt> block(){Nesting guard(*this);LexicalRegion scope(*this);consume("{");std::vector<Stmt> result;
   while(!at("}")){if(at("<eof>"))throw Error(peek().where,"unterminated block");
     if(block_operator_start()){parse_operator_declaration(&result);continue;}
    if(fixity_header()){fixity_directive();continue;}
   result.push_back(statement());if(result.size()>100000)throw Error(peek().where,"too many statements");}
   consume("}");return result;
  }
   Pattern pattern(){
   Nesting guard(*this);Pattern result;result.where=peek().where;
   if(accept("(")){result.kind=Pattern::Kind::Tuple;
    result.elements.push_back(pattern());consume(",");
    do{result.elements.push_back(pattern());}while(accept(","));
    consume(")");
   }else result.name=ident();
   return result;
  }
   Pattern match_pattern_atom(bool dash_wildcard){
    Nesting guard(*this);Pattern p;p.where=peek().where;using P=Pattern::Kind;
    if(accept("_")){p.kind=P::Wildcard;return p;}
    // The dash is a wildcard only at a complete pattern boundary. Keep the
    // ordinary signed-literal parsing and diagnostics for every other dash.
    if(dash_wildcard&&at("-")){
     const auto& next=peek(1).text;
     if(next==")"||next=="]"||next=="}"||next==","||next=="as"||next=="|"||next=="if"||next=="=>"){
      ++pos;p.kind=P::Wildcard;return p;
     }
    }
    if(accept("rest")){p.kind=P::Rest;return p;}
    if(accept("(")){
     p=match_pattern(dash_wildcard);if(!accept(",")){consume(")");return p;}
     Pattern tuple;tuple.kind=P::Tuple;tuple.where=p.where;tuple.elements.push_back(std::move(p));
     do{tuple.elements.push_back(match_pattern(dash_wildcard));if(tuple.elements.size()>256)throw Error(tuple.where,"pattern width limit exceeded");}while(accept(",")&&!at(")"));
    consume(")");return tuple;
   }
   if(accept("[")){
    p.kind=P::Array;
     if(!at("]")){do{p.elements.push_back(match_pattern(dash_wildcard));if(p.elements.size()>256)throw Error(p.where,"pattern width limit exceeded");
     if(at("::"))throw Error(peek().where,"ON1X-PATTERN: list-cons patterns require an owning list runtime","ON1X-PATTERN");
    }while(accept(",")&&!at("]"));}
    consume("]");return p;
   }
   if(at("record")&&peek(1).text=="{"){
    pos+=2;p.kind=P::Record;std::set<std::string> labels;
    if(!at("}")){do{if(accept("..")){p.open=true;break;}
     auto label=ident();if(!labels.insert(label).second)throw Error(p.where,"duplicate record pattern field: "+label);
      p.labels.push_back(label);consume(":");p.elements.push_back(match_pattern(dash_wildcard));
     if(p.elements.size()>256)throw Error(p.where,"pattern width limit exceeded");
    }while(accept(",")&&!at("}"));}
    consume("}");return p;
   }
   auto token=peek();bool negative=accept("-");if(negative)token=peek();
   if(token.text=="true"||token.text=="false"||(!token.text.empty()&&std::isdigit(static_cast<unsigned char>(token.text[0])))||
      string_token(token.text)||token.text.starts_with("#\\")||token.text.starts_with("b!#\\")){
    if(negative&&(token.text.empty()||!std::isdigit(static_cast<unsigned char>(token.text[0]))))throw Error(p.where,"negative pattern literal must be numeric");
    if(string_token(token.text)||token.text.starts_with("b!#\\")||token.text.starts_with("#\\")){
     auto value=atom();
     if(value.node->kind==Expr::Kind::ByteBuffer||value.node->kind==Expr::Kind::Interpolated)
      throw Error(p.where,"pattern requires constant scalar or string literal");
     p.name=std::move(value.node->text);
    }else{++pos;p.name=literals::radix_integer(token.text,token.where,negative);}
    p.kind=P::Literal;return p;
   }
   if(negative)throw Error(p.where,"expected a numeric pattern literal");
   p.name=ident();
    if(accept("(")){p.kind=P::Variant;if(!at(")"))do{p.elements.push_back(match_pattern(dash_wildcard));if(p.elements.size()>256)throw Error(p.where,"pattern width limit exceeded");}while(accept(",")&&!at(")"));consume(")");}
   return p;
  }
   Pattern match_pattern(bool dash_wildcard){
    Nesting guard(*this);auto p=match_pattern_atom(dash_wildcard);
    if(accept("as")){Pattern alias;alias.kind=Pattern::Kind::As;alias.where=p.where;alias.name=ident();alias.elements.push_back(std::move(p));p=std::move(alias);}
    if(accept("|")){Pattern either;either.kind=Pattern::Kind::Or;either.where=p.where;either.elements.push_back(std::move(p));either.elements.push_back(match_pattern(dash_wildcard));return either;}
   return p;
  }
    Expression quotation(const Location& where){
     Nesting guard(*this);LexicalRegion scope(*this);Expr region;region.where=where;consume("{");
     while(!at("}")){
      if(at("<eof>"))throw Error(peek().where,"unterminated quotation region");
      if(block_operator_start()){parse_operator_declaration(&region.body);continue;}
      if(fixity_header()){fixity_directive();continue;}
     if(region.body.size()>=100000)throw Error(where,"too many quotation statements");
     const bool labelled_statement=is_ident(peek().text)&&peek(1).text==":"&&
       (peek(2).text=="while"||peek(2).text=="for");
     const bool structured=at("let")||at("var")||at("return")||at("if")||
       at("while")||at("{")||taskgroup_header()||defer_header()||for_header()||
       labelled_statement||
       ((at("break")||at("continue"))&&peek(1).text!="(")||
       peek(1).text=="="||peek(1).text==":=";
     if(structured){region.body.push_back(statement());continue;}
     const bool match_statement=(match_header()&&!regex_header())||
       (at("poll")&&peek(1).text=="{");
     auto value=expr().node;
     const bool syntax_block=value->kind==Expr::Kind::SyntaxLet&&
       value->children.size()==1&&value->children[0]->kind==Expr::Kind::SyntaxBlock;
     if(accept(";")||syntax_block){
      if(match_statement&&value->kind==Expr::Kind::PatternMatch)
       value->statement_match=true;
      Stmt action;action.kind=Stmt::Kind::Expr;action.where=value->where;
      action.value=std::move(value);region.body.push_back(std::move(action));continue;
     }
     if(at("}")){region.children.push_back(std::move(value));break;}
     if(match_statement&&value->kind==Expr::Kind::PatternMatch){
      value->statement_match=true;
      Stmt action;action.kind=Stmt::Kind::Expr;action.where=value->where;
      action.value=std::move(value);region.body.push_back(std::move(action));continue;
     }
     if(value->kind==Expr::Kind::Loop){
      // A final loop remains the pre-existing value-loop expression. A loop
      // followed by another region item is the ordinary semicolon-free loop
      // statement spelling.
      Stmt loop;loop.kind=Stmt::Kind::While;loop.where=value->where;
      loop.label=std::move(value->text);loop.label_context=value->context;
      loop.body=std::move(value->body);loop.value=std::make_unique<Expr>();
      loop.value->kind=Expr::Kind::Boolean;loop.value->where=loop.where;
      loop.value->text="true";region.body.push_back(std::move(loop));continue;
     }
     throw Error(peek().where,"quotation expression statement requires ';' before '"+peek().text+"'","ON1X-PARSE-EXPECTED",";",peek().text);
    }
    consume("}");
    if(region.children.empty()){
     auto unit=std::make_unique<Expr>();unit->kind=Expr::Kind::Unit;
     unit->where=where;region.children.push_back(std::move(unit));
    }
    auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Quote;e->where=where;
    if(region.body.empty())e->children.push_back(std::move(region.children.at(0)));
    else{
     // Keep Unit-v1 PPX and the checked staging HIR unchanged: a lexical
     // quotation region is source sugar for an immediately invoked,
     // result-inferred lambda inside the quote. The original statement/tail
     // locations survive on the generated body and return.
     auto thunk=std::make_unique<Expr>();thunk->kind=Expr::Kind::Lambda;
     thunk->where=where;thunk->result=Type::Hole;thunk->body=std::move(region.body);
     auto tail=std::move(region.children.at(0));
     if(tail->kind!=Expr::Kind::Unit){
      Stmt result;result.kind=Stmt::Kind::Return;result.where=tail->where;
      result.value=std::move(tail);thunk->body.push_back(std::move(result));
     }
     auto depth=body_expression_depth(thunk->body)+1;
     auto call=std::make_unique<Expr>();call->kind=Expr::Kind::Call;call->where=where;
     call->callee=bounded(std::move(thunk),depth).node;
     e->children.push_back(syntax_bounded(std::move(call)).node);
    }
    return syntax_bounded(std::move(e));
   }
    void value_block(Expr& arm){
     Nesting guard(*this);LexicalRegion scope(*this);consume("{");
     while(!at("}")){
       if(at("<eof>"))throw Error(peek().where,"unterminated value block");
       if(block_operator_start()){parse_operator_declaration(&arm.body);continue;}
       if(fixity_header()){fixity_directive();continue;}
      if(arm.body.size()>=100000)throw Error(arm.where,"too many value block statements");
     bool structured=at("let")||at("var")||at("return")||at("if")||at("while")||at("{")||at("let_syntax")||at("letrec_syntax")||taskgroup_header()||defer_header()||for_header()||
     (at("loop")&&peek(1).text=="{")||((at("break")||at("continue"))&&peek(1).text!="(")||
     (is_ident(peek().text)&&peek(1).text==":"&&(peek(2).text=="loop"||peek(2).text=="while"||peek(2).text=="for"))||peek(1).text=="="||peek(1).text==":=";
    if(structured){arm.body.push_back(statement());continue;}
    auto value=expr().node;
    if(accept(";")){Stmt s;s.kind=Stmt::Kind::Expr;s.where=value->where;s.value=std::move(value);arm.body.push_back(std::move(s));}
    else{arm.children.push_back(std::move(value));consume("}");return;}
   }
   consume("}");auto unit=std::make_unique<Expr>();unit->kind=Expr::Kind::Unit;unit->where=arm.where;arm.children.push_back(std::move(unit));
  }
   Expression matching(bool statement_mode){
   auto where=peek().where;consume("match");auto e=std::make_unique<Expr>();e->kind=Expr::Kind::PatternMatch;e->where=where;e->statement_match=statement_mode;
   bool old_parens=at("(");
   if(accept("let")){e->text=ident();consume("=");old_parens=false;}
   e->children.push_back(expr().node);
   if(!accept("with")){
    if(!old_parens||!e->text.empty())consume("with");
    // Preserve the old spelling and its payload-lambda semantics exactly.
    e->kind=Expr::Kind::Match;e->statement_match=false;consume("{");
    if(!at("}")){do{e->labels.push_back(ident());consume("(");auto arm=std::make_unique<Expr>();arm->kind=Expr::Kind::Lambda;arm->where=peek().where;arm->result=Type::Hole;arm->parameters.push_back(parameter(0));consume(")");consume("=>");
     Stmt s;s.kind=Stmt::Kind::Return;s.where=peek().where;s.value=expr().node;arm->body.push_back(std::move(s));auto depth=body_expression_depth(arm->body)+1;auto checked=bounded(std::move(arm),depth);e->children.push_back(std::move(checked.node));
     if(e->labels.size()>256)throw Error(where,"match arm limit exceeded");
    }while(accept(","));}
     consume("}");return syntax_bounded(std::move(e));
    }
    return matching_arms(std::move(e));
   }
   Expression matching_arms(ExprPtr e){
     consume("{");
    if(!at("}")){do{
     auto arm=std::make_unique<Expr>();arm->kind=Expr::Kind::Case;arm->where=peek().where;arm->patterns.push_back(match_pattern(e->sum_match));
      if(accept("if")){arm->text="@guard";arm->children.push_back(expr().node);}
      else arm->children.push_back(truth(arm->where));
      consume("=>");if(at("{"))value_block(*arm);else arm->children.push_back(expr().node);
     auto depth=std::max({body_expression_depth(arm->body),arm->children[0]->syntax_depth,arm->children[1]->syntax_depth})+1;
     auto checked=bounded(std::move(arm),depth);e->children.push_back(std::move(checked.node));
     if(e->children.size()>257)throw Error(e->where,"match arm limit exceeded");
   }while(accept(",")&&!at("}"));}
    consume("}");return syntax_bounded(std::move(e));
   }
   ExprPtr csp_duration(const Location& at){
    auto duration=expr().node;
    if(!accept("s")){accept("ms");return duration;}
    auto multiply=std::make_unique<Expr>();multiply->kind=Expr::Kind::Binary;multiply->text="*";multiply->where=at;
    multiply->children.push_back(std::move(duration));
    auto scale=std::make_unique<Expr>();scale->kind=Expr::Kind::Integer;scale->text="1000";scale->where=at;multiply->children.push_back(std::move(scale));
    return syntax_bounded(std::move(multiply)).node;
   }
   Expression polling(bool statement_mode){
    auto where=peek().where;consume("poll");consume("{");
    auto choice=std::make_unique<Expr>();choice->kind=Expr::Kind::SourceCsp;choice->where=where;choice->text="poll";
    auto match=std::make_unique<Expr>();match->kind=Expr::Kind::PatternMatch;match->where=where;match->statement_match=statement_mode;
    std::vector<std::string> endpoints;std::set<std::string> closed;bool has_default=false;ExprPtr deadline;
    do{
     if(at("}"))break;
     auto site=peek().where;std::string label,binder;
     if(at("recv")||at("send")){
      bool send=accept("send");if(!send)consume("recv");auto name=ident();
      if(std::find(endpoints.begin(),endpoints.end(),name)!=endpoints.end())throw Error(site,"ON1X-CSP: duplicate poll endpoint: "+name);
      if(endpoints.size()>=16)throw Error(site,"ON1X-CSP-LIMIT: poll offer limit exceeded");
      auto offer=std::make_unique<Expr>();offer->kind=Expr::Kind::SourceCsp;offer->where=site;offer->text=send?"offer_send":"offer_recv";
      auto endpoint=std::make_unique<Expr>();endpoint->kind=Expr::Kind::Variable;endpoint->where=site;endpoint->text=name;offer->children.push_back(std::move(endpoint));
      if(send){consume("<-");offer->children.push_back(expr().node);}else{consume("as");binder=ident();}
      label=(send?"Sent":"Recv")+std::to_string(endpoints.size());
      endpoints.push_back(name);choice->children.push_back(std::move(offer));
     }else if(accept("closed")){
      auto name=ident();if(!closed.insert(name).second)throw Error(site,"ON1X-CSP: duplicate poll closed arm: "+name);
      label="@closed:"+name;
     }else if(accept("default")){
      if(has_default)throw Error(site,"ON1X-CSP: duplicate poll default arm");
      has_default=true;label="Default";
     }else if(accept("after")){
      if(deadline)throw Error(site,"ON1X-CSP: duplicate poll deadline");
      deadline=std::make_unique<Expr>();deadline->kind=Expr::Kind::SourceCsp;deadline->text="offer_after";deadline->where=site;
      deadline->children.push_back(csp_duration(site));label="After";
     }
     else throw Error(site,"expected recv, send, closed or default poll offer");
     consume("=>");auto arm=std::make_unique<Expr>();arm->kind=Expr::Kind::Case;arm->where=site;
     Pattern pattern;pattern.kind=Pattern::Kind::Variant;pattern.where=site;pattern.name=label;
     if(!binder.empty()){Pattern p;p.kind=Pattern::Kind::Name;p.where=site;p.name=binder;pattern.elements.push_back(std::move(p));}
     arm->patterns.push_back(std::move(pattern));arm->children.push_back(truth(site));
      if(at("{"))value_block(*arm);else arm->children.push_back(expr().node);
     match->children.push_back(std::move(arm));
    }while(accept(",")&&!at("}"));consume("}");
    if(endpoints.empty()&&!deadline)throw Error(where,"ON1X-CSP: poll requires a channel offer or deadline");
    for(auto& arm:match->children){auto& name=arm->patterns[0].name;
     if(name.starts_with("@closed:")){
      auto found=std::find(endpoints.begin(),endpoints.end(),name.substr(8));
      if(found==endpoints.end())throw Error(arm->where,"ON1X-CSP: closed arm has no channel offer");
      name="Closed"+std::to_string(found-endpoints.begin());
     }
    }
    for(const auto& name:endpoints)if(!closed.contains(name))throw Error(where,"ON1X-CSP: poll requires a closed arm for "+name);
    if(deadline)choice->children.push_back(std::move(deadline));
    if(has_default){auto marker=std::make_unique<Expr>();marker->kind=Expr::Kind::Boolean;marker->text="true";marker->where=where;choice->children.push_back(std::move(marker));}
    match->children.insert(match->children.begin(),std::move(choice));
    return syntax_bounded(std::move(match));
   }
   Param parameter(std::size_t index,bool allow_ref=false){
   Param result;result.where=peek().where;
   if(at("(")){result.pattern=pattern();result.name="@argument_"+std::to_string(index);}
   else result.name=ident();
   if(!accept(":")){result.type=Type::Hole;return result;}
     if(at("ref")||at("borrow")){
     if(!allow_ref||result.pattern)
      throw Error(peek().where,"borrow/ref requires a simple parameter of a named, non-extern On1x function","ON1X-REF");
     const bool exclusive=accept("ref");if(!exclusive)consume("borrow");
     result.borrow=exclusive?BorrowMode::Exclusive:BorrowMode::Shared;
     result.type=type();
    }else result.type=type();
   return result;
  }
  std::size_t group_end(std::size_t start,const char* open,const char* close)const{
   std::size_t depth=0;
   for(std::size_t i=start;pos+i<ts.size();++i){
    if(peek(i).text==open)++depth;
    else if(peek(i).text==close&&!--depth)return i;
   }
   return 0;
  }
  enum class ForHeader { None, Clauses, Pattern };
  ForHeader for_form()const{
   if(!at("for"))return ForHeader::None;
   const auto& first=peek(1).text;
   if(first=="("){
    if(const auto end=group_end(1,"(",")")){
     if(peek(end+1).text=="in")return ForHeader::Pattern;
     if(peek(end+1).text=="{")return ForHeader::Clauses;
    }
   }else if(first=="["){
    if(const auto end=group_end(1,"[","]");end&&peek(end+1).text=="in")return ForHeader::Pattern;
   }else if(is_ident(first)){
    const auto& second=peek(2).text;
    if(second=="in"||second=="as"||second=="|"||second=="::")return ForHeader::Pattern;
    if(second=="("){
     if(const auto end=group_end(2,"(",")");end&&peek(end+1).text=="in")return ForHeader::Pattern;
    }
    if(first=="record"&&second=="{"){
     if(const auto end=group_end(2,"{","}");end&&peek(end+1).text=="in")return ForHeader::Pattern;
    }
   }else if(templating&&first=="$"&&is_ident(peek(2).text)&&peek(3).text=="in"){
    return ForHeader::Pattern;
   }
   return ForHeader::None;
  }
  bool for_header()const{return for_form()!=ForHeader::None;}
  Stmt clause(const std::string& end,bool binding=true){
   Stmt s;s.where=peek().where;
   if(binding&&(at("let")||at("var"))){
    s.kind=Stmt::Kind::Let;s.mutable_binding=accept("var");if(!s.mutable_binding)consume("let");
    if(at("("))s.pattern=pattern();else s.name=ident();
    if(accept(":"))s.declared=type();
    consume("=");s.value=expr().node;
   }else if(peek(1).text=="="||peek(1).text==":="){
    s.kind=Stmt::Kind::Assign;s.name=ident();s.ref_store=accept(":=");if(!s.ref_store)consume("=");s.value=expr().node;
   }else{s.kind=Stmt::Kind::Expr;s.value=expr().node;}
   consume(end);return s;
  }
  ExprPtr truth(const Location& at){auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Boolean;e->text="true";e->where=at;return e;}
  std::string iteration_name(){
   return std::string(canonical::iteration_name_prefix)+std::to_string(++iteration_names);
  }
  [[noreturn]] void iteration_pattern_error(const Location& at){
   throw Error(at,"ON1X-FOR-PATTERN: expected an irrefutable name, '_' or tuple pattern; list/cons patterns are unsupported","ON1X-FOR-PATTERN");
  }
  void iteration_binders(Pattern& p){
   using P=Pattern::Kind;
   if(p.kind==P::Wildcard||(p.kind==P::Name&&p.name=="_")){
    // Lower wildcard leaves to inaccessible, ordinary immutable locals.
    p.kind=P::Name;p.name=iteration_name();
   }else if(p.kind==P::Rest){
    // `rest` is special in a match arm, not a reserved iteration binder.
    p.kind=P::Name;p.name="rest";
   }else if(p.kind!=P::Name&&p.kind!=P::Tuple)iteration_pattern_error(p.where);
   for(auto& child:p.elements)iteration_binders(child);
  }
  Stmt finite_iteration(Stmt loop,Pattern pattern,ExprPtr subject){
   using E=Expr::Kind;using S=Stmt::Kind;
   iteration_binders(pattern);
   const auto at=loop.where,subject_at=subject->where;
   const auto snapshot=iteration_name(),limit=iteration_name(),cursor=iteration_name();
   auto local=[](const std::string& name,const Location& at){return literal(E::Variable,at,name);};
   auto binary=[&](const char* op,ExprPtr left,ExprPtr right){
    auto e=literal(E::Binary,at,op);e->children.push_back(std::move(left));
    e->children.push_back(std::move(right));return syntax_bounded(std::move(e)).node;
   };
   auto bind=[](std::string name,ExprPtr value,Location site,bool mut=false){
    Stmt s;s.kind=S::Let;s.where=std::move(site);s.name=std::move(name);
    s.value=std::move(value);s.mutable_binding=mut;return s;
   };
   Stmt scope;scope.kind=S::Block;scope.where=at;
   // All backends receive ordinary checked data and control. Snapshot the
   // operand before introducing any user binders, and cache its logical length.
   scope.body.push_back(bind(snapshot,std::move(subject),subject_at));
   auto length=literal(E::Length,subject_at,std::string(canonical::iteration_length_marker));
   length->children.push_back(local(snapshot,subject_at));
   scope.body.push_back(bind(limit,syntax_bounded(std::move(length)).node,subject_at));
   scope.body.push_back(bind(cursor,literal(E::Integer,at,"0"),at,true));
   loop.value=binary("<",local(cursor,at),local(limit,at));
   auto element=literal(E::Index,pattern.where);
   element->children.push_back(local(snapshot,pattern.where));
   element->children.push_back(local(cursor,pattern.where));
   auto item=bind({},syntax_bounded(std::move(element)).node,pattern.where);
   if(pattern.kind==Pattern::Kind::Name)item.name=std::move(pattern.name);
   else item.pattern=std::move(pattern);
   // The iteration binder and the user's block have distinct scopes, as with
   // ranges. The canonical latch runs after body cleanup, including continue.
   Stmt body;body.kind=S::Block;body.where=peek().where;body.body=block();
   loop.body.push_back(std::move(item));loop.body.push_back(std::move(body));
   Stmt step;step.kind=S::Assign;step.where=at;step.name=cursor;
   step.value=binary("+",local(cursor,at),literal(E::Integer,at,"1"));
   loop.otherwise.push_back(std::move(step));scope.body.push_back(std::move(loop));return scope;
  }
  Stmt iteration(std::string label={}){
   Stmt s;s.where=peek().where;s.label=std::move(label);s.kind=Stmt::Kind::While;
   if(accept("loop")){s.value=truth(s.where);s.body=block();return s;}
   if(accept("while")){s.value=expr().node;s.body=block();return s;}
   const auto form=for_form();consume("for");
   if(form!=ForHeader::Pattern&&accept("(")){
    Stmt scope;scope.kind=Stmt::Kind::Block;scope.where=s.where;
    if(!accept(";"))scope.body.push_back(clause(";"));
    if(accept(";"))s.value=truth(s.where);else{s.value=expr().node;consume(";");}
    if(!accept(")"))s.otherwise.push_back(clause(")",false));
    s.body=block();scope.body.push_back(std::move(s));return scope;
   }
   const bool simple=(is_ident(peek().text)&&peek(1).text=="in")||
     (templating&&at("$")&&is_ident(peek(1).text)&&peek(2).text=="in");
   Pattern binder;
   if(simple){binder.where=peek().where;binder.name=ident();}
   else binder=match_pattern(false);
   if(at("::"))iteration_pattern_error(peek().where);
   consume("in");
   auto start=expr().node;
   if(!at("..")&&!at("..="))return finite_iteration(std::move(s),std::move(binder),std::move(start));
   if(!simple)throw Error(binder.where,"ON1X-FOR-PATTERN: i64 ranges require a single named binder","ON1X-FOR-PATTERN");
   s.kind=Stmt::Kind::Range;s.name=std::move(binder.name);
   auto bounds=std::make_unique<Expr>();bounds->kind=Expr::Kind::Tuple;bounds->where=start->where;
   bounds->children.push_back(std::move(start));
   s.inclusive=accept("..=");if(!s.inclusive)consume("..");bounds->children.push_back(expr().node);
   s.value=syntax_bounded(std::move(bounds)).node;s.body=block();return s;
  }
   bool defer_header()const{
    if(!at("defer"))return false;
    // Preserve ordinary `defer(...)`, `defer;`, assignments and operators.
    auto next=peek(1).text;return is_ident(next)||next=="@"||next=="{"||(!next.empty()&&(std::isdigit(static_cast<unsigned char>(next[0]))||next[0]=='"'));
   }
   bool taskgroup_header()const{return at("taskgroup")&&(peek(1).text=="{"||peek(1).text=="within");}
     Stmt statement(){Stmt s; s.where=peek().where;
     if(at("unsafe")&&peek(1).text=="{"){
      consume("unsafe");s.kind=Stmt::Kind::Block;s.unsafe_block=true;s.body=block();return s;
     }
    if(taskgroup_header()){
     ++pos;s.kind=Stmt::Kind::TaskGroup;
     if(accept("within"))s.value=csp_duration(s.where);
     s.body=block();return s;
    }
     if(at("poll")&&peek(1).text=="{"){s.kind=Stmt::Kind::Expr;s.value=polling(true).node;accept(";");return s;}
    if(defer_header()){
     if(peek(1).text=="{")throw Error(s.where,"defer block syntax is unsupported; use defer fn(){ ... }();","ON1X-DEFER");
     ++pos;s.kind=Stmt::Kind::Defer;s.value=expr().node;consume(";");
     if(s.value->kind!=Expr::Kind::Call&&s.value->kind!=Expr::Kind::MacroCall&&s.value->kind!=Expr::Kind::Extension)throw Error(s.where,"defer requires a call expression","ON1X-DEFER");
     return s;
    }
   if(match_header()&&!regex_header()){
    s.kind=Stmt::Kind::Expr;s.value=matching(true).node;if(s.value->kind==Expr::Kind::PatternMatch)accept(";");else consume(";");return s;
   }
   if(is_ident(peek().text)&&peek(1).text==":"&&
      (peek(2).text=="loop"||peek(2).text=="while"||peek(2).text=="for")){
    auto label=ident();consume(":");return iteration(std::move(label));
   }
   if((at("loop")&&peek(1).text=="{")||at("while")||for_header())return iteration();
   if((at("break")||at("continue"))&&peek(1).text!="("&&peek(1).text!="="&&
      (peek(1).text==";"||is_ident(peek(1).text))){
    bool exits=accept("break");if(!exits)consume("continue");s.kind=exits?Stmt::Kind::Break:Stmt::Kind::Continue;
     // `with` remains a legal label in the historical `break with;` form.
     if(exits&&at("with")&&peek(1).text!=";"){++pos;s.value=expr().node;}
     else{
      if(!at(";"))s.label=ident();
      if(exits&&accept("with"))s.value=expr().node;
     }
    consume(";");return s;
   }
   if(at("let_syntax")||at("letrec_syntax")){
    s.kind=Stmt::Kind::Expr;s.value=syntax_scope().node;
    if(s.value->children[0]->kind==Expr::Kind::SyntaxBlock)accept(";");else consume(";");
    return s;
   }
  if(accept("let")||accept("var")){
   s.kind=Stmt::Kind::Let;s.mutable_binding=ts[pos-1].text=="var";
    if(at("("))s.pattern=pattern();else s.name=ident();
    if(accept(":"))s.declared=type();
    consume("=");s.value=expr().node;consume(";");return s;
  }
   if(accept("return")){s.kind=Stmt::Kind::Return;if(!at(";"))s.value=expr().node;consume(";");return s;}
   if(accept("if")){s.kind=Stmt::Kind::If;s.value=expr().node;s.body=block();if(accept("else"))s.otherwise=block();return s;}
  if(at("{")){s.kind=Stmt::Kind::Block;s.body=block();return s;}
  if(peek(1).text=="="||peek(1).text==":="){
    s.kind=Stmt::Kind::Assign;s.name=ident();s.ref_store=accept(":=");if(!s.ref_store)consume("=");s.value=expr().node;consume(";");return s;
  }
   s.kind=Stmt::Kind::Expr;s.value=expr().node;consume(";");return s;
 }
  public:
   explicit Parser(const std::vector<Token>&t,const std::vector<std::string>& enabled,const OperatorScope* scope=nullptr):ts(t),operators(scope){
   for(const auto& name:enabled)enabled_extensions.insert(extensions::canonical(name,{"<command-line>"}));
  }
   std::vector<InterfaceMember> interface(){
    if(!at("on1xi"))
     throw Error(peek().where,"expected interface header 'on1xi 1;'","ON1X-INTERFACE-VERSION");
    consume("on1xi");
    if(!at("1"))
     throw Error(peek().where,"unsupported interface version: "+peek().text+" (expected 1)","ON1X-INTERFACE-VERSION");
    consume("1");consume(";");
    std::vector<InterfaceMember> members;
    std::set<std::string> names;
    auto signature_type=[&](bool result){
     auto where=peek().where;auto shape=type();
     auto allowed=[&](auto&& self,const Type& t)->bool{
      if(t==Type::I64||t==Type::Bool||t==Type::String)return true;
      if(!t.is_tuple())return false;
      if(t.elements().size()>32)
       throw Error(where,"interface tuple width limit exceeded","ON1X-INTERFACE-LIMIT");
      return std::all_of(t.elements().begin(),t.elements().end(),[&](const auto& part){return self(self,part);});
     };
     if(!(result&&shape==Type::Void)&&!allowed(allowed,shape))
      throw Error(where,"interface v1 expects i64, bool, string or tuples of these (void only as a result)",
                  "ON1X-INTERFACE-SHAPE");
     return shape;
    };
    if(at("<eof>"))throw Error(peek().where,"interface requires at least one function declaration","ON1X-INTERFACE-SHAPE");
    while(!at("<eof>")){
     if(members.size()>=256)throw Error(peek().where,"interface member limit exceeded","ON1X-INTERFACE-LIMIT");
     if(!at("fn"))
      throw Error(peek().where,"interface v1 accepts only 'fn name(parameter: type, ...) -> type;' declarations",
                  "ON1X-INTERFACE-SHAPE");
     consume("fn");auto where=peek().where;auto name=ident();
     if(!names.insert(name).second)
      throw Error(where,"duplicate interface member: "+name,"ON1X-INTERFACE-DUPLICATE");
     consume("(");std::vector<Type> parameters;std::set<std::string> parameter_names;
     if(!at(")"))do{
      auto parameter=peek().where;
      if(parameters.size()>=32)throw Error(parameter,"interface parameter limit exceeded","ON1X-INTERFACE-LIMIT");
      auto label=ident();
      if(!parameter_names.insert(label).second)
       throw Error(parameter,"duplicate interface parameter: "+label,"ON1X-INTERFACE-DUPLICATE");
      consume(":");parameters.push_back(signature_type(false));
     }while(accept(","));
     consume(")");consume("->");auto result=signature_type(true);consume(";");
     members.push_back({where,std::move(name),Type::function(std::move(parameters),std::move(result))});
    }
     return members;
    }
     std::vector<std::string> signature_parameters(){
      std::vector<std::string> parameters;
      std::set<std::string> seen;
      if(accept("<")){
       do{
        auto where=peek().where;
        if(parameters.size()>=32)
         throw Error(where,"interface type parameter limit exceeded","ON1X-INTERFACE-LIMIT");
        auto name=ident();
        if(!seen.insert(name).second)
         throw Error(where,"duplicate interface type parameter: "+name,"ON1X-INTERFACE-DUPLICATE");
        parameters.push_back(std::move(name));
       }while(accept(","));
       close_angle();
      }
      return parameters;
     }
     SignatureReference signature_reference(){
      SignatureReference reference;reference.where=peek().where;reference.name=ident();
      if(accept("<")){
       do{
        if(reference.arguments.size()>=32)
         throw Error(peek().where,"signature argument limit exceeded","ON1X-INTERFACE-LIMIT");
        reference.arguments.push_back(type());
       }while(accept(","));
       close_angle();
      }
      return reference;
     }
     bool signature_keyword(ModuleSignature& signature,const char* spelling){
      if(!at(spelling))return false;
      signature.context_keywords.push_back(peek().where.offset);consume(spelling);
      return true;
     }
     void signature_declarations(ModuleSignature& signature,NamedSignature& body,bool named){
      std::set<std::string> functions,types;
      while(!at("<eof>")&&!(named&&at("}"))){
       if(body.members.size()+body.types.size()+body.includes.size()+body.sharing.size()>=256)
        throw Error(peek().where,"interface member limit exceeded","ON1X-INTERFACE-LIMIT");
       if(named&&signature_keyword(signature,"include")){
        body.includes.push_back(signature_reference());consume(";");continue;
       }
       if(named&&signature_keyword(signature,"sharing")){
        if(!signature_keyword(signature,"type"))consume("type");
        auto where=peek().where;auto name=ident();consume("=");
        body.sharing.push_back({std::move(where),std::move(name),type()});consume(";");continue;
       }
       if(at("abstract")||at("type")){
        const bool abstract=signature_keyword(signature,"abstract");
        if(!signature_keyword(signature,"type"))consume("type");
        auto where=peek().where;auto name=ident();
        if(!types.insert(name).second)
         throw Error(where,"duplicate interface type: "+name,"ON1X-INTERFACE-DUPLICATE");
        if(named&&bound_types.contains(name))
         throw Error(where,"signature parameter and type member have the same name: "+name,"ON1X-INTERFACE-DUPLICATE");
        const auto arity=signature_parameters().size();
        std::optional<Type> manifest;
        if(named&&!abstract&&accept("=")){
         if(arity)throw Error(where,"manifest type equations must be monomorphic","ON1X-INTERFACE-UNSUPPORTED");
         manifest=type();
        }
        consume(";");
        body.types.push_back({std::move(where),std::move(name),arity,std::move(manifest)});
        continue;
       }
       const bool procedure=accept("proc");
       if(!procedure&&!accept("fn"))
        throw Error(peek().where,named?"expected a signature type, callable, include or sharing declaration":
                    "abstract module accepts only 'type T;', 'fn name(...) -> type;' and 'proc name(...);' declarations",
                    "ON1X-INTERFACE-SHAPE");
       auto where=peek().where;auto name=ident();
       if(!functions.insert(name).second)
        throw Error(where,"duplicate interface member: "+name,"ON1X-INTERFACE-DUPLICATE");
       consume("(");std::vector<Type> parameters;std::set<std::string> parameter_names;
       if(!at(")"))do{
        auto at=peek().where;
        if(parameters.size()>=32)
         throw Error(at,"interface parameter limit exceeded","ON1X-INTERFACE-LIMIT");
        if(!parameter_names.insert(ident()).second)
         throw Error(at,"duplicate interface parameter","ON1X-INTERFACE-DUPLICATE");
        consume(":");parameters.push_back(type());
       }while(accept(","));
       consume(")");Type result=Type::Void;
       if(!procedure){consume("->");result=type();}
       consume(";");
       body.members.push_back({std::move(where),std::move(name),
                               Type::function(std::move(parameters),std::move(result)),procedure});
      }
     }
     ModuleSignature module_signature(){
      ModuleSignature signature;signature.where=peek().where;
      if(!at("signature")&&!at("module")&&!at("abstract")){
       auto members=interface();
      signature.members.reserve(members.size());
      for(auto& member:members)
       signature.members.push_back({std::move(member.where),std::move(member.name),
                                    std::move(member.signature)});
       return signature;
      }
      while(signature_keyword(signature,"signature")){
       if(signature.definitions.size()>=64)
        throw Error(peek().where,"named signature limit exceeded","ON1X-INTERFACE-LIMIT");
       NamedSignature definition;definition.where=peek().where;definition.name=ident();
       definition.parameters=signature_parameters();
       for(const auto& p:definition.parameters)bound_types.emplace(p,bound_types.size());
       consume("{");signature_declarations(signature,definition,true);consume("}");consume(";");
       bound_types.clear();signature.definitions.push_back(std::move(definition));
      }
      signature.where=peek().where;
      const bool opaque=signature_keyword(signature,"abstract");
      if(!signature_keyword(signature,"module"))consume("module");
      signature.module=module_name();
      if(accept(":")){
       signature.seal=opaque?SignatureSeal::Opaque:SignatureSeal::Transparent;
       signature.ascription=signature_reference();consume(";");consume("<eof>");
       return signature;
      }
      if(!opaque||!signature.definitions.empty())
       throw Error(peek().where,"named signature requires a module ascription","ON1X-INTERFACE-SHAPE");
      consume(";");
      NamedSignature body;signature_declarations(signature,body,false);
      signature.members=std::move(body.members);signature.types=std::move(body.types);
      if(signature.members.empty()&&signature.types.empty())
       throw Error(peek().where,"abstract module requires at least one declaration","ON1X-INTERFACE-SHAPE");
     return signature;
    }
    std::string module_name(){
   auto where=peek().where;std::string name=ident();
   while(accept(".")){name+="."+ident();if(name.size()>4096)throw Error(where,"module name exceeds 4096 bytes");}
    return name;
   }
    std::optional<ModuleHeader> module_header(){
     const bool module_file=peek().where.file.ends_with(".on1xm");
     if(!module_file&&!at("module"))return std::nullopt;
     const auto where=peek().where;
     if(!module_file)
      throw Error(where,"module headers require a .on1xm source file","ON1X-MODULE-HEADER");
     if(!accept("module"))
      throw Error(where,"expected 'module name;' at the start of .on1xm source","ON1X-MODULE-HEADER");
     auto name=module_name();
     if(name.size()>4096)throw Error(where,"module name exceeds 4096 bytes","ON1X-MODULE-HEADER");
     consume(";");
     return ModuleHeader{where,std::move(name)};
    }
    VisibilityRule declaration_visibility(bool module){
     VisibilityRule visibility;
     const auto where=peek().where;
     if(accept("public"))visibility.kind=DeclarationVisibility::Public;
     else if(accept("private"))visibility.kind=DeclarationVisibility::Private;
     else if(accept("shared"))visibility.kind=DeclarationVisibility::Shared;
     else return visibility;
     if(!module)
      throw Error(where,"visibility annotations require a module declaration","ON1X-MODULE-VISIBILITY");
     if(visibility.kind==DeclarationVisibility::Shared)visibility.elevation=1;
     if(at("elevate")){
      if(visibility.kind!=DeclarationVisibility::Shared)
       throw Error(peek().where,"elevate is only allowed with shared visibility","ON1X-MODULE-VISIBILITY");
      consume("elevate");consume("(");
      const auto& n=peek();
      const auto end=n.text.data()+n.text.size();
      const auto [next,ec]=std::from_chars(n.text.data(),end,visibility.elevation);
      const bool decimal=ec==std::errc{}&&next==end;
      if(!decimal||visibility.elevation==0){
       const auto* fault=decimal&&peek(1).text==")"?zero_elevation_fault():nullptr;
       throw Error(n.where,fault?fault:"elevation must be a positive decimal integer in range",
                   "ON1X-MODULE-ELEVATION");
      }
      ++pos;consume(")");
     }
     if(!at("fn")&&!at("proc")&&!at("extern")&&!at("@")&&!operator_declaration_start()&&
        !((at("record")||at("enum")||at("cliche"))&&is_ident(peek(1).text))&&!at("macro"))
      throw Error(peek().where,"visibility annotation must precede a named declaration","ON1X-MODULE-VISIBILITY");
     if(at("@")&&(peek(1).text=="enable"||peek(1).text=="extension"||
                   peek(1).text=="scrutiny"||peek(1).text=="ppx"))
      throw Error(peek().where,"visibility annotations do not apply to directives","ON1X-MODULE-VISIBILITY");
     return visibility;
    }
    NominalDeclaration nominal_declaration(){
     NominalDeclaration d;d.where=peek().where;bool sum=accept("enum");if(!sum)consume("record");d.kind=sum?Type::Sum:Type::Record;d.name=ident();
      static const std::set<std::string> builtins={"i64","f64","f32","u8","u16","u32","u64","bool","string","void","json","json_view","sexpr","buffer","record","sum","array","slice","code","forall","fn","xfiber","_"};
     if(builtins.contains(d.name))throw Error(d.where,"built-in type name cannot be redeclared: "+d.name,"ON1X-NOMINAL");
    if(accept("<")){
     do{auto at=peek().where;auto name=ident();if(d.parameters.size()>=32)throw Error(at,"nominal parameter limit exceeded");
      if(!bound_types.emplace(name,d.parameters.size()).second)throw Error(at,"duplicate nominal parameter: "+name);
      d.parameters.push_back(std::move(name));
     }while(accept(","));close_angle();
    }
    consume("{");std::set<std::string> labels;
    if(!at("}"))do{
     auto where=peek().where;auto label=ident();if(!labels.insert(label).second)throw Error(where,"duplicate nominal field/constructor: "+label);
     if(d.fields.size()>=256)throw Error(where,"nominal field/constructor limit exceeded");
     d.labels.push_back(std::move(label));
     if(!sum){consume(":");d.fields.push_back(type());}
     else{
      std::vector<Type> fields;
      if(accept("(")){if(!at(")"))do{fields.push_back(type());if(fields.size()>256)throw Error(where,"constructor payload width limit exceeded");}while(accept(","));consume(")");}
      d.fields.push_back(fields.empty()?Type::data(Type::Record,{}):fields.size()==1?std::move(fields[0]):Type::tuple(std::move(fields)));
     }
    }while(accept(",")&&!at("}"));
    consume("}");accept(";");bound_types.clear();
    if(sum&&d.fields.empty())throw Error(d.where,"enum requires at least one constructor");
     return d;
    }
     bool cliche_implementation_header()const{
      if(!at("impl"))return false;
      std::size_t angles=0,groups=0;
      for(std::size_t i=1;pos+i<ts.size();++i){
       const auto& token=peek(i).text;
       if(token=="<"){++angles;continue;}
        if(auto closes=angle_closes(token);closes){angles-=std::min(angles,closes);continue;}
       if(token=="("||token=="["){++groups;continue;}
       if(token==")"||token=="]"){if(groups)--groups;continue;}
       if(!angles&&!groups&&token=="for")return true;
       if(!angles&&!groups&&(token=="{"||token==";"||token=="<eof>"))return false;
      }
      return false;
     }
     InherentMember inherent_member(const Type& target,std::size_t implementation_index,
                                     std::size_t member_index,
                                     const std::map<std::string,std::size_t>& implementation_bindings){
      InherentMember member;member.where=peek().where;
      const bool function=accept("fn");
      if(!function&&!accept("value"))
       throw Error(peek().where,"inherent implementation expects 'fn' or 'value'","ON1X-IMPL");
      member.kind=function?InherentMember::Kind::Function:InherentMember::Kind::Value;
      member.name=ident();
      member.runtime_name="@impl_"+std::to_string(implementation_index)+"_"+std::to_string(member_index);
      member.function.where=member.where;member.function.name=member.name;
      std::vector<ClicheConstraint> unsupported_constraints;
      member.type_parameters=generic_parameters(unsupported_constraints);
      if(!unsupported_constraints.empty())
       throw Error(member.where,"bounded inherent members are not supported; use an unconstrained generic member","ON1X-IMPL-UNSUPPORTED");

      auto receiver=[&](std::size_t parameter_index){
       Param parameter;parameter.where=peek().where;
        if(parameter_index==0&&(at("borrow")||at("ref")||at("move"))&&peek(1).text=="self"){
         if(accept("borrow"))parameter.borrow=BorrowMode::Shared;
         else if(accept("ref"))parameter.borrow=BorrowMode::Exclusive;
         else consume("move");
         parameter.name=ident();
         member.receiver=parameter.borrow!=BorrowMode::Value?
                         InherentMember::Receiver::Borrow:InherentMember::Receiver::Move;
        parameter.type=accept(":")?type():target;
        return parameter;
       }
       parameter=this->parameter(parameter_index);
       if(parameter.name=="self"){
        if(parameter_index)
         throw Error(parameter.where,"self must be the first inherent member parameter","ON1X-IMPL-RECEIVER");
        member.receiver=InherentMember::Receiver::Value;
        if(parameter.type==Type::Hole)parameter.type=target;
       }
       return parameter;
      };

      if(function){
       consume("(");
       if(!at(")"))do{
        member.function.params.push_back(receiver(member.function.params.size()));
        if(member.function.params.size()>64)
         throw Error(member.where,"inherent member parameter limit exceeded","ON1X-IMPL-LIMIT");
       }while(accept(","));
       consume(")");if(accept("->"))member.function.result=type();
       if(at("where"))
        throw Error(peek().where,"where constraints on inherent members are not supported","ON1X-IMPL-UNSUPPORTED");
       member.function.body=block();
      }else{
       if(accept("(")){
        if(at(")"))throw Error(peek().where,"member value receiver list cannot be empty","ON1X-IMPL-RECEIVER");
        member.function.params.push_back(receiver(0));
        if(accept(","))throw Error(peek().where,"member values accept only an optional self receiver","ON1X-IMPL-RECEIVER");
        consume(")");
       }
       member.value_type=accept(":")?type():Type::Hole;
       consume("=");member.value=expr().node;consume(";");
      }
      bound_types=implementation_bindings;
      return member;
     }
     InherentImplementation inherent_implementation(std::size_t index){
      InherentImplementation implementation;implementation.where=peek().where;
      consume("impl");bound_types.clear();
      std::vector<ClicheConstraint> unsupported_constraints;
      implementation.type_parameters=generic_parameters(unsupported_constraints);
      if(!unsupported_constraints.empty())
       throw Error(implementation.where,"bounded inherent implementations are not supported","ON1X-IMPL-UNSUPPORTED");
      const auto bindings=bound_types;
      implementation.target=type();
      if(implementation.target.kind()!=Type::Named)
       throw Error(implementation.where,"inherent implementation target must be a named record or enum","ON1X-IMPL");
      if(at("where"))
       throw Error(peek().where,"where constraints on inherent implementations are not supported","ON1X-IMPL-UNSUPPORTED");
      consume("{");
      while(!at("}")){
       if(at("<eof>"))throw Error(implementation.where,"unterminated inherent implementation","ON1X-IMPL");
       if(implementation.members.size()>=256)
        throw Error(peek().where,"inherent implementation member limit exceeded","ON1X-IMPL-LIMIT");
       implementation.members.push_back(inherent_member(
        implementation.target,index,implementation.members.size(),bindings));
      }
      consume("}");accept(";");bound_types.clear();
      if(implementation.members.empty())
       throw Error(implementation.where,"inherent implementation requires at least one member","ON1X-IMPL");
      return implementation;
     }
     ClicheDeclaration cliche_declaration(){
     ClicheDeclaration declaration;declaration.where=peek().where;consume("cliche");
     declaration.name=ident();bound_types.clear();bound_types.emplace("Self",0);
     declaration.type_parameters=generic_parameters(declaration.constraints);
     where_constraints(declaration.constraints);consume("{");std::set<std::string> members;
     const auto base=bound_types;
     while(!accept("}")){
      if(at("<eof>"))throw Error(declaration.where,"unterminated cliche declaration","ON1X-CLICHE-SYNTAX");
      ClicheMember member;member.where=peek().where;
      if(accept("fn"))member.kind=ClicheMember::Kind::Function;
      else if(accept("let"))member.kind=ClicheMember::Kind::Value;
      else throw Error(peek().where,"cliche members must start with 'fn' or 'let'","ON1X-CLICHE-SYNTAX");
      member.name=ident();
      if(!members.insert(member.name).second)
       throw Error(member.where,"duplicate cliche member: "+member.name,"ON1X-CLICHE-SIGNATURE");
      bound_types=base;member.type_parameters=generic_parameters(member.constraints);
      if(member.kind==ClicheMember::Kind::Function){
       consume("(");
       if(!at(")"))do{
        member.params.push_back(parameter(member.params.size()));
        if(member.params.size()>64)throw Error(member.where,"cliche member parameter limit exceeded","ON1X-CLICHE-LIMIT");
       }while(accept(","));
       consume(")");if(accept("->"))member.result=type();
      }else{
       consume(":");member.result=type();
      }
      where_constraints(member.constraints);consume(";");
      declaration.members.push_back(std::move(member));
      if(declaration.members.size()>128)throw Error(declaration.where,"cliche member limit exceeded","ON1X-CLICHE-LIMIT");
     }
     accept(";");bound_types.clear();return declaration;
    }
     ClicheImplementation cliche_implementation(){
     ClicheImplementation implementation;implementation.where=peek().where;consume("impl");
     bound_types.clear();implementation.type_parameters=generic_parameters(implementation.constraints);
     implementation.name=qualified_identifier();
     std::vector<Type> arguments;
     if(accept("<")){
       if(!at_close_angle())do{
       arguments.push_back(type());
       if(arguments.size()>31)throw Error(implementation.where,"implementation argument limit exceeded","ON1X-CLICHE-LIMIT");
      }while(accept(","));
      close_angle();
     }
     consume("for");implementation.arguments.push_back(type());
     implementation.arguments.insert(implementation.arguments.end(),arguments.begin(),arguments.end());
     where_constraints(implementation.constraints);consume("{");std::set<std::string> members;
     const auto base=bound_types;
     while(!accept("}")){
      if(at("<eof>"))throw Error(implementation.where,"unterminated cliche implementation","ON1X-CLICHE-SYNTAX");
      ClicheImplementationMember member;member.where=peek().where;
      if(accept("fn"))member.kind=ClicheMember::Kind::Function;
      else if(accept("let"))member.kind=ClicheMember::Kind::Value;
      else throw Error(peek().where,"implementation members must start with 'fn' or 'let'","ON1X-CLICHE-SYNTAX");
      member.name=ident();member.function.name=member.name;member.function.where=member.where;
      if(!members.insert(member.name).second)
       throw Error(member.where,"duplicate implementation member: "+member.name,"ON1X-CLICHE-SIGNATURE");
      bound_types=base;
      member.function.type_parameters=generic_parameters(member.function.constraints);
      if(member.kind==ClicheMember::Kind::Function){
       consume("(");
       if(!at(")"))do{
        member.function.params.push_back(parameter(member.function.params.size(),true));
        if(member.function.params.size()>64)throw Error(member.where,"implementation member parameter limit exceeded","ON1X-CLICHE-LIMIT");
       }while(accept(","));
       consume(")");if(accept("->"))member.function.result=type();
       where_constraints(member.function.constraints);member.function.body=block();
      }else{
       consume(":");member.value_type=type();member.function.result=member.value_type;
       where_constraints(member.function.constraints);consume("=");member.value=expr().node;consume(";");
      }
      implementation.members.push_back(std::move(member));
      if(implementation.members.size()>128)throw Error(implementation.where,"implementation member limit exceeded","ON1X-CLICHE-LIMIT");
     }
      accept(";");bound_types.clear();return implementation;
     }
    OperatorDeclaration operator_declaration_header(){
     OperatorDeclaration declaration;
     auto& header=declaration.header;auto& function=declaration.function;
     header.where=consume("operator").where;function.where=header.where;
     bound_types.clear();
     function.type_parameters=generic_parameters(function.constraints);
     consume("(");std::set<std::string> attributes,formal_names;
     if(!at(")"))do{
      const auto name=peek();const auto attribute=ident();consume(":");
      if(!attributes.insert(attribute).second)
       throw Error(name.where,"duplicate operator attribute: "+attribute,"ON1X-OPERATOR-DECLARATION");
      if(attribute=="formals"){
       if(!at(")"))do{
        const auto where=peek().where;
         if(!is_ident(peek().text)&&!(templating&&at("$")&&is_ident(peek(1).text)))
         throw Error(where,"operator formals must be simple named parameters","ON1X-OPERATOR-DECLARATION");
        if(function.params.size()>=3)
         throw Error(where,"operators accept at most three formals","ON1X-OPERATOR-ARITY");
        auto formal=parameter(function.params.size(),true);
        if(!formal_names.insert(formal.name).second)
         throw Error(where,"duplicate operator formal: "+formal.name,"ON1X-OPERATOR-DECLARATION");
        function.params.push_back(std::move(formal));
       }while(accept(","));
       break; // Formals are a comma-separated suffix, with no attribute following them.
      }
       const auto value=peek();++pos;
       if(templating&&value.text=="$")
        throw Error(value.where,"macro operator header attributes must be literal","ON1X-OPERATOR-DECLARATION");
      if(attribute=="lexeme")header.lexeme=operator_literal(value);
      else if(attribute=="lexeme2")header.lexeme2=operator_literal(value);
      else if(attribute=="arity"){
       const auto word=operator_word(value);
       if(word=="unary")header.arity=OperatorArity::Unary;
       else if(word=="binary")header.arity=OperatorArity::Binary;
       else if(word=="ternary")header.arity=OperatorArity::Ternary;
       else throw Error(value.where,"operator arity must be unary, binary or ternary","ON1X-OPERATOR-ARITY");
      }else if(attribute=="position"){
       const auto word=operator_word(value);
       if(word=="prefix")header.position=OperatorPosition::Prefix;
       else if(word=="postfix")header.position=OperatorPosition::Postfix;
       else throw Error(value.where,"unary operator position must be prefix or postfix","ON1X-OPERATOR-FIXITY");
      }else if(attribute=="binds"){
       const auto word=operator_word(value);
       if(word=="left")header.binds=OperatorAssociativity::Left;
       else if(word=="right")header.binds=OperatorAssociativity::Right;
       else throw Error(value.where,"operator associativity must be left or right","ON1X-OPERATOR-FIXITY");
      }else if(attribute=="precedence")header.precedence=operator_rank_value(value,false,"precedence");
      else if(attribute=="tightness")header.tightness=operator_rank_value(value,true,"tightness");
      else throw Error(name.where,"unknown operator attribute: "+attribute,"ON1X-OPERATOR-DECLARATION");
     }while(accept(","));
     consume(")");
     for(const auto* required:{"lexeme","arity","formals"})
      if(!attributes.contains(required))
       throw Error(header.where,"missing operator attribute: "+std::string(required),"ON1X-OPERATOR-DECLARATION");
     const std::size_t arity=header.arity==OperatorArity::Unary?1:header.arity==OperatorArity::Binary?2:3;
     if(function.params.size()!=arity)
      throw Error(header.where,"operator requires exactly "+std::to_string(arity)+" formal(s)","ON1X-OPERATOR-ARITY");
     if(header.arity==OperatorArity::Unary){
      if(!attributes.contains("position"))header.position=OperatorPosition::Prefix;
      if(attributes.contains("binds"))
       throw Error(header.where,"binds is only valid on binary and ternary operators","ON1X-OPERATOR-FIXITY");
     }else if(attributes.contains("position"))
      throw Error(header.where,"position is only valid on unary operators","ON1X-OPERATOR-FIXITY");
     if(header.arity==OperatorArity::Ternary){
      if(header.lexeme2.empty()||header.lexeme==header.lexeme2)
       throw Error(header.where,"ternary operator requires a distinct second lexeme","ON1X-OPERATOR-TERNARY");
     }else if(!header.lexeme2.empty())
      throw Error(header.where,"lexeme2 is only valid on ternary operators","ON1X-OPERATOR-TERNARY");
     function.result=accept("->")?type():Type::Hole;
     where_constraints(function.constraints);
     declaration.identity=operator_identity(header);function.name=declaration.identity;
     return declaration;
    }
     void import_selectors(Import& declaration,bool parens=false){
      declaration.selective=true;
      do{
       ImportName name;
       if(at("operator")&&peek(1).text=="("){
        if(declaration.kind==Import::Kind::Program)
         throw Error(peek().where,"program imports cannot select operators","ON1X-OPERATOR-IMPORT");
        pos+=2;name.is_operator=true;name.name=operator_literal(peek());++pos;consume(")");
        if(at("as"))throw Error(peek().where,"selective operator imports cannot be aliased","ON1X-OPERATOR-IMPORT");
       }else{
        name.name=ident();if(accept("as"))name.alias=ident();
       }
       declaration.names.push_back(std::move(name));
      }while(accept(",")&&!(parens&&at(")")));
      if(parens)consume(")");
     }
     bool colon_import_selectors(Import& declaration){
      if(!at(":"))return false;
      if(!declaration.alias.empty())
       throw Error(peek().where,"a selective import cannot alias its whole target; alias individual selectors instead",
                   "ON1X-IMPORT-SELECTORS");
      consume(":");
      import_selectors(declaration);
      return true;
     }
     bool import_declaration(Unit& u){
    // `foreign` and `kernel` are contextual: bare, dotted, and aliased
    // ordinary module imports still use the ordinary import grammar.
     const bool unsafe_import=at("unsafe")&&peek(1).text=="import";
     if(unsafe_import&&peek(2).text=="foreign")
      throw Error(peek().where,"unsafe import foreign has been removed; use import foreign stdffi.<kernel> "
                  "and remove unsafe blocks around portable calls, or use unsafe import kernel mulib.<kernel> "
                  "where KernelBind supports it",
                  "ON1X-STDFFI-MIGRATION");
     const auto& target=peek(2).text;
     const bool target_follows=(is_ident(target)&&target!="as")||target.starts_with('"')||
                               target==".."||target=="..."||target=="[";
     auto check_import_count=[&]{
      if(u.foreign_imports.size()+u.kernel_imports.size()+u.module_imports.size()>1024)
       throw Error(peek().where,"import count limit exceeded");
     };
     if(at("import")&&peek(1).text=="foreign"&&target_follows){
      ForeignImport item;item.where=peek().where;
      consume("import");consume("foreign");consume("stdffi");consume(".");
      item.kernel=ident();
      if(item.kernel.size()>4096)throw Error(item.where,"stdffi kernel name exceeds 4096 bytes");
      item.alias=accept("as")?ident():item.kernel;
      consume(";");
      u.foreign_imports.push_back(std::move(item));
      check_import_count();
      return true;
     }
    const bool unsafe_kernel=unsafe_import&&peek(2).text=="kernel";
    if(unsafe_kernel||(at("import")&&peek(1).text=="kernel"&&
        target_follows)){
     KernelImport item;item.where=peek().where;item.is_unsafe=unsafe_kernel;
     if(unsafe_kernel)consume("unsafe");
     consume("import");consume("kernel");
     if(accept("[")){consume("dynlib");consume("]");item.dynlib=true;}
     item.module=module_name();
     if(item.module.size()>4096)throw Error(item.where,"kernel module name exceeds 4096 bytes");
     if(accept("as"))item.alias=ident();
     else item.alias=item.module.substr(item.module.rfind('.')+1);
     consume(";");
     u.kernel_imports.push_back(std::move(item));
      check_import_count();
      return true;
    }
     // `program` is an ordinary name unless another target follows it.
     if(at("import")&&peek(1).text=="program"&&
        ((is_ident(target)&&target!="as")||target.starts_with('"'))){
      Import item;item.where=peek().where;item.kind=Import::Kind::Program;
      consume("import");consume("program");
      if(peek().text.starts_with('"')){
       const auto& token=peek();
       if(token.text.starts_with("\"\"\"")||token.text.find('\\')!=std::string::npos)
        throw Error(token.where,"program import paths must be unescaped double-quoted strings","ON1X-PROGRAM-IMPORT");
       if(token.text.size()<=2||token.text.size()>4098)
        throw Error(token.where,"import path must be 1..4096 bytes","ON1X-PROGRAM-IMPORT");
       item.quoted=true;item.module=token.text.substr(1,token.text.size()-2);++pos;
      }else{
       item.module=module_name();
       if(item.module.size()>4096)
        throw Error(item.where,"program import name exceeds 4096 bytes","ON1X-PROGRAM-IMPORT");
      }
      if(accept("as"))item.alias=ident();
      colon_import_selectors(item);
      consume(";");
      u.module_imports.push_back(std::move(item));check_import_count();return true;
     }
    if(at("import")||at("from")){
      Import declaration;declaration.where=peek().where;
      if(accept("from")){
       while(at(".")||at("..")||at("...")){declaration.relative+=peek().text.size();++pos;}
      if(!at("import"))declaration.module=module_name();
      if(declaration.module.empty()&&!declaration.relative)throw Error(peek().where,"expected module name");
       consume("import");
       import_selectors(declaration,accept("("));
      u.module_imports.push_back(std::move(declaration));
     }else{
      consume("import");
      if(!peek().text.empty()&&peek().text.front()=='"'){
       auto token=peek();
       if(token.text.find('\\')!=std::string::npos)throw Error(token.where,"import paths cannot contain string escapes");
       if(token.text.size()<=2||token.text.size()>4098)throw Error(token.where,"import path must be 1..4096 bytes");
       declaration.quoted=true;declaration.module=token.text.substr(1,token.text.size()-2);
       u.imports.push_back(declaration.module);u.module_imports.push_back(std::move(declaration));++pos;
      }else do{
       Import item;item.where=peek().where;item.module=module_name();if(accept("as"))item.alias=ident();
       const bool selective=colon_import_selectors(item);
       u.module_imports.push_back(std::move(item));
       if(selective)break;
      }while(accept(","));
     }
     check_import_count();
      consume(";");return true;
    }
     return false;
    }
    bool operator_declaration_start()const{return at("operator")&&(peek(1).text=="("||peek(1).text=="<");}
    bool block_operator_start()const{
     if(!operator_declaration_start())return false;
     const auto attributes_at=[&](std::size_t i){
      return peek(i).text=="("&&is_ident(peek(i+1).text)&&peek(i+2).text==":";
     };
     if(peek(1).text=="(")return attributes_at(1);
     // `operator` is contextual: `operator < value` might be a comparison.
     // Commit only when the bounded generic header leads to named attributes.
     std::size_t angles=1,nested=0;
     for(std::size_t i=2;i<max_syntax_depth;++i){
      const auto& text=peek(i).text;
      if(text=="<eof>"||text==";"||text=="="||text=="=>")return false;
      if(text=="("||text=="["||text=="{")++nested;
      else if(text==")"||text=="]"||text=="}"){if(!nested)return false;--nested;}
      else if(!nested&&text=="<")++angles;
      else if(!nested){
       const auto closes=angle_closes(text);
       if(closes>=angles)return closes==angles&&closes==text.size()&&attributes_at(i+1);
       angles-=closes;
      }
     }
     return false;
    }
     void parse_operator_declaration(std::vector<Stmt>* lexical_body=nullptr, VisibilityRule visibility={}){
      if(!operator_count||*operator_count>=512)
       throw Error(peek().where,"operator declaration limit exceeded","ON1X-OPERATOR-LIMIT");
      ++*operator_count;
      auto enclosing_types=std::move(bound_types);
      auto declaration=operator_declaration_header();
      declaration.header.visibility=visibility;
      declaration.block_local=lexical_body!=nullptr;
      if(lexical_body){
      const auto& header=declaration.header;
      const bool prefix=header.position==OperatorPosition::Prefix;
      auto& slots=local_operators[header.lexeme];
      auto& slot=prefix?slots.prefix:slots.trailing;
      if(!slot.empty()&&slot.back().depth==lexical_depth)
       throw Error(header.where,"conflicting local operator '"+header.lexeme+"' in the same block","ON1X-OPERATOR-CONFLICT");
      slot.push_back({{header,declaration.identity},lexical_depth});
      local_operator_undo.emplace_back(header.lexeme,prefix);
     }
      if(lexical_body&&(templating||structural_operator_syntax)){
       Stmt syntax;syntax.kind=Stmt::Kind::Block;syntax.where=declaration.header.where;
       declaration.function.body=block();accept(";");
       syntax.syntax_operator=std::make_unique<OperatorDeclaration>(std::move(declaration));
       lexical_body->push_back(std::move(syntax));
      }else{
       // Ordinary declarations are noncapturing named callables with a
       // lexical syntax binding. Preserve source order when hoisting them.
       const auto index=declarations->size();
       declarations->push_back(std::move(declaration));
       auto body=block();accept(";");
       (*declarations)[index].function.body=std::move(body);
      }
     bound_types=std::move(enclosing_types);
    }
    static std::string_view group_close(std::string_view text){
     if(text=="(")return ")";
     if(text=="[")return "]";
     if(text=="{")return "}";
     return {};
    }
    // This pass never reads an expression or statement. Skip whole groups
    // in linear token order; malformed unrelated syntax is diagnosed by the
    // full parser, which has the exact grammar context for the delimiter.
    bool skip_discovery_group(){
     std::vector<std::string_view> ends{group_close(peek().text)};++pos;
     while(!ends.empty()&&!at("<eof>")){
      auto close=group_close(peek().text);
      if(!close.empty()){
       if(ends.size()>=max_syntax_depth)break;
       ends.push_back(close);
      }else if(at(")")||at("]")||at("}")){
       if(!at(ends.back()))break;
       ends.pop_back();
      }
      ++pos;
     }
     if(ends.empty())return true;
     pos=ts.size()-1;return false;
    }
    void skip_discovery_item(){
     const auto start=pos;
     while(!at("<eof>")){
      if(accept(";"))return;
      auto group=group_close(peek().text);
      if(group.empty()){++pos;continue;}
      // A record/sum result type or constraint can expose a brace at the
      // declaration's outer level, before the actual declaration body.
      const bool type_brace=pos>start+1&&(ts[pos-1].text=="record"||ts[pos-1].text=="sum")&&
                            (ts[pos-2].text=="->"||ts[pos-2].text=="<"||ts[pos-2].text==",");
      if(!skip_discovery_group()||(group=="}"&&!type_brace))return;
     }
    }
    OperatorDiscovery discover(){
     Unit imports;OperatorDiscovery result;
     result.module=module_header();
     while(!at("<eof>")){
      if(accept(";"))continue;
      if(at("module"))
       throw Error(peek().where,"a module header must appear once, before all declarations","ON1X-MODULE-HEADER");
      const auto visibility=declaration_visibility(result.module.has_value());
      if(import_declaration(imports))continue;
      if(operator_declaration_start()){
       if(result.operators.size()>=512)throw Error(peek().where,"operator declaration limit exceeded","ON1X-OPERATOR-LIMIT");
       auto declaration=operator_declaration_header();bound_types.clear();
       declaration.header.visibility=visibility;
       result.operators.push_back(std::move(declaration.header));
       if(!at("{"))consume("{");
       if(!skip_discovery_group())break;
       continue;
      }
      if(at("@")&&(peek(1).text=="enable"||peek(1).text=="scrutiny"||peek(1).text=="ppx")){
       const std::size_t count=peek(1).text=="ppx"?4u:3u;
       pos=std::min(pos+count,ts.size()-1);accept(";");continue;
      }
      skip_discovery_item();
     }
     result.imports=std::move(imports.module_imports);return result;
    }
      Unit parse(std::vector<Docstring> docs,bool declaration_interfaces=false){Unit u;
       std::size_t operator_total=0;operator_count=&operator_total;
        u.docstrings=std::move(docs);docstrings=&u.docstrings;declarations=&u.operators;directives=&u.fixity_directives;
       u.module=module_header();
       while(!at("<eof>")){
        if(at("module"))
         throw Error(peek().where,"a module header must appear once, before all declarations","ON1X-MODULE-HEADER");
        if(u.module&&at("type")&&is_ident(peek(1).text)&&peek(2).text=="="){
         auto where=peek().where;consume("type");auto name=ident();consume("=");
         if(!declaration_interfaces)
          throw Error(where,"module type equations are declaration-only","ON1X-INTERFACE-UNSUPPORTED");
         static const std::set<std::string> reserved_types={
          "_","i64","f64","f32","u8","u16","u32","u64","bool","string","char","void",
          "json","json_view","sexpr","buffer","code","own","xfiber"};
         if(reserved_types.contains(name))
          throw Error(where,"built-in type cannot be redefined: "+name,"ON1X-INTERFACE-TYPE");
         if(u.signature_type_aliases.size()>=256)
          throw Error(where,"module type equation limit exceeded","ON1X-INTERFACE-LIMIT");
         u.signature_type_aliases.push_back({std::move(where),std::move(name),type()});
         consume(";");continue;
        }
       const auto visibility=declaration_visibility(u.module.has_value());
       if(operator_declaration_start()){
        parse_operator_declaration(nullptr,visibility);continue;
       }
        if(fixity_header(true)){fixity_directive();continue;}
       if(at("cliche")&&is_ident(peek(1).text)){
        if(u.cliches.size()>=256)throw Error(peek().where,"cliche declaration limit exceeded","ON1X-CLICHE-LIMIT");
        u.cliches.push_back(cliche_declaration());u.cliches.back().visibility=visibility;continue;
       }
        if(at("impl")&&!cliche_implementation_header()&&(peek(1).text=="<"||is_ident(peek(1).text))){
         if(u.inherent_implementations.size()>=1024)
          throw Error(peek().where,"inherent implementation limit exceeded","ON1X-IMPL-LIMIT");
         u.inherent_implementations.push_back(inherent_implementation(u.inherent_implementations.size()));continue;
        }
        if(cliche_implementation_header()){
         if(u.cliche_implementations.size()>=1024)throw Error(peek().where,"cliche implementation limit exceeded","ON1X-CLICHE-LIMIT");
         u.cliche_implementations.push_back(cliche_implementation());continue;
        }
       if((at("record")||at("enum"))&&is_ident(peek(1).text)){
       if(u.nominals.size()>=512)throw Error(peek().where,"nominal declaration limit exceeded");
       u.nominals.push_back(nominal_declaration());u.nominals.back().visibility=visibility;continue;
      }
        if(at("@")&&peek(1).text=="scrutiny"){
         auto where=peek().where;pos+=2;auto level=ident();
         consume(";");u.scrutiny.push_back({where,std::move(level)});continue;
        }
       if(at("@")&&peek(1).text=="enable"){
       auto where=peek().where;pos+=2;auto token=peek();auto name=extensions::canonical(ident(),where);
       if(used_extensions.contains(name))throw Error(where,"ON1X-CAT-ENABLE: late @enable "+name,"ON1X-CAT-ENABLE");
       auto end=token.where;end.offset+=token.text.size();end.column+=token.text.size();
       if(at(";")){end=peek().where;++end.offset;++end.column;++pos;}enabled_extensions.insert(name);
       if(std::none_of(u.features.begin(),u.features.end(),[&](const auto& f){return f.name==name;}))u.features.push_back({where,end,name});
       continue;
      }
      if(at("@")&&peek(1).text=="extension"){
       ModuleExtension carrier;carrier.where=peek().where;pos+=2;carrier.owner=extensions::canonical(ident(),carrier.where);
       if(!enabled_extensions.contains(carrier.owner))throw Error(carrier.where,"ON1X-CAT-ENABLE: @extension requires @enable "+carrier.owner,"ON1X-CAT-ENABLE");
       used_extensions.insert(carrier.owner);consume("{");std::vector<std::string> closing{"}"};
       while(!closing.empty()){
        auto t=peek();if(t.text=="<eof>")throw Error(t.where,"ON1X-CAT-SYNTAX: unterminated extension carrier","ON1X-CAT-SYNTAX");
        if(t.text=="{"||t.text=="("||t.text=="["){
         if(closing.size()>=max_syntax_depth)throw Error(t.where,"ON1X-CAT-SYNTAX: carrier depth budget exhausted","ON1X-CAT-SYNTAX");
         closing.push_back(t.text=="{"?"}":t.text=="("?")":"]");
        }else if(t.text=="}"||t.text==")"||t.text=="]"){
         if(t.text!=closing.back())throw Error(t.where,"ON1X-CAT-SYNTAX: mismatched carrier delimiter","ON1X-CAT-SYNTAX");
         closing.pop_back();if(closing.empty()){carrier.end=t.where;++carrier.end.offset;++carrier.end.column;++pos;break;}
        }
        if(carrier.tokens.size()>=16384)throw Error(t.where,"ON1X-CAT-SYNTAX: carrier token budget exhausted","ON1X-CAT-SYNTAX");
        carrier.tokens.push_back(t);++pos;
       }
       if(u.extensions.size()>=32)throw Error(carrier.where,"ON1X-CAT-SYNTAX: carrier count budget exhausted","ON1X-CAT-SYNTAX");
       u.extensions.push_back(std::move(carrier));accept(";");continue;
      }
     if(accept("macro")){
      u.macros.push_back(macro_definition());u.macros.back().visibility=visibility;
      if(u.macros.size()>512)throw Error(peek().where,"macro definition limit exceeded");
      continue;
     }
     if(at("@")&&peek(1).text=="ppx"){
      auto where=peek().where;consume("@");consume("ppx");auto label=ident();auto token=peek();
      if(label.size()>128)throw Error(where,"PPX label exceeds 128 bytes");
      if(token.text.size()<2||token.text.front()!='"'||token.text.find('\\')!=std::string::npos)throw Error(token.where,"PPX registration expects an unescaped quoted path");
      if(token.text.size()<=2||token.text.size()>4098)throw Error(token.where,"PPX program path must be 1..4096 bytes");
      if(u.ppx.size()>=32)throw Error(where,"PPX program count limit exceeded");
      ++pos;u.ppx.push_back({where,std::move(label),token.text.substr(1,token.text.size()-2)});accept(";");continue;
     }
     if(import_declaration(u))continue;
     Function f;f.where=peek().where;f.visibility=visibility;
     while(accept("@"))f.attributes.push_back(extension(ts[pos-1].where).node);
     f.external=accept("extern");f.procedure=!f.external&&accept("proc");
     if(!f.procedure)consume("fn");
     f.name=ident();bound_types.clear();
     f.type_parameters=generic_parameters(f.constraints);
     if(f.external&&!f.type_parameters.empty())
      throw Error(f.where,"extern functions cannot be generic","ON1X-CLICHE-UNSUPPORTED");
     consume("(");
     if(!at(")"))do{f.params.push_back(parameter(f.params.size(),!f.external));}while(accept(","));
    consume(")");if(accept("->"))f.result=type();
    where_constraints(f.constraints);
    if(f.external)consume(";");else f.body=block();
    bound_types.clear();
    u.functions.push_back(std::move(f));
   if(u.functions.size()>2048)throw Error(peek().where,"too many functions");
  }
    // Empty files are valid package initializers; the checker still requires main.
   std::sort(u.docstrings.begin(),u.docstrings.end(),[](const Docstring& a,const Docstring& b){
    return a.where.offset<b.where.offset;
   });
   if(operator_total)u.operator_entry=ts.front().where.file;
  return u;
 }
 };
 }
 OperatorDiscovery discover_operators(const std::vector<Token>& ts){
  return ts.empty()?OperatorDiscovery{}:Parser(ts,{}).discover();
 }
 Unit parse_with_operators(const std::vector<Token>& ts,const std::vector<std::string>& enabled,
                           std::vector<Docstring> docs,OperatorScope scope,bool declaration_interfaces){
  return ts.empty()?Unit{}:Parser(ts,enabled,&scope).parse(std::move(docs),declaration_interfaces);
 }
 Unit parse(const std::vector<Token>&ts,const std::vector<std::string>& enabled,std::vector<Docstring> docs){
  if(ts.empty())return {};
  OperatorScope scope;
  for(const auto& header:discover_operators(ts).operators)
   scope.add({header,operator_identity(header)},header.where);
  auto unit=Parser(ts,enabled,&scope).parse(std::move(docs));
   if(!unit.operator_entry.empty()||!unit.operators.empty()){
   const auto& key=ts.front().where.file;unit.operator_entry=key;
   record_operator_scope(unit,scope,key);
   for(const auto& function:unit.functions)unit.operator_owners.emplace(function.name,key);
   for(const auto& declaration:unit.operators)unit.operator_owners.emplace(declaration.identity,key);
  }
  return unit;
 }
std::vector<InterfaceMember> parse_interface(const std::vector<Token>& ts){return Parser(ts,{}).interface();}
ModuleSignature parse_module_signature(const std::vector<Token>& ts){return Parser(ts,{}).module_signature();}
void require_no_kernel_imports(const Unit& unit){
 if(unit.kernel_imports.empty()){
  if(!unit.foreign_imports.empty())
   throw Error(unit.foreign_imports.front().where,
               "portable stdffi imports require source-module loading","ON1X-STDFFI-UNRESOLVED");
  return;
 }
 const auto& request=unit.kernel_imports.front();
 const char* code=request.is_unsafe?"ON1X-KERNEL-UNSAFE-UNSUPPORTED":"ON1X-KERNEL-NOT-ADMITTED";
 throw Error(request.where,std::string(code)+": "+request.module+
  (request.is_unsafe?" has no ABI-verified unsafe import support":" has no independently admitted KernelBind interface")+
  "; use --kernel-imports to inspect source requests",code);
}
}
