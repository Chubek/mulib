#include <on1x/ast.hpp>
#include <on1x/generated_scan.h>
#include <on1x/literal_scan.h>
#include <on1x/regex_scan.h>
#include <algorithm>
#include <mutex>
#include <exception>
#include <string_view>
namespace on1x {
namespace {
struct Collected { std::vector<Token> tokens; const std::vector<std::size_t>* lines; const std::string* file; std::exception_ptr error; };
void collect(void* ctx, const char* text, size_t pos) noexcept {
 auto& out=*static_cast<Collected*>(ctx);
 if(out.error)return;
 try {
  auto line=std::upper_bound(out.lines->begin(),out.lines->end(),pos)-out.lines->begin();
  Location at{*out.file,pos,static_cast<std::size_t>(line),pos-out.lines->at(line-1)+1};
 out.tokens.push_back({text, std::move(at)});
 } catch(...) { out.error=std::current_exception(); }
}
Location pos_at(const std::string& input,const std::string& file,std::size_t pos){
 Location at{file,pos,1,1};
 for(std::size_t i=0;i<pos&&i<input.size();++i) if(input[i]=='\n'){++at.line;at.column=1;}else ++at.column;
 return at;
}
struct RegexToken { std::size_t start,len; };
bool regex_operand(std::string_view previous) {
 // Only unambiguous operand openings. In particular, identifiers (including
 // contextual keywords), closing delimiters and postfix '?' cannot open it.
 return previous.empty()||previous=="("||previous=="["||previous=="="||previous==":="||
        previous==","||previous==";"||previous=="{"||previous=="=>"||
        previous=="return"||previous=="!"||previous=="&&"||previous=="||"||
        previous=="=="||previous=="!="||previous=="~";
}
std::vector<RegexToken> regex_tokens(const std::string& input) {
 if(input.find('/')==std::string::npos)return {};
 std::vector<RegexToken> tokens;
 std::string_view prev;
 auto layout=[&](std::size_t p){
  while(p<input.size()){
   if(input[p]==' '||input[p]=='\t'||input[p]=='\r'||input[p]=='\n'){++p;continue;}
   if(input[p]!='#'||(p+1<input.size()&&input[p+1]=='\\'))break;
   while(p<input.size()&&input[p]!='\n')++p;
  }
  return p;
 };
 for(auto i=layout(0);i<input.size();i=layout(i)){
  auto start=i;const auto c=input[i++];const auto rest=input.size()-start;
  if(c=='/'){
   const bool contextual=prev=="match"||prev=="capture";
   auto width=(contextual||regex_operand(prev))?on1x_regex_literal_width(input.data()+start,rest):0;
   bool opens=regex_operand(prev);
   if(contextual&&width){
    auto after=layout(start+width);
    opens=after<input.size()&&input[after]=='~';
   }
   if(opens&&width){
    tokens.push_back({start,width});i=start+width;
   }
  }else if(c=='"'||((c=='r'||c=='b')&&rest>=3&&input[i]=='!'&&input[i+1]=='"')){
   auto width=on1x_lit_string(input.data()+start,rest,0);
   // Leave malformed preceding terminals to the ordinary scanner.
   if(!width)break;
   i=start+width;
  }else if(c=='#'){
   auto width=on1x_lit_character(input.data()+start,rest);
   if(!width)break;
   i=start+width;
  }else if(c=='`'){
   const unsigned delim=rest>=3&&input[i]=='`'&&input[i+1]=='`'?3:1;
   i=start+delim;
   while(i<input.size()&&input.compare(i,delim,delim==3?"```":"`")!=0){
    if(delim==1&&(input[i]=='\r'||input[i]=='\n'))return tokens;
    if(delim==1&&input[i]=='\\'){
     ++i;
     if(i==input.size()||input[i]=='\r'||input[i]=='\n')return tokens;
    }
    ++i;
   }
   if(i==input.size())break;
   i+=delim;
  }else if(on1x_lit_ident(static_cast<unsigned char>(c))){
   while(i<input.size()&&on1x_lit_ident(static_cast<unsigned char>(input[i])))++i;
  }else{
   auto pair=std::string_view(input).substr(start,2);
   if(pair=="=>"||pair==":="||pair=="=="||pair=="!="||pair=="&&"||pair=="||"||
      pair=="<="||pair==">="||pair=="<-"||pair=="->"||pair=="::")++i;
  }
  prev=std::string_view(input).substr(start,i-start);
 }
 return tokens;
}
}
std::vector<Token> scan(const std::string& source,const std::string& filename){
   if(source.size()>max_source_bytes) throw Error({filename,0,1,1},"source exceeds 2 MiB limit","ON1X-LEX-LIMIT");
  auto bad=source.find('\0'); if(bad!=std::string::npos)throw Error(pos_at(source,filename,bad),"NUL in source","ON1X-LEX-NUL");
  // Callbacks from Aurocks arrive backwards. Index lines once rather than
  // rescanning the whole source for every token (quadratic on dense input).
  std::vector<std::size_t> lines{0};
  for(std::size_t i=0;i<source.size();++i)if(source[i]=='\n')lines.push_back(i+1);
  auto regexes=regex_tokens(source);
  // An ASCII punctuation placeholder occupies the same byte range as the
  // literal, preserving source offsets and leaving a non-layout separator:
  // simply blanking the span would change adjacent-token boundaries.
  std::string masked;
  const std::string* scanner_input=&source;
  if(!regexes.empty()){
   masked=source;scanner_input=&masked;
   for(auto r:regexes){
    std::fill_n(masked.begin()+static_cast<std::ptrdiff_t>(r.start),r.len,' ');
    masked[r.start]='@';
   }
  }
  Collected c{{},&lines,&filename,{}}; size_t error=0;
 {static std::mutex mutex;std::lock_guard guard(mutex);
    int status=on1x_generated_scan(scanner_input->data(),scanner_input->size(),collect,&c,&error);
  if(c.error)std::rethrow_exception(c.error);
   if(status)throw Error(pos_at(source,filename,error),status==2?"scanner allocation failure":"invalid token",status==2?"ON1X-LEX-ALLOCATION":"ON1X-LEX-TOKEN");}
  // Fallback callbacks are forwards; the ASDL linked list is traversed backwards.
  if(!std::is_sorted(c.tokens.begin(),c.tokens.end(),[](const Token& a,const Token& b){return a.where.offset<b.where.offset;}))
    std::reverse(c.tokens.begin(),c.tokens.end());
  auto r=regexes.begin();
  for(auto& token:c.tokens){
   if(r==regexes.end())break;
   if(token.where.offset!=r->start)continue;
   token.text=source.substr(r->start,r->len);++r;
  }
 c.tokens.push_back({"<eof>",pos_at(source,filename,source.size())});
 return c.tokens;
}
}
