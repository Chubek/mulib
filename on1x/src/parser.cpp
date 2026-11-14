#include <on1x/ast.hpp>
#include <charconv>
#include <cctype>
#include <algorithm>
#include <set>
namespace on1x {
namespace {
class Parser {
 const std::vector<Token>& ts;
 std::size_t pos=0;
 std::size_t nesting=0;
 std::size_t lambdas=0;
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
 const Token& peek(std::size_t offset=0)const{return ts.at(std::min(pos+offset,ts.size()-1));}
 bool at(std::string_view s)const{return peek().text==s;}
 bool accept(std::string_view s){if(at(s)){++pos;return true;}return false;}
 const Token& consume(std::string_view s){if(!at(s))throw Error(peek().where,"expected '"+std::string(s)+"', found '"+peek().text+"'");return ts[pos++];}
 static bool is_ident(const std::string& s){if(s.empty()||!(std::isalpha((unsigned char)s[0])||s[0]=='_'))return false;
  for(char c:s)if(!(std::isalnum((unsigned char)c)||c=='_'))return false;
  return true;}
 std::string ident(){auto t=peek();if(!is_ident(t.text))throw Error(t.where,"expected identifier");++pos;
  static const std::set<std::string> reserved={"fn","let","var","if","else","while","return","true","false","extern","import"};
  if(reserved.count(t.text))throw Error(t.where,"reserved keyword used as identifier: "+t.text);
  return t.text;}
 Type type(){Nesting guard(*this);auto t=peek();
  if(accept("fn")){
   consume("(");std::vector<Type> parameters;
   if(!at(")"))do{parameters.push_back(type());}while(accept(","));
   consume(")");consume("->");auto result=type();
   return Type::function(std::move(parameters),std::move(result));
  }
   ++pos;
   if(t.text=="_")return Type::Hole;
  for(auto x:{Type::I64,Type::Bool,Type::String,Type::Void,Type::Json,Type::JsonView})if(t.text==type_name(x))return x;
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
   if(accept("fn")){
    if(++lambdas>2048)throw Error(t.where,"too many anonymous functions");
    auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Lambda;e->where=t.where;
    consume("(");
    if(!at(")"))do{
     Param p;p.where=peek().where;p.name=ident();p.type=accept(":")?type():Type(Type::Hole);
     e->parameters.push_back(std::move(p));
    }while(accept(","));
    consume(")");if(accept("->"))e->result=type();e->body=block();
    auto depth=body_expression_depth(e->body)+1;return bounded(std::move(e),depth);
   }
  if(accept("(")){auto e=expr();consume(")");return e;}
  if(accept("true")||accept("false")){auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Boolean;e->where=t.where;e->text=t.text;return {std::move(e)};}
  if(!t.text.empty()&&t.text[0]=='"'){
   ++pos;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::String;e->where=t.where;e->text=t.text;return {std::move(e)};
  }
  if(!t.text.empty()&&std::isdigit((unsigned char)t.text[0])){
   ++pos;auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Integer;e->where=t.where;e->text=t.text;return {std::move(e)};
  }
  std::string name=ident();
  auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Variable;e->where=t.where;e->text=name;
  return {std::move(e)};
 }
 Expression primary(){
  Nesting guard(*this);
  const Token t=peek();
  if(accept("!")||accept("-")){
   if(t.text=="-" && peek().text=="9223372036854775808" && peek(1).text!="(") {
    ++pos;auto literal=std::make_unique<Expr>();literal->kind=Expr::Kind::Integer;
    literal->where=t.where;literal->text="-9223372036854775808";return {std::move(literal)};
   }
   auto child=primary();
   auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Unary;e->where=t.where;e->text=t.text;
   e->children.push_back(std::move(child.node));return bounded(std::move(e),child.depth+1);
  }
  auto value=atom();
  while(accept("(")){
   auto call=std::make_unique<Expr>();call->kind=Expr::Kind::Call;call->where=value.node->where;
   call->callee=std::move(value.node);auto depth=value.depth+1;
   if(!at(")"))do{
    auto argument=expr();depth=std::max(depth,argument.depth+1);
    call->children.push_back(std::move(argument.node));
   }while(accept(","));
   consume(")");value=bounded(std::move(call),depth);
  }
  return value;
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
 Stmt statement(){Stmt s; s.where=peek().where;
  if(accept("let")||accept("var")){
   s.kind=Stmt::Kind::Let;s.mutable_binding=ts[pos-1].text=="var";
   s.name=ident();if(accept(":"))s.declared=type();consume("=");s.value=expr().node;consume(";");return s;
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
   if(accept("import")){
    auto token=peek();
    if(token.text.size()<2||token.text.front()!='"'||token.text.back()!='"')
      throw Error(token.where,"import expects a quoted path");
    if(token.text.find('\\')!=std::string::npos)
      throw Error(token.where,"import paths cannot contain string escapes");
    u.imports.push_back(token.text.substr(1,token.text.size()-2));
    ++pos;consume(";");continue;
   }
   Function f;f.where=peek().where;f.external=accept("extern");consume("fn");f.name=ident();consume("(");
   if(!at(")"))do{Param p;p.where=peek().where;p.name=ident();p.type=accept(":")?type():Type(Type::Hole);f.params.push_back(std::move(p));}while(accept(","));
   consume(")");if(accept("->"))f.result=type();
   if(f.external)consume(";");else f.body=block();
   u.functions.push_back(std::move(f));
   if(u.functions.size()>2048)throw Error(peek().where,"too many functions");
  }
  if(u.functions.empty()&&u.imports.empty())throw Error(peek().where,"empty module");
  return u;
 }
};
}
Unit parse(const std::vector<Token>&ts){return Parser(ts).parse();}
}
