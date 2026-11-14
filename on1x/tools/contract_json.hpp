#pragma once
// Private build-tool JSON transport. The public semantic IR remains ExoML's
// FunctionContract/CDecl, never a second foreign type or ownership universe.
#include <charconv>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace on1x::contracts::json {
struct Value {
 char kind='n';std::string text;std::vector<Value> array;std::map<std::string,Value> object;
 bool operator==(const Value&)const=default;
 const Value& at(const std::string& name)const{auto it=object.find(name);if(kind!='o'||it==object.end())throw std::invalid_argument("missing JSON field: "+name);return it->second;}
 bool has(const std::string& name)const{return object.contains(name);}
};
class Reader {
 std::string_view input;std::size_t pos=0,nodes=0;
 [[noreturn]] void fail(const std::string& message)const{throw std::invalid_argument("contract JSON at byte "+std::to_string(pos)+": "+message);}
 void space(){while(pos<input.size()&&(input[pos]==' '||input[pos]=='\r'||input[pos]=='\n'||input[pos]=='\t'))++pos;}
 bool accept(char c){space();if(pos<input.size()&&input[pos]==c){++pos;return true;}return false;}
 void expect(char c){if(!accept(c))fail(std::string("expected '")+c+"'");}
 unsigned hex(){unsigned n=0;for(int i=0;i<4;++i){if(pos==input.size())fail("truncated unicode escape");char c=input[pos++];unsigned d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:16;if(d==16)fail("invalid unicode escape");n=n*16+d;}return n;}
 std::string str(){
  expect('"');std::string out;
  while(pos<input.size()){
   unsigned char c=static_cast<unsigned char>(input[pos++]);if(c=='"')return out;if(c<32)fail("unescaped control byte");
   if(c!='\\'){
    if(c<128){out+=static_cast<char>(c);continue;}
    unsigned more=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:0;
    if(!more||more>input.size()-pos)fail("invalid UTF-8");
    unsigned point=c&((1u<<(6-more))-1);out+=static_cast<char>(c);
    for(unsigned i=0;i<more;++i){unsigned char next=static_cast<unsigned char>(input[pos++]);if((next&0xc0)!=0x80)fail("invalid UTF-8 continuation");point=(point<<6)|(next&63);out+=static_cast<char>(next);}
    if(point<(more==1?0x80u:more==2?0x800u:0x10000u)||point>0x10ffff||(point>=0xd800&&point<=0xdfff))fail("invalid UTF-8 scalar");
    continue;
   }
   if(pos==input.size())fail("truncated escape");
   c=static_cast<unsigned char>(input[pos++]);
   if(c=='"'||c=='\\'||c=='/')out+=static_cast<char>(c);
   else if(c=='n')out+='\n';else if(c=='r')out+='\r';else if(c=='t')out+='\t';else if(c=='b')out+='\b';else if(c=='f')out+='\f';
   else if(c=='u'){
    unsigned n=hex();if(n>=0xd800&&n<=0xdbff){if(pos+2>input.size()||input.substr(pos,2)!="\\u")fail("missing low surrogate");pos+=2;unsigned low=hex();if(low<0xdc00||low>0xdfff)fail("invalid low surrogate");n=0x10000+(n-0xd800)*1024+low-0xdc00;}else if(n>=0xdc00&&n<=0xdfff)fail("unpaired low surrogate");
    if(n<128)out+=static_cast<char>(n);else if(n<2048){out+=static_cast<char>(0xc0|(n>>6));out+=static_cast<char>(0x80|(n&63));}
    else if(n<65536){out+=static_cast<char>(0xe0|(n>>12));out+=static_cast<char>(0x80|((n>>6)&63));out+=static_cast<char>(0x80|(n&63));}
    else{out+=static_cast<char>(0xf0|(n>>18));out+=static_cast<char>(0x80|((n>>12)&63));out+=static_cast<char>(0x80|((n>>6)&63));out+=static_cast<char>(0x80|(n&63));}
   }else fail("unknown escape");
  }fail("unterminated string");
 }
 static bool digit(char c){return c>='0'&&c<='9';}
 void digits(){if(pos==input.size()||!digit(input[pos]))fail("expected digit");while(pos<input.size()&&digit(input[pos]))++pos;}
 Value value(unsigned depth){
  space();if(depth>=64||++nodes>1048576)fail("depth/node limit exceeded");if(pos==input.size())fail("unexpected end");Value out;
  if(accept('{')){out.kind='o';if(accept('}'))return out;do{auto key=str();expect(':');auto child=value(depth+1);if(!out.object.emplace(std::move(key),std::move(child)).second)fail("duplicate object key");}while(accept(','));expect('}');}
  else if(accept('[')){out.kind='a';if(accept(']'))return out;do{out.array.push_back(value(depth+1));}while(accept(','));expect(']');}
  else if(input[pos]=='"'){out.kind='s';out.text=str();}
  else if(input.substr(pos,4)=="true"){out.kind='b';out.text="true";pos+=4;}
  else if(input.substr(pos,5)=="false"){out.kind='b';out.text="false";pos+=5;}
  else if(input.substr(pos,4)=="null"){out.kind='n';pos+=4;}
  else{
   out.kind='d';auto begin=pos;if(input[pos]=='-')++pos;if(pos==input.size())fail("truncated number");
   if(input[pos]=='0')++pos;else digits();
   if(pos<input.size()&&input[pos]=='.'){++pos;digits();}
   if(pos<input.size()&&(input[pos]=='e'||input[pos]=='E')){++pos;if(pos<input.size()&&(input[pos]=='+'||input[pos]=='-'))++pos;digits();}
   out.text=std::string(input.substr(begin,pos-begin));
  }return out;
 }
public:
 explicit Reader(std::string_view text):input(text){if(input.size()>64*1024*1024)fail("byte limit exceeded");}
 Value read(){auto out=value(0);space();if(pos!=input.size())fail("trailing input");return out;}
};
inline Value parse(std::string_view text){return Reader(text).read();}
inline std::string quote(std::string_view text){
 std::string out="\"";static constexpr char digits[]="0123456789abcdef";
 for(unsigned char c:text){if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}else if(c<32){out+="\\u00";out+=digits[c>>4];out+=digits[c&15];}else out+=static_cast<char>(c);}return out+'"';
}
inline std::uint64_t natural(const Value& n,std::uint64_t max=UINT32_MAX){
 std::uint64_t out=0;auto [end,error]=std::from_chars(n.text.data(),n.text.data()+n.text.size(),out);
 if(n.kind!='d'||n.text.empty()||n.text[0]=='-'||error!=std::errc{}||end!=n.text.data()+n.text.size()||out>max)throw std::invalid_argument("expected bounded unsigned integer");
 return out;
}
}
