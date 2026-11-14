#include <on1x/ast.hpp>
#include <on1x/generated_scan.h>
#include <algorithm>
#include <mutex>
#include <exception>
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
}
std::vector<Token> scan(const std::string& source,const std::string& filename){
  if(source.size()>max_source_bytes) throw Error({filename,0,1,1},"source exceeds 2 MiB limit");
 auto bad=source.find('\0'); if(bad!=std::string::npos)throw Error(pos_at(source,filename,bad),"NUL in source");
  // Callbacks from Aurocks arrive backwards. Index lines once rather than
  // rescanning the whole source for every token (quadratic on dense input).
  std::vector<std::size_t> lines{0};
  for(std::size_t i=0;i<source.size();++i)if(source[i]=='\n')lines.push_back(i+1);
  Collected c{{},&lines,&filename,{}}; size_t error=0;
 {static std::mutex mutex;std::lock_guard guard(mutex);
   int status=on1x_generated_scan(source.data(),source.size(),collect,&c,&error);
  if(c.error)std::rethrow_exception(c.error);
  if(status)throw Error(pos_at(source,filename,error),status==2?"scanner allocation failure":"invalid token");}
  // Fallback callbacks are forwards; the ASDL linked list is traversed backwards.
 if(!std::is_sorted(c.tokens.begin(),c.tokens.end(),[](const Token& a,const Token& b){return a.where.offset<b.where.offset;}))
   std::reverse(c.tokens.begin(),c.tokens.end());
 c.tokens.push_back({"<eof>",pos_at(source,filename,source.size())});
 return c.tokens;
}
}
