#include <on1x/ast.hpp>
#include <charconv>
#include <cctype>
#include <algorithm>
#include <set>
#include <map>
namespace on1x {
namespace {
class Parser {
 const std::vector<Token>& ts;
  std::size_t pos=0;
  std::optional<std::size_t> split_angle;
  Token assignment_token;
 std::size_t nesting=0;
 std::size_t lambdas=0;
 std::map<std::string,std::size_t> bound_types;
 bool templating=false;
 bool syntax_atoms=false,literal_ellipsis=false;
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
 const Token& peek(std::size_t offset=0)const{if(!offset&&split_angle==pos)return assignment_token;return ts.at(std::min(pos+offset,ts.size()-1));}
 bool at(std::string_view s)const{return peek().text==s;}
 bool accept(std::string_view s){if(at(s)){++pos;return true;}return false;}
 const Token& consume(std::string_view s){if(!at(s))throw Error(peek().where,"expected '"+std::string(s)+"', found '"+peek().text+"'");const auto& token=peek();++pos;return token;}
 static bool is_ident(const std::string& s){if(s.empty()||!(std::isalpha((unsigned char)s[0])||s[0]=='_'))return false;
  for(char c:s)if(!(std::isalnum((unsigned char)c)||c=='_'))return false;
  return true;}
  std::string ident(){if(templating&&accept("$"))return "$"+ident();
   auto t=peek();if(!is_ident(t.text))throw Error(t.where,"expected identifier");++pos;
    static const std::set<std::string> reserved={"fn","let","var","if","else","while","return","true","false","extern","import","forall","macro","rule","quote","splice","lift"};
  if(reserved.count(t.text))throw Error(t.where,"reserved keyword used as identifier: "+t.text);
  return t.text;}
    Type type(){Nesting guard(*this);auto t=peek();
    if(accept("code")){consume("<");auto body=type();
     if(at(">=")){assignment_token=peek();assignment_token.text="=";++assignment_token.where.offset;++assignment_token.where.column;split_angle=pos;}
     else consume(">");
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
    if(!accept(",")){consume(")");return first;}
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
    ++pos;
    if(auto found=bound_types.find(t.text);found!=bound_types.end())return Type::bound(found->second);
   if(t.text=="_")return Type::Hole;
   for(auto x:{Type::I64,Type::Bool,Type::String,Type::Void,Type::Json,Type::JsonView,Type::SExpr,Type::Buffer})if(t.text==type_name(x))return x;
  throw Error(t.where,"unknown type: "+t.text);
 }
 static int precedence(const std::string& op){if(op=="||")return 1;if(op=="&&")return 2;
  if(op=="=="||op=="!=")return 3;
  if(op=="<"||op==">"||op=="<="||op==">=")return 4;
  if(op=="+"||op=="-")return 5;
  if(op=="*"||op=="/"||op=="%")return 6;
  return -1;}
   Expression atom(){
    const Token t=peek();
    if(accept("quote")){
     consume("{");auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Quote;e->where=t.where;e->children.push_back(expr().node);consume("}");return syntax_bounded(std::move(e));
    }
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
       (at("...")||at("else")||at("if")||at("let")||at("var")||at("return")||at("while")||at("extern")||at("import")||at("macro")||at("rule")||at("forall")||(at("fn")&&peek(1).text!="("))){
     ++pos;return syntax_token(t);
    }
    if(templating&&accept("$")){
     auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SyntaxHole;e->where=t.where;e->text="$"+ident();return {std::move(e)};
    }
    if(accept("@"))return extension(t.where);
   if(accept("fn")){
    if(++lambdas>2048)throw Error(t.where,"too many anonymous functions");
    auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Lambda;e->where=t.where;
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
    if(!accept(",")){consume(")");return first;}
    auto tuple=std::make_unique<Expr>();tuple->kind=Expr::Kind::Tuple;tuple->where=t.where;
    auto depth=first.depth+1;tuple->children.push_back(std::move(first.node));
    do{auto element=expr();depth=std::max(depth,element.depth+1);tuple->children.push_back(std::move(element.node));}while(accept(","));
    consume(")");return bounded(std::move(tuple),depth);
   }
  if(accept("true")||accept("false")){auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Boolean;e->where=t.where;e->text=t.text;return {std::move(e)};}
  if(!t.text.empty()&&t.text[0]=='"'){
   ++pos;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::String;e->where=t.where;e->text=t.text;return {std::move(e)};
  }
  if(!t.text.empty()&&std::isdigit((unsigned char)t.text[0])){
   ++pos;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Integer;e->where=t.where;e->text=t.text;return {std::move(e)};
  }
   std::string name=ident();
   auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Variable;e->where=t.where;e->text=name;
    if(accept("!")){
     e->kind=Expr::Kind::MacroCall;consume("(");
     syntax_sequence(*e,")");return syntax_bounded(std::move(e));
   }
  return {std::move(e)};
 }
 Expression primary(){
  Nesting guard(*this);
  const Token t=peek();
  if(accept("!")||accept("-")){
   if(t.text=="-" && (peek().text=="9223372036854775808"||peek().text=="0x8000000000000000"||peek().text=="0X8000000000000000") && peek(1).text!="(") {
    ++pos;auto literal=std::make_unique<Expr>();literal->kind=Expr::Kind::Integer;
    literal->where=t.where;literal->text="-9223372036854775808";return {std::move(literal)};
   }
   auto child=primary();
   auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Unary;e->where=t.where;e->text=t.text;
   e->children.push_back(std::move(child.node));return bounded(std::move(e),child.depth+1);
  }
   auto value=atom();
   while(true){
    if(templating&&!literal_ellipsis&&accept(ellipsis)){if(++value.node->syntax_repeat>256)throw Error(value.node->where,"template ellipsis depth limit exceeded");continue;}
    if(!accept("("))break;
   auto call=std::make_unique<Expr>();call->kind=Expr::Kind::Call;call->where=value.node->where;
   call->callee=std::move(value.node);auto depth=value.depth+1;
   if(!at(")")&&!(templating&&at(".")))do{
      auto argument=expr();depth=std::max(depth,argument.depth+1);
      call->children.push_back(std::move(argument.node));
     }while(accept(",")&&!(templating&&at(".")));
    if(templating&&accept(".")){call->syntax_tail=expr().node;depth=std::max(depth,call->syntax_tail->syntax_depth+1);}
   consume(")");value=bounded(std::move(call),depth);
  }
   return value;
  }
  ExprPtr syntax_argument(){
   if(!at("{")){bool saved=syntax_atoms;syntax_atoms=true;auto e=expr().node;syntax_atoms=saved;return e;}
   auto e=std::make_unique<Expr>();e->kind=Expr::Kind::SyntaxBlock;e->where=peek().where;e->body=block();
   auto depth=body_expression_depth(e->body)+1;return bounded(std::move(e),depth).node;
  }
  Expression syntax_token(const Token& token){
   auto e=std::make_unique<Expr>();e->where=token.where;e->text=token.text;
   static const std::set<std::string> keywords={"fn","let","var","if","else","while","return","extern","import","forall","macro","rule","quote","splice","lift","..."};
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
   if(accept("$")){p.name="$"+ident();consume(":");p.category=ident();}
   else if(at("_")&&std::find(macro.literals.begin(),macro.literals.end(),"_")==macro.literals.end()){
    ++pos;p.kind=MacroParameter::Kind::Wildcard;if(accept(":"))p.category=ident();
   }else if(at("[")||at("(")||(at("vector")&&peek(1).text=="[")||
             ((at("ast_call")||at("ast_unary")||at("ast_binary"))&&peek(1).text=="(")){
    auto form=peek().text;++pos;std::string end;
    if(form=="vector"){consume("[");end="]";p.kind=MacroParameter::Kind::Vector;}
    else if(form=="["){end="]";p.kind=MacroParameter::Kind::List;}
    else if(form=="("){end=")";p.kind=MacroParameter::Kind::Tuple;}
    else{consume("(");end=")";p.kind=form=="ast_call"?MacroParameter::Kind::Call:form=="ast_unary"?MacroParameter::Kind::Unary:MacroParameter::Kind::Binary;}
    macro_sequence(p.elements,p.tail,end,macro);
    if(p.kind==MacroParameter::Kind::Vector&&p.tail)throw Error(p.where,"vector patterns cannot have dotted tails");
   }else{
    bool saved=syntax_atoms;syntax_atoms=true;p.kind=MacroParameter::Kind::Literal;
    if(accept("-")){auto value=atom();if(value.node->kind!=Expr::Kind::Integer)throw Error(p.where,"negative pattern datum requires an integer");value.node->text="-"+value.node->text;p.literal=std::move(value.node);}
    else p.literal=atom().node;
    syntax_atoms=saved;
    if(p.literal->kind!=Expr::Kind::Variable&&p.literal->kind!=Expr::Kind::String&&p.literal->kind!=Expr::Kind::Integer&&p.literal->kind!=Expr::Kind::Boolean&&p.literal->kind!=Expr::Kind::SyntaxKeyword)
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
 Expression expr(int minprec=1){auto lhs=primary();
  while(true){std::string op=peek().text;int prec=precedence(op);if(prec<minprec)break;
   auto where=peek().where;++pos;auto rhs=expr(prec+1);
   auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Binary;e->where=where;e->text=op;
   auto depth=std::max(lhs.depth,rhs.depth)+1;
   e->children.push_back(std::move(lhs.node));e->children.push_back(std::move(rhs.node));lhs=bounded(std::move(e),depth);
  }return lhs;
 }
  std::vector<Stmt> block(){Nesting guard(*this);consume("{");std::vector<Stmt> result;
  while(!at("}")){if(at("<eof>"))throw Error(peek().where,"unterminated block");
   result.push_back(statement());if(result.size()>100000)throw Error(peek().where,"too many statements");}
   consume("}");return result;
  }
  Pattern pattern(){
   Nesting guard(*this);Pattern result;result.where=peek().where;
   if(accept("(")){
    result.elements.push_back(pattern());consume(",");
    do{result.elements.push_back(pattern());}while(accept(","));
    consume(")");
   }else result.name=ident();
   return result;
  }
  Param parameter(std::size_t index){
   Param result;result.where=peek().where;
   if(at("(")){result.pattern=pattern();result.name="@argument_"+std::to_string(index);}
   else result.name=ident();
   result.type=accept(":")?type():Type(Type::Hole);return result;
  }
 Stmt statement(){Stmt s; s.where=peek().where;
  if(accept("let")||accept("var")){
   s.kind=Stmt::Kind::Let;s.mutable_binding=ts[pos-1].text=="var";
    if(at("("))s.pattern=pattern();else s.name=ident();
    if(accept(":"))s.declared=type();
    consume("=");s.value=expr().node;consume(";");return s;
  }
   if(accept("return")){s.kind=Stmt::Kind::Return;if(!at(";"))s.value=expr().node;consume(";");return s;}
   if(accept("if")){s.kind=Stmt::Kind::If;s.value=expr().node;s.body=block();if(accept("else"))s.otherwise=block();return s;}
   if(accept("while")){s.kind=Stmt::Kind::While;s.value=expr().node;s.body=block();return s;}
  if(at("{")){s.kind=Stmt::Kind::Block;s.body=block();return s;}
  if(peek(1).text=="="){
    s.kind=Stmt::Kind::Assign;s.name=ident();consume("=");s.value=expr().node;consume(";");return s;
  }
   s.kind=Stmt::Kind::Expr;s.value=expr().node;consume(";");return s;
 }
 public:
 explicit Parser(const std::vector<Token>&t):ts(t){}
 Unit parse(){Unit u;
   while(!at("<eof>")){
    if(accept("macro")){u.macros.push_back(macro_definition());if(u.macros.size()>512)throw Error(peek().where,"macro definition limit exceeded");continue;}
    if(at("@")&&peek(1).text=="ppx"){
     auto where=peek().where;consume("@");consume("ppx");auto label=ident();auto token=peek();
     if(label.size()>128)throw Error(where,"PPX label exceeds 128 bytes");
     if(token.text.size()<2||token.text.front()!='"'||token.text.find('\\')!=std::string::npos)throw Error(token.where,"PPX registration expects an unescaped quoted path");
     if(token.text.size()<=2||token.text.size()>4098)throw Error(token.where,"PPX program path must be 1..4096 bytes");
     if(u.ppx.size()>=32)throw Error(where,"PPX program count limit exceeded");
     ++pos;u.ppx.push_back({where,std::move(label),token.text.substr(1,token.text.size()-2)});accept(";");continue;
    }
   if(accept("import")){
    auto token=peek();
    if(token.text.size()<2||token.text.front()!='"'||token.text.back()!='"')
      throw Error(token.where,"import expects a quoted path");
    if(token.text.find('\\')!=std::string::npos)
      throw Error(token.where,"import paths cannot contain string escapes");
    u.imports.push_back(token.text.substr(1,token.text.size()-2));
    ++pos;consume(";");continue;
   }
    Function f;f.where=peek().where;
    while(accept("@"))f.attributes.push_back(extension(ts[pos-1].where).node);
    f.external=accept("extern");consume("fn");f.name=ident();consume("(");
    if(!at(")"))do{f.params.push_back(parameter(f.params.size()));}while(accept(","));
   consume(")");if(accept("->"))f.result=type();
   if(f.external)consume(";");else f.body=block();
   u.functions.push_back(std::move(f));
   if(u.functions.size()>2048)throw Error(peek().where,"too many functions");
  }
   if(u.functions.empty()&&u.imports.empty()&&u.macros.empty())throw Error(peek().where,"empty module");
  return u;
 }
};
}
Unit parse(const std::vector<Token>&ts){return Parser(ts).parse();}
}
