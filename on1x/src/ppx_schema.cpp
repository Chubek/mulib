#include <on1x/meta.hpp>
#include "ppx_schema.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace on1x::meta {
namespace {
struct Json {char kind='n';std::string text;std::vector<Json> array;std::map<std::string,Json> object;};
class JsonReader {
 std::string_view text;std::size_t pos=0,nodes=0;Location at;
 [[noreturn]] void fail(const std::string& message){at.offset=pos;at.line=at.column=1;for(std::size_t i=0;i<pos;++i){if(text[i]=='\n'){++at.line;at.column=1;}else ++at.column;}throw Error(at,"PPX schema JSON: "+message);}
 void space(){while(pos<text.size()&&(text[pos]==' '||text[pos]=='\r'||text[pos]=='\n'||text[pos]=='\t'))++pos;}
 bool accept(char c){space();if(pos<text.size()&&text[pos]==c){++pos;return true;}return false;}
 void expect(char c){if(!accept(c))fail(std::string("expected '")+c+"'");}
 unsigned code(){unsigned n=0;for(int i=0;i<4;++i){if(pos==text.size())fail("truncated unicode escape");char c=text[pos++];unsigned d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:16;if(d==16)fail("invalid unicode escape");n=n*16+d;}return n;}
 std::string str(){
  expect('"');std::string out;
  while(pos<text.size()){
   unsigned char c=static_cast<unsigned char>(text[pos++]);if(c=='"')return out;
   if(c<32)fail("control byte in string");
   if(c!='\\'){out+=static_cast<char>(c);continue;}
   if(pos==text.size())fail("truncated escape");
   c=static_cast<unsigned char>(text[pos++]);
   if(c=='"'||c=='\\'||c=='/')out+=static_cast<char>(c);
   else if(c=='n')out+='\n';else if(c=='r')out+='\r';else if(c=='t')out+='\t';else if(c=='b')out+='\b';else if(c=='f')out+='\f';
   else if(c=='u'){
    unsigned n=code();if(n>=0xd800&&n<=0xdbff){if(pos+2>text.size()||text.substr(pos,2)!="\\u")fail("missing low surrogate");pos+=2;auto low=code();if(low<0xdc00||low>0xdfff)fail("invalid low surrogate");n=0x10000+(n-0xd800)*1024+low-0xdc00;}else if(n>=0xdc00&&n<=0xdfff)fail("unpaired low surrogate");
    if(n<128)out+=static_cast<char>(n);else if(n<2048){out+=static_cast<char>(0xc0|(n>>6));out+=static_cast<char>(0x80|(n&63));}
    else if(n<65536){out+=static_cast<char>(0xe0|(n>>12));out+=static_cast<char>(0x80|((n>>6)&63));out+=static_cast<char>(0x80|(n&63));}
    else{out+=static_cast<char>(0xf0|(n>>18));out+=static_cast<char>(0x80|((n>>12)&63));out+=static_cast<char>(0x80|((n>>6)&63));out+=static_cast<char>(0x80|(n&63));}
   }else fail("invalid escape");
  }fail("unterminated string");
 }
 Json value(unsigned depth){
  space();if(depth>=64||++nodes>65536)fail("tree limit exceeded");if(pos==text.size())fail("unexpected end");Json n;
  if(text[pos]=='{'){
   ++pos;n.kind='o';if(accept('}'))return n;
   do{auto key=str();expect(':');auto child=value(depth+1);if(!n.object.emplace(std::move(key),std::move(child)).second)fail("duplicate object key");}while(accept(','));expect('}');
  }else if(text[pos]=='['){++pos;n.kind='a';if(accept(']'))return n;do{n.array.push_back(value(depth+1));}while(accept(','));expect(']');}
  else if(text[pos]=='"'){n.kind='s';n.text=str();}
  else{n.kind='v';auto start=pos;while(pos<text.size()&&text[pos]!=','&&text[pos]!=']'&&text[pos]!='}'&&text[pos]!=' '&&text[pos]!='\n'&&text[pos]!='\r'&&text[pos]!='\t')++pos;n.text=std::string(text.substr(start,pos-start));if(n.text!="1")fail("only schema version 1 scalar is supported");}
  return n;
 }
public:
 JsonReader(std::string_view input,const std::string& file):text(input){at.file=file;if(text.size()>max_source_bytes)fail("byte limit exceeded");}
 Json read(){auto n=value(0);space();if(pos!=text.size())fail("trailing input");return n;}
};
const Json& member(const Json& n,const char* key){auto found=n.object.find(key);if(n.kind!='o'||found==n.object.end())throw std::runtime_error(std::string("PPX schema: missing field ")+key);return found->second;}
std::string text(const Json& n){if(n.kind!='s')throw std::runtime_error("PPX schema: expected string");return n.text;}
bool identifier(const std::string& s){
 auto alpha=[](char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';};
 if(s.empty()||!alpha(s[0]))return false;
 for(char c:s)if(!alpha(c)&&(c<'0'||c>'9'))return false;
  static const std::set<std::string> keywords={"fn","let","var","if","else","while","return","true","false","extern","import","forall","macro","rule","quote","splice","lift"};return !keywords.contains(s);
}
bool natural(const std::string& s,bool positive){
 std::size_t v=0;auto [end,ec]=std::from_chars(s.data(),s.data()+s.size(),v);
 return !s.empty()&&s[0]!='-'&&ec==std::errc{}&&end==s.data()+s.size()&&(!positive||v>0);
}
bool scalar(const SExpr& n,const std::string& expected){
 bool quoted=n.kind==SExpr::Kind::String;const auto& s=n.text;
 if(n.kind==SExpr::Kind::List||s.find('\0')!=std::string::npos)return false;
 if(expected=="Bytes")return quoted;
 if(expected=="File")return quoted&&s.size()<=4096&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32||c==127;});
 if(expected=="Name")return quoted&&identifier(s);
 if(expected=="Version")return !quoted&&s=="1";
 if(expected=="Nat"||expected=="Positive")return !quoted&&natural(s,expected=="Positive");
 if(expected=="Boolean")return !quoted&&(s=="true"||s=="false");
 if(expected=="UnaryOperator")return !quoted&&(s=="!"||s=="-");
 if(expected=="BinaryOperator"){
  static const std::set<std::string> operators={"+","-","*","/","%","<",">","<=",">=","==","!=","&&","||"};return !quoted&&operators.contains(s);
 }
 if(expected=="DecimalSpelling"){
  if(!quoted||s.empty())return false;
  std::size_t i=s[0]=='-'?1:0;if(i==s.size())return false;
  for(;i<s.size();++i)if(s[i]<'0'||s[i]>'9')return false;
  return true;
 }
 if(expected=="HexSpelling")return quoted&&s.size()>2&&s[0]=='0'&&(s[1]=='x'||s[1]=='X')&&std::all_of(s.begin()+2,s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');});
 return false;
}
const std::set<std::string> scalars={"Bytes","File","Name","Version","Nat","Positive","Boolean","UnaryOperator","BinaryOperator","DecimalSpelling","HexSpelling"};
void add_nodes(Schema& schema,const Json& input,const std::string& owner){
 if(input.kind!='o')throw std::runtime_error("PPX schema: nodes must be an object");
 for(const auto& [tag,definition]:input.object){
  if(schema.nodes.size()>=1024)throw std::runtime_error("PPX schema constructor limit exceeded");
  if(tag.empty()||tag.size()>128||tag.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_:")!=std::string::npos)throw std::runtime_error("PPX schema: invalid constructor name");
  if(!owner.empty()&&!tag.starts_with(owner+":"))throw std::runtime_error("PPX schema extensions must prefix constructors with "+owner+":");
  SchemaNode node;node.owner=owner;node.category=text(member(definition,"category"));
  if(!schema.parents.contains(node.category))throw std::runtime_error("PPX schema: unknown category "+node.category);
  const auto& fields=member(definition,"fields");if(fields.kind!='a'||fields.array.size()>32)throw std::runtime_error("PPX schema: invalid fields array");std::set<std::string> names;
  for(const auto& field:fields.array){
   if(field.kind!='a'||field.array.size()!=2)throw std::runtime_error("PPX schema: field must be [name, type]");
   auto name=text(field.array[0]),type=text(field.array[1]);
   if(name.size()>128||!identifier(name)||!names.insert(name).second)throw std::runtime_error("PPX schema: invalid/duplicate field name");
   auto base=type.ends_with('*')?type.substr(0,type.size()-1):type;
   if(!schema.parents.contains(base)&&!scalars.contains(base))throw std::runtime_error("PPX schema: unknown field type "+type);
   node.fields.emplace_back(std::move(name),std::move(type));
  }
  if(!schema.nodes.emplace(tag,std::move(node)).second)throw std::runtime_error("PPX schema: constructor cannot be redefined: "+tag);
 }
}
}
bool Schema::subtype(std::string child,const std::string& parent)const{
 for(std::size_t i=0;i<=parents.size();++i){if(child==parent)return true;auto found=parents.find(child);if(found==parents.end()||found->second.empty())return false;child=found->second;}return false;
}
Schema base_schema(){
 auto json=JsonReader(embedded_schema,"PPX.schema.json").read();
 if(text(member(json,"schema_format"))!="on1x.sexpr-ast/v1"||member(json,"version").kind!='v'||member(json,"version").text!="1")throw std::runtime_error("unsupported PPX schema version");
 Schema out;const auto& cats=member(json,"categories");if(cats.kind!='o')throw std::runtime_error("PPX schema: categories must be object");
 for(const auto& [name,parent]:cats.object)out.parents.emplace(name,text(parent));
 for(const auto& [name,parent]:out.parents){(void)parent;if(!out.subtype(name,"Syntax"))throw std::runtime_error("PPX schema: cyclic/unknown syntax category");}
 add_nodes(out,member(json,"nodes"),{});return out;
}
void Schema::extend(std::string_view json,const std::string& label,const std::string& file){
 try{
  auto extension=JsonReader(json,file).read();if(member(extension,"version").kind!='v'||member(extension,"version").text!="1")throw Error({file},"unsupported PPX schema extension version");
  auto copy=*this;add_nodes(copy,member(extension,"nodes"),label);*this=std::move(copy);
 }catch(const Error&){throw;}catch(const std::exception& error){throw Error({file},error.what());}
}
void Schema::validate(const SExpr& tree,const std::string& expected,const std::string& path)const{
 std::size_t count=0;
 auto visit=[&](auto&& self,const SExpr& n,const std::string& type,const std::string& at,unsigned depth)->void{
  auto fail=[&](const std::string& message){throw Error(n.where,"PPX schema at "+at+": "+message);};
  if(depth>=256||++count>max_wire_nodes)fail("tree limit exceeded");
  if(type.ends_with('*')){
   if(n.kind!=SExpr::Kind::List)fail("expected a list of "+type);
   for(std::size_t i=0;i<n.items.size();++i)self(self,n.items[i],type.substr(0,type.size()-1),at+"["+std::to_string(i)+"]",depth+1);
   return;
  }
  if(scalars.contains(type)){if(!scalar(n,type))fail("expected "+type);return;}
  if(n.kind!=SExpr::Kind::List||n.items.empty()||n.items[0].kind!=SExpr::Kind::Symbol)fail("expected "+type+" constructor");
  auto found=nodes.find(n.items[0].text);if(found==nodes.end())fail("unknown constructor "+n.items[0].text);
  const auto& shape=found->second;
  if(!subtype(shape.category,type))fail(n.items[0].text+" has category "+shape.category+", expected "+type);
  if(n.items.size()!=shape.fields.size()+1)fail(n.items[0].text+" expects "+std::to_string(shape.fields.size())+" fields, got "+std::to_string(n.items.size()-1));
  for(std::size_t i=0;i<shape.fields.size();++i)self(self,n.items[i+1],shape.fields[i].second,at+"."+shape.fields[i].first,depth+1);
 };
 visit(visit,tree,expected,path,0);
}
}
