#include <on1x/meta.hpp>
#include <cctype>
#ifdef ON1X_ASDL_PPX
#include <on1x/generated_ppx.h>
#endif
namespace on1x::meta {
SExpr symbol(std::string text){SExpr n;n.kind=SExpr::Kind::Symbol;n.text=std::move(text);return n;}
SExpr string(std::string text){SExpr n;n.kind=SExpr::Kind::String;n.text=std::move(text);return n;}
SExpr list(std::vector<SExpr> items){SExpr n;n.items=std::move(items);return n;}
namespace {
int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
class Reader {
 std::string_view input;Location at;std::size_t pos=0,nodes=0;
 [[noreturn]] void fail(const std::string& message){throw Error(at,"S-expression: "+message);}
 char get(){auto c=input[pos++];++at.offset;if(c=='\n'){++at.line;at.column=1;}else ++at.column;return c;}
 void spaces(){while(pos<input.size()&&(input[pos]==' '||input[pos]=='\t'||input[pos]=='\r'||input[pos]=='\n'))get();}
 SExpr node(std::size_t depth){
  spaces();if(pos==input.size())fail("unexpected end of input");
  if(depth>=256)fail("256-level depth limit exceeded");
  if(++nodes>max_wire_nodes)fail("node limit exceeded");
  SExpr n;n.where=at;auto c=get();
  if(c=='('){while(true){spaces();if(pos==input.size())fail("unterminated list");if(input[pos]==')'){get();break;}n.items.push_back(node(depth+1));}return n;}
  if(c==')')fail("unexpected ')'");
  if(c=='"'){
   n.kind=SExpr::Kind::String;bool done=false;
   while(pos<input.size()){
    c=get();if(c=='"'){done=true;break;}
    if(c=='\\'){
     if(pos==input.size())fail("unterminated escape");
     c=get();
     if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';
     else if(c=='x'){
      if(pos+2>input.size()||hex(input[pos])<0||hex(input[pos+1])<0)fail("expected two hex digits after \\x");
      int value=hex(get())*16;value+=hex(get());c=static_cast<char>(value);
     }else if(c!='\\'&&c!='"')fail("unknown string escape");
    }else if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)>126)fail("strings must escape control/non-ASCII bytes");
    n.text+=c;
   }
   if(!done)fail("unterminated string");
   return n;
  }
  n.kind=SExpr::Kind::Symbol;
  do{
   if(static_cast<unsigned char>(c)<33||static_cast<unsigned char>(c)>126||c=='"')fail("invalid symbol byte");
   n.text+=c;
   if(pos==input.size()||input[pos]=='('||input[pos]==')'||std::isspace(static_cast<unsigned char>(input[pos])))break;
   c=get();
  }while(true);
  return n;
 }
public:
 Reader(std::string_view text,const std::string& file):input(text){if(file.size()>4096)throw Error({"<ppx>"},"PPX diagnostic filename exceeds 4096 bytes");at.file=file;if(text.size()>max_wire_bytes)fail("byte limit exceeded");}
 SExpr read(){auto result=node(0);spaces();if(pos!=input.size())fail("trailing input after root");return result;}
};
void put(const SExpr& n,std::string& out,std::size_t depth,std::size_t& nodes){
 if(depth>=256||++nodes>max_wire_nodes)throw Error(n.where,"S-expression output tree limit exceeded");
 if(n.kind==SExpr::Kind::List){
  out+='(';for(std::size_t i=0;i<n.items.size();++i){if(i)out+=' ';put(n.items[i],out,depth+1,nodes);}out+=')';
 }else if(n.kind==SExpr::Kind::Symbol){
  if(n.text.empty())throw Error(n.where,"S-expression: empty symbol");
  for(unsigned char c:n.text)if(c<33||c>126||c=='('||c==')'||c=='"')throw Error(n.where,"S-expression: invalid output symbol");
  out+=n.text;
 }
 else{
  static constexpr char digits[]="0123456789abcdef";out+='"';
  for(unsigned char c:n.text){
   if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}
   else if(c>=32&&c<=126)out+=static_cast<char>(c);
   else{out+="\\x";out+=digits[c>>4];out+=digits[c&15];}
  }out+='"';
 }
 if(out.size()>max_wire_bytes)throw Error(n.where,"S-expression output byte limit exceeded");
}
}
SExpr read_sexpr(std::string_view input,const std::string& file){
 auto tree=Reader(input,file).read();
#ifdef ON1X_ASDL_PPX
 struct Arena {void* value=on1x_ppx_arena_new();~Arena(){on1x_ppx_arena_free(value);}} arena;
 if(!arena.value)throw Error(tree.where,"PPX ASDL arena allocation failure");
 auto build=[&](auto&& self,const SExpr& n)->void*{
  void* out=nullptr;
  if(n.kind==SExpr::Kind::List){std::vector<void*> items;for(const auto& c:n.items)items.push_back(self(self,c));out=on1x_ppx_list(arena.value,items.data(),items.size());}
  else out=on1x_ppx_atom(arena.value,n.kind==SExpr::Kind::String,n.text.data(),n.text.size());
  if(!out)throw Error(n.where,"PPX ASDL node allocation failure");
  return out;
 };
 if(!on1x_ppx_validate(build(build,tree)))throw Error(tree.where,"PPX ASDL structural validation failed");
#endif
 return tree; // No arena-owned pointers/strings cross this boundary.
}
std::string write_sexpr(const SExpr& n){std::string out;std::size_t nodes=0;put(n,out,0,nodes);if(out.size()>=max_wire_bytes)throw Error(n.where,"S-expression output byte limit exceeded");return out+'\n';}
std::string literal_value(const std::string& text){
 std::string value;
 for(std::size_t i=1;i+1<text.size();++i){char c=text[i];if(c=='\\'){
  c=text[++i];if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';else if(c=='0')c='\0';
 }value+=c;}return value;
}
std::string source_literal(const std::string& value){
 std::string out="\"";for(char c:value){
  if(c=='\0')throw std::runtime_error("NUL is unsupported by the On1x string ABI");
  if(c=='\\'||c=='"'){out+='\\';out+=c;}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else out+=c;
 }return out+'"';
}
}
