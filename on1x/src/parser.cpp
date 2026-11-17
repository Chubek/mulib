#include <on1x/ast.hpp>
#include <on1x/numeric.hpp>
#include <on1x/extensions.hpp>
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
  std::set<std::string> enabled_extensions,used_extensions;
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
  const Token& consume(std::string_view s){if(!at(s))throw Error(peek().where,"expected '"+std::string(s)+"', found '"+peek().text+"'","ON1X-PARSE-EXPECTED",std::string(s),peek().text);const auto& token=peek();++pos;return token;}
 static bool is_ident(const std::string& s){if(s.empty()||!(std::isalpha((unsigned char)s[0])||s[0]=='_'))return false;
  for(char c:s)if(!(std::isalnum((unsigned char)c)||c=='_'))return false;
  return true;}
  std::string ident(){if(templating&&accept("$"))return "$"+ident();
   auto t=peek();if(!is_ident(t.text))throw Error(t.where,"expected identifier");++pos;
    static const std::set<std::string> reserved={"fn","let","var","if","else","while","return","true","false","extern","import","forall","macro","rule","quote","splice","lift","let_syntax","letrec_syntax"};
  if(reserved.count(t.text))throw Error(t.where,"reserved keyword used as identifier: "+t.text);
  return t.text;}
     Type type(){Nesting guard(*this);auto t=peek();
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
     if(at(">=")){assignment_token=peek();assignment_token.text="=";++assignment_token.where.offset;++assignment_token.where.column;split_angle=pos;}else consume(">");
     return Type::data(slice?Type::Slice:Type::Array,{std::move(element)},{},extent);
    }
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
     for(auto x:{Type::I64,Type::F64,Type::F32,Type::U8,Type::U16,Type::U32,Type::U64,Type::Bool,Type::String,Type::Void,Type::Json,Type::JsonView,Type::SExpr,Type::Buffer})if(t.text==type_name(x))return x;
   throw Error(t.where,"unknown type: "+t.text,"ON1X-PARSE-TYPE",{},t.text);
 }
  static int precedence(const std::string& op){if(op=="||")return 1;if(op=="&&")return 2;
  if(op=="=="||op=="!=")return 3;
  if(op=="<"||op==">"||op=="<="||op==">=")return 4;
  if(op=="+"||op=="-")return 5;
  if(op=="*"||op=="/"||op=="%")return 6;
   return -1;}
  bool match_header()const{
   if(!at("match"))return false;
   if(peek(1).text=="let")return true;
   if(precedence(peek(1).text)>=0&&peek(1).text!="-")return false;
   // Existing keyword-spelled values remain ordinary operands/callees unless
   // a complete discriminant is followed by the match delimiter.
   std::size_t parens=0,brackets=0,braces=0;
   for(std::size_t i=1;pos+i<ts.size();++i){const auto& text=peek(i).text;
    if(!parens&&!brackets&&!braces){
     if(text=="with"&&i>1&&precedence(peek(i-1).text)<0&&peek(i-1).text!="!"&&peek(i-1).text!=".")return true;
     if(text=="{"&&peek(1).text=="("&&i>1&&peek(i-1).text==")")return true;
     if(text==";"||text=="="||text==","||text==")"||text=="]"||text=="}"||text=="=>"||text=="<eof>")return false;
    }
    if(text=="(")++parens;else if(text==")"){if(!parens)return false;--parens;}
    if(text=="[")++brackets;else if(text=="]"){if(!brackets)return false;--brackets;}
    if(text=="{")++braces;else if(text=="}"){if(!braces)return false;--braces;}
   }
   return false;
  }
    Expression atom(){
      const Token t=peek();
     if(at("loop")&&peek(1).text=="{")throw Error(t.where,"ON1X-CONTROL-UNSUPPORTED: loop expressions are not implemented; use statement loop","ON1X-CONTROL-UNSUPPORTED");
    if(at("record")&&peek(1).text=="{"){
     ++pos;consume("{");auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Record;e->where=t.where;std::set<std::string> names;
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
    if(at("variant")&&peek(1).text=="<"&&peek(2).text=="sum"&&peek(3).text=="{"){
     ++pos;consume("<");auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Variant;e->where=t.where;e->result=type();consume(">");consume("(");e->text=ident();consume(",");e->children.push_back(expr().node);consume(")");return syntax_bounded(std::move(e));
    }
    if(match_header())return matching(false);
    if(at("let_syntax")||at("letrec_syntax"))return syntax_scope();
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
   ++pos;auto e=std::make_unique<Expr>();
   bool hexadecimal=t.text.starts_with("0x")||t.text.starts_with("0X");
    e->kind=(!hexadecimal&&numeric_literal_type(t.text).is_float())?Expr::Kind::Float:Expr::Kind::Integer;
   e->where=t.where;e->text=t.text;return {std::move(e)};
  }
    std::string name=ident();
    // A qualified module reference is lowered to a lexical global by the loader.
    // A spaced dot in macro syntax remains a dotted-list tail.
    while(at(".")&&peek().where.offset==ts[pos-1].where.offset+ts[pos-1].text.size()&&
          peek(1).where.offset==peek().where.offset+1){
     ++pos;name+="."+ident();
     if(name.size()>4096)throw Error(t.where,"qualified name exceeds 4096 bytes");
    }
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
    if(accept("[")){
     auto e=std::make_unique<Expr>();e->where=value.node->where;e->children.push_back(std::move(value.node));
     if(accept("]")){e->kind=Expr::Kind::Length;value=syntax_bounded(std::move(e));continue;}
     if(!peek().text.empty()&&peek().text.front()=='"'&&peek(1).text=="]"){
      auto field=peek();++pos;consume("]");e->kind=Expr::Kind::Field;e->text=field.text.substr(1,field.text.size()-2);
      if(!is_ident(e->text))throw Error(field.where,"record projection requires an identifier label");
     }else{
      e->children.push_back(expr().node);
      if(accept(":")){e->kind=Expr::Kind::Slice;e->children.push_back(expr().node);}else e->kind=Expr::Kind::Index;
      consume("]");
     }
     value=syntax_bounded(std::move(e));continue;
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
   static const std::set<std::string> keywords={"fn","let","var","if","else","while","return","extern","import","forall","macro","rule","quote","splice","lift","let_syntax","letrec_syntax","..."};
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
     if(p.literal->kind!=Expr::Kind::Variable&&p.literal->kind!=Expr::Kind::String&&p.literal->kind!=Expr::Kind::Integer&&p.literal->kind!=Expr::Kind::Float&&p.literal->kind!=Expr::Kind::Boolean&&p.literal->kind!=Expr::Kind::SyntaxKeyword)
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
   if(accept("expression")){consume("{");e->children.push_back(expr().node);consume("}");}
   else{consume("block");auto child=std::make_unique<Expr>();child->kind=Expr::Kind::SyntaxBlock;child->where=peek().where;child->body=block();child->syntax_depth=body_expression_depth(child->body)+1;e->children.push_back(std::move(child));}
   return syntax_bounded(std::move(e));
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
   if(accept("(")){result.kind=Pattern::Kind::Tuple;
    result.elements.push_back(pattern());consume(",");
    do{result.elements.push_back(pattern());}while(accept(","));
    consume(")");
   }else result.name=ident();
   return result;
  }
  Pattern match_pattern_atom(){
   Nesting guard(*this);Pattern p;p.where=peek().where;using P=Pattern::Kind;
   if(accept("_")){p.kind=P::Wildcard;return p;}
   if(accept("rest")){p.kind=P::Rest;return p;}
   if(accept("(")){
    p=match_pattern();if(!accept(",")){consume(")");return p;}
    Pattern tuple;tuple.kind=P::Tuple;tuple.where=p.where;tuple.elements.push_back(std::move(p));
    do{tuple.elements.push_back(match_pattern());if(tuple.elements.size()>256)throw Error(tuple.where,"pattern width limit exceeded");}while(accept(",")&&!at(")"));
    consume(")");return tuple;
   }
   if(accept("[")){
    p.kind=P::Array;
    if(!at("]")){do{p.elements.push_back(match_pattern());if(p.elements.size()>256)throw Error(p.where,"pattern width limit exceeded");
     if(at("::"))throw Error(peek().where,"ON1X-PATTERN: list-cons patterns require an owning list runtime","ON1X-PATTERN");
    }while(accept(",")&&!at("]"));}
    consume("]");return p;
   }
   if(at("record")&&peek(1).text=="{"){
    pos+=2;p.kind=P::Record;std::set<std::string> labels;
    if(!at("}")){do{if(accept("..")){p.open=true;break;}
     auto label=ident();if(!labels.insert(label).second)throw Error(p.where,"duplicate record pattern field: "+label);
     p.labels.push_back(label);consume(":");p.elements.push_back(match_pattern());
     if(p.elements.size()>256)throw Error(p.where,"pattern width limit exceeded");
    }while(accept(",")&&!at("}"));}
    consume("}");return p;
   }
   auto token=peek();bool negative=accept("-");if(negative)token=peek();
   if(token.text=="true"||token.text=="false"||(!token.text.empty()&&(std::isdigit(static_cast<unsigned char>(token.text[0]))||token.text[0]=='"'))){
    if(negative&&(token.text=="true"||token.text=="false"||token.text[0]=='"'))throw Error(p.where,"negative pattern literal must be numeric");
    ++pos;p.kind=P::Literal;p.name=(negative?"-":"")+token.text;return p;
   }
   if(negative)throw Error(p.where,"expected a numeric pattern literal");
   p.name=ident();
   if(accept("(")){p.kind=P::Variant;if(!at(")"))do{p.elements.push_back(match_pattern());if(p.elements.size()>256)throw Error(p.where,"pattern width limit exceeded");}while(accept(",")&&!at(")"));consume(")");}
   return p;
  }
  Pattern match_pattern(){
   Nesting guard(*this);auto p=match_pattern_atom();
   if(accept("as")){Pattern alias;alias.kind=Pattern::Kind::As;alias.where=p.where;alias.name=ident();alias.elements.push_back(std::move(p));p=std::move(alias);}
   if(accept("|")){Pattern either;either.kind=Pattern::Kind::Or;either.where=p.where;either.elements.push_back(std::move(p));either.elements.push_back(match_pattern());return either;}
   return p;
  }
  void match_arm_block(Expr& arm){
   Nesting guard(*this);consume("{");
   while(!at("}")){
    bool structured=at("let")||at("var")||at("return")||at("if")||at("while")||at("{")||at("let_syntax")||at("letrec_syntax")||for_header()||
     (at("loop")&&peek(1).text=="{")||((at("break")||at("continue"))&&peek(1).text!="(")||
     (is_ident(peek().text)&&peek(1).text==":"&&(peek(2).text=="loop"||peek(2).text=="while"||peek(2).text=="for"))||peek(1).text=="=";
    if(structured){arm.body.push_back(statement());continue;}
    auto value=expr().node;
    if(accept(";")){Stmt s;s.kind=Stmt::Kind::Expr;s.where=value->where;s.value=std::move(value);arm.body.push_back(std::move(s));}
    else{arm.children.push_back(std::move(value));consume("}");return;}
    if(arm.body.size()>100000)throw Error(arm.where,"too many match arm statements");
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
   consume("{");
   if(!at("}")){do{
    auto arm=std::make_unique<Expr>();arm->kind=Expr::Kind::Case;arm->where=peek().where;arm->patterns.push_back(match_pattern());
    if(accept("if"))arm->children.push_back(expr().node);else arm->children.push_back(truth(arm->where));
    consume("=>");if(at("{"))match_arm_block(*arm);else arm->children.push_back(expr().node);
    auto depth=std::max(body_expression_depth(arm->body)+1,arm->children.back()->syntax_depth+1);auto checked=bounded(std::move(arm),depth);e->children.push_back(std::move(checked.node));
    if(e->children.size()>257)throw Error(where,"match arm limit exceeded");
   }while(accept(",")&&!at("}"));}
   consume("}");return syntax_bounded(std::move(e));
  }
  Param parameter(std::size_t index){
   Param result;result.where=peek().where;
   if(at("(")){result.pattern=pattern();result.name="@argument_"+std::to_string(index);}
   else result.name=ident();
   result.type=accept(":")?type():Type(Type::Hole);return result;
  }
  bool for_header()const{
   if(!at("for"))return false;
   if(is_ident(peek(1).text)&&peek(2).text=="in")return true;
   if(peek(1).text!="(")return false;
   std::size_t depth=0;
   for(std::size_t i=1;pos+i<ts.size();++i){
    if(peek(i).text=="(")++depth;
    if(peek(i).text==")"&&!--depth)return peek(i+1).text=="{";
   }
   return false;
  }
  Stmt clause(const std::string& end,bool binding=true){
   Stmt s;s.where=peek().where;
   if(binding&&(at("let")||at("var"))){
    s.kind=Stmt::Kind::Let;s.mutable_binding=accept("var");if(!s.mutable_binding)consume("let");
    if(at("("))s.pattern=pattern();else s.name=ident();
    if(accept(":"))s.declared=type();
    consume("=");s.value=expr().node;
   }else if(peek(1).text=="="){
    s.kind=Stmt::Kind::Assign;s.name=ident();consume("=");s.value=expr().node;
   }else{s.kind=Stmt::Kind::Expr;s.value=expr().node;}
   consume(end);return s;
  }
  ExprPtr truth(const Location& at){auto e=std::make_unique<Expr>();e->kind=Expr::Kind::Boolean;e->text="true";e->where=at;return e;}
  Stmt iteration(std::string label={}){
   Stmt s;s.where=peek().where;s.label=std::move(label);s.kind=Stmt::Kind::While;
   if(accept("loop")){s.value=truth(s.where);s.body=block();return s;}
   if(accept("while")){s.value=expr().node;s.body=block();return s;}
   consume("for");
   if(accept("(")){
    Stmt scope;scope.kind=Stmt::Kind::Block;scope.where=s.where;
    if(!accept(";"))scope.body.push_back(clause(";"));
    if(accept(";"))s.value=truth(s.where);else{s.value=expr().node;consume(";");}
    if(!accept(")"))s.otherwise.push_back(clause(")",false));
    s.body=block();scope.body.push_back(std::move(s));return scope;
   }
   s.kind=Stmt::Kind::Range;s.name=ident();consume("in");
   auto bounds=std::make_unique<Expr>();bounds->kind=Expr::Kind::Tuple;bounds->where=peek().where;
   bounds->children.push_back(expr().node);
   if(!at("..")&&!at("..="))throw Error(peek().where,"ON1X-CONTROL-UNSUPPORTED: for-in requires an i64 range; general iterators are not implemented","ON1X-CONTROL-UNSUPPORTED");
   s.inclusive=accept("..=");if(!s.inclusive)consume("..");bounds->children.push_back(expr().node);
   s.value=syntax_bounded(std::move(bounds)).node;s.body=block();return s;
  }
  Stmt statement(){Stmt s; s.where=peek().where;
   if(match_header()){
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
    if(exits&&at("with")&&peek(1).text!=";")throw Error(peek().where,"ON1X-CONTROL-UNSUPPORTED: value-carrying loop exits are not implemented","ON1X-CONTROL-UNSUPPORTED");
    if(!at(";"))s.label=ident();
    if(exits&&at("with"))throw Error(peek().where,"ON1X-CONTROL-UNSUPPORTED: value-carrying loop exits are not implemented","ON1X-CONTROL-UNSUPPORTED");
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
  if(peek(1).text=="="){
    s.kind=Stmt::Kind::Assign;s.name=ident();consume("=");s.value=expr().node;consume(";");return s;
  }
   s.kind=Stmt::Kind::Expr;s.value=expr().node;consume(";");return s;
 }
  public:
  explicit Parser(const std::vector<Token>&t,const std::vector<std::string>& enabled):ts(t){
   for(const auto& name:enabled)enabled_extensions.insert(extensions::canonical(name,{"<command-line>"}));
  }
  std::string module_name(){
   auto where=peek().where;std::string name=ident();
   while(accept(".")){name+="."+ident();if(name.size()>4096)throw Error(where,"module name exceeds 4096 bytes");}
   return name;
  }
   Unit parse(){Unit u;
    while(!at("<eof>")){
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
    if(accept("macro")){u.macros.push_back(macro_definition());if(u.macros.size()>512)throw Error(peek().where,"macro definition limit exceeded");continue;}
    if(at("@")&&peek(1).text=="ppx"){
     auto where=peek().where;consume("@");consume("ppx");auto label=ident();auto token=peek();
     if(label.size()>128)throw Error(where,"PPX label exceeds 128 bytes");
     if(token.text.size()<2||token.text.front()!='"'||token.text.find('\\')!=std::string::npos)throw Error(token.where,"PPX registration expects an unescaped quoted path");
     if(token.text.size()<=2||token.text.size()>4098)throw Error(token.where,"PPX program path must be 1..4096 bytes");
     if(u.ppx.size()>=32)throw Error(where,"PPX program count limit exceeded");
     ++pos;u.ppx.push_back({where,std::move(label),token.text.substr(1,token.text.size()-2)});accept(";");continue;
    }
    if(at("import")||at("from")){
     Import declaration;declaration.where=peek().where;declaration.selective=accept("from");
     if(declaration.selective){
      while(at(".")||at("...")){declaration.relative+=at("...")?3:1;++pos;}
      if(!at("import"))declaration.module=module_name();
      if(declaration.module.empty()&&!declaration.relative)throw Error(peek().where,"expected module name");
      consume("import");bool parens=accept("(");
      do{ImportName name;name.name=ident();if(accept("as"))name.alias=ident();declaration.names.push_back(std::move(name));}while(accept(",")&&!(parens&&at(")")));
      if(parens)consume(")");
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
       u.module_imports.push_back(std::move(item));
      }while(accept(","));
     }
     if(u.module_imports.size()>1024)throw Error(peek().where,"import count limit exceeded");
     consume(";");continue;
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
    // Empty files are valid package initializers; the checker still requires main.
  return u;
 }
};
}
Unit parse(const std::vector<Token>&ts,const std::vector<std::string>& enabled){return Parser(ts,enabled).parse();}
}
