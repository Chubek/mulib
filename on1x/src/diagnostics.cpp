#include <on1x/diagnostics.hpp>
#include <algorithm>
#include <limits>
#include <sstream>

namespace on1x::diagnostics {
namespace {
using namespace moonstone;
using namespace moonstone::syren;
std::uint32_t narrow(std::size_t n){return static_cast<std::uint32_t>(std::min(n,std::size_t(UINT32_MAX)));}
// Return a strict UTF-8 scalar's width, or zero for an invalid sequence.
std::size_t width(std::string_view s,std::size_t i){
 auto c=static_cast<unsigned char>(s[i]);
 if(c<128)return 1;
 std::size_t n=c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
 if(!n||n>s.size()-i)return 0;
 for(std::size_t j=1;j<n;++j)if((static_cast<unsigned char>(s[i+j])&0xc0)!=0x80)return 0;
 auto next=static_cast<unsigned char>(s[i+1]);
 if((c==0xe0&&next<0xa0)||(c==0xed&&next>=0xa0)||(c==0xf0&&next<0x90)||(c==0xf4&&next>=0x90))return 0;
 return n;
}
std::string quote(std::string_view s){
 std::string valid;
 for(std::size_t i=0;i<s.size();){
  auto n=width(s,i);
  if(n){valid+=json_escape(s.substr(i,n));i+=n;}
  else {const char* hex="0123456789abcdef";auto c=static_cast<unsigned char>(s[i++]);valid+="\\u00";valid+=hex[c>>4];valid+=hex[c&15];}
 }
 return "\""+valid+"\"";
}
std::string uri(std::string_view path){
 std::string out=path.starts_with('/')?"file://":"";const char* hex="0123456789ABCDEF";
 for(unsigned char c:path){
  if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='-'||c=='_'||c=='.'||c=='~')out+=char(c);
  else {out+='%';out+=hex[c>>4];out+=hex[c&15];}
 }
 return out;
}
bool located(const Error& e,std::string_view s){
 if(e.where.offset>s.size())return false;
 std::size_t line=1,column=1;
 for(std::size_t i=0;i<e.where.offset;++i)if(s[i]=='\n'){++line;column=1;}else ++column;
 return line==e.where.line&&column==e.where.column;
}
const Catalog& catalog(){
 static const Catalog rules=[] {
  Catalog c;
  for(const auto& [id,note]:std::vector<std::pair<std::string,std::string>>{
    {"ON1X-SOURCE","inspect the reported source; no automatic semantic repair is available"},
    {"ON1X-PARSE-EXPECTED","check the expected token and surrounding delimiters"},
    {"ON1X-PARSE-TYPE","check the type spelling; type changes require review"},
    {"ON1X-TYPE","check the inferred types; no coercion or language rule has been changed"},
    {"ON1X-SEMA","check the binding and semantic constraints at this location"},
    {"ON1X-LEX-TOKEN","check string escapes, comments and token spelling"},
    {"ON1X-LEX-NUL","source files cannot contain embedded NUL bytes"},
    {"ON1X-LEX-LIMIT","reduce the source size; the compiler limit is unchanged"},
    {"ON1X-LEX-ALLOCATION","scanner allocation failed; no repair was attempted"}}){
   auto rule=parse_rule("(error (id "+id+") (message \""+note+"\") (pattern ("+id+" ?message)))");
   c.add(std::move(rule));
  }
  return c;
 }();
 return rules;
}
std::string edits_json(const std::vector<SourceEdit>& edits){
 std::string out="[";bool first=true;
 for(const auto& e:edits){if(!first)out+=',';first=false;out+="{\"begin\":"+std::to_string(e.begin)+",\"end\":"+std::to_string(e.end)+",\"replacement\":"+quote(e.replacement)+"}";}
 return out+"]";
}
std::optional<std::size_t> utf16_column(const Report& r){
 if(!r.source)return {};
 const auto& s=*r.source;auto start=r.offset;
 while(start&&s[start-1]!='\n')--start;
 std::size_t column=1;
 while(start<r.offset){auto n=width(s,start);if(!n||n>r.offset-start)return {};column+=n==4?2:1;start+=n;}
 return column;
}
}
const char* status_name(moonstone::syren::RepairStatus s){
 using S=moonstone::syren::RepairStatus;
 switch(s){case S::Candidate:return "candidate";case S::Rejected:return "rejected";case S::Validated:return "validated";case S::Inconclusive:return "inconclusive";}return "inconclusive";
}
Report describe(const Error& e,std::optional<std::string> source){
 using namespace moonstone;
 Report r;r.offset=e.where.offset;
 auto span=SourceSpan{e.where.file,narrow(e.where.line),narrow(e.where.column),narrow(e.where.line),narrow(e.where.column)};
 // Match actual compiler events, not source text or user-controlled rules.
 auto event=sexpr::Node::sequence({sexpr::Node::symbol(e.diagnostic_id),sexpr::Node::symbol(e.what())});
 auto ds=catalog().diagnose(event,span);
 if(!ds.empty()){r.diagnostic=std::move(ds.front());r.diagnostic.notes.push_back(r.diagnostic.message);}
 r.diagnostic.id=e.diagnostic_id;r.diagnostic.primary=span;r.diagnostic.message=e.what();
 if(source&&source->size()<=max_source_bytes&&located(e,*source))r.source=std::move(source);
 if(!r.source)return r;
 const auto& s=*r.source;
 auto propose=[&](std::string id,std::size_t end,std::string replacement){
  r.repairs.push_back({std::move(id),"Proposed source change; requires review and compiler validation",{{r.offset,end,std::move(replacement)}},1,moonstone::syren::RepairStatus::Candidate,{}});
 };
 bool token_matches=!e.found.empty()&&((e.found=="<eof>"&&r.offset==s.size())||s.substr(r.offset,e.found.size())==e.found);
 if(e.diagnostic_id=="ON1X-PARSE-EXPECTED"&&token_matches&&
    (e.expected==";"||e.expected==")"||e.expected=="}"||e.expected=="]"||e.expected==","||e.expected==":"||e.expected=="->")){
  propose("insert-token",r.offset,e.expected);
  if(e.found!="<eof>"&&e.found.size()==1&&std::string_view(";)}],:").find(e.found)!=std::string_view::npos)
   propose("replace-token",r.offset+e.found.size(),e.expected);
 }
 if(e.diagnostic_id=="ON1X-PARSE-TYPE"&&token_matches&&e.found.size()<=16){
  // Deliberately bounded spelling suggestions, never invented types/coercions.
  for(auto k:{Type::I64,Type::Bool,Type::String,Type::Void,Type::Json,Type::JsonView,Type::SExpr,Type::Buffer}){
   auto name=type_name(k);std::vector<std::size_t> row(name.size()+1);
   for(std::size_t j=0;j<row.size();++j)row[j]=j;
   for(std::size_t i=0;i<e.found.size();++i){auto diagonal=row[0];row[0]=i+1;
    for(std::size_t j=1;j<row.size();++j){auto previous=row[j];row[j]=std::min({row[j]+1,row[j-1]+1,diagonal+(e.found[i]!=name[j-1])});diagonal=previous;}}
   if(row.back()==1)propose("type-"+name,r.offset+e.found.size(),name);
  }
 }
 if(!r.repairs.empty())r.diagnostic.notes.push_back("Repairs are suggestions, not proof of intended semantics; input files are never modified.");
 return r;
}
void validate(Report& r,const moonstone::syren::Validator& validator){
 using namespace moonstone;using namespace moonstone::syren;
 for(auto& c:r.repairs){
  bool valid=r.source.has_value()&&!c.edits.empty()&&c.edits.size()<=16;
  std::size_t end=0,previous=std::numeric_limits<std::size_t>::max();
  for(const auto& e:c.edits){
   if(!valid)break;
   valid=e.begin>=end&&e.begin!=previous&&e.end>=e.begin&&e.end<=r.source->size()&&e.replacement.size()<=max_source_bytes;
   previous=e.begin;end=e.end;
  }
  if(!valid){c.status=RepairStatus::Rejected;c.reason="invalid, unordered, overlapping or duplicate edits";continue;}
  c=verify_repair(std::move(c),*r.source,[&](std::string_view changed){
   if(changed.size()>max_source_bytes)return ProofResult{Status::InvalidInput,"repaired source exceeds source byte limit",{},0};
   if(!validator)return ProofResult{Status::Unknown,"no host validator supplied",{},0};
   try{return validator(changed);}
   catch(const std::exception& e){return ProofResult{Status::Unknown,std::string("host validation failed: ")+e.what(),{},0};}
  });
 }
 // The supplied Syren comparator does not strictly order rejected/inconclusive
 // groups. Pre-group statuses and use its ranker only within equal-status groups.
 auto rank=[](RepairStatus s){return s==RepairStatus::Validated?0:s==RepairStatus::Candidate?1:s==RepairStatus::Inconclusive?2:3;};
 std::stable_sort(r.repairs.begin(),r.repairs.end(),[&](const auto& a,const auto& b){return rank(a.status)<rank(b.status);});
 for(auto begin=r.repairs.begin();begin!=r.repairs.end();){
  auto end=std::find_if(begin,r.repairs.end(),[&](const auto& c){return c.status!=begin->status;});
  auto group=rank_repairs({begin,end});std::move(group.begin(),group.end(),begin);begin=end;
 }
}
std::string text(const Report& r){
 const auto& d=r.diagnostic;
 // Keep the existing first line byte-for-byte compatible for editor parsers.
 auto out=d.primary.file+":"+std::to_string(d.primary.line)+":"+std::to_string(d.primary.column)+": error: "+d.message+"\n  diagnostic: "+d.id+"\n";
 for(const auto& note:d.notes)out+="  note: "+note+"\n";
 for(const auto& c:r.repairs){out+="  repair["+c.id+"] ("+status_name(c.status)+"): ";
  for(const auto& e:c.edits)out+="bytes "+std::to_string(e.begin)+".."+std::to_string(e.end)+" -> "+quote(e.replacement)+" ";
  if(!c.reason.empty())out+="("+c.reason+")";
  out+='\n';}
 return out;
}
std::string json(const Report& r){
 const auto& d=r.diagnostic;
 std::string out="{\"schema\":\"on1x.diagnostics/v1\",\"diagnostics\":[{\"id\":"+quote(d.id)+",\"severity\":"+quote(moonstone::name(d.severity))+",\"message\":"+quote(d.message)+
  ",\"location\":{\"file\":"+quote(d.primary.file)+",\"offset\":"+std::to_string(r.offset)+",\"line\":"+std::to_string(d.primary.line)+",\"byteColumn\":"+std::to_string(d.primary.column)+"},\"notes\":[";
 bool first=true;for(const auto& note:d.notes){if(!first)out+=',';first=false;out+=quote(note);}out+="],\"repairs\":[";first=true;
 for(const auto& c:r.repairs){if(!first)out+=',';first=false;out+="{\"id\":"+quote(c.id)+",\"status\":"+quote(status_name(c.status))+",\"reason\":"+quote(c.reason)+",\"edits\":"+edits_json(c.edits)+"}";}
 return out+"]}]}\n";
}
std::string sarif(const Report& r){
 // Syren's serializer does not convert byte columns or encode artifact URIs.
 // Retain its diagnostic model, but serialize those fields with host knowledge.
 const auto& d=r.diagnostic;
 std::string out="{\"version\":\"2.1.0\",\"$schema\":\"https://json.schemastore.org/sarif-2.1.0.json\",\"runs\":[{\"tool\":{\"driver\":{\"name\":\"on1xc (Syren)\"}},\"columnKind\":\"utf16CodeUnits\",\"results\":[{\"ruleId\":"+quote(d.id)+",\"level\":\"error\",\"message\":{\"text\":"+quote(d.message)+"}";
 if(!d.primary.file.empty()){
  out+=",\"locations\":[{\"physicalLocation\":{\"artifactLocation\":{\"uri\":"+quote(uri(d.primary.file))+"},\"region\":{\"startLine\":"+std::to_string(std::max(1u,d.primary.line));
  if(auto column=utf16_column(r))out+=",\"startColumn\":"+std::to_string(*column);
  out+="}}}]";
 }
 // Candidate edits are not SARIF fixes: even a type-checked proposal has no
 // proof of the author's intended behavior. Expose statuses as properties.
 out+=",\"properties\":{\"on1xReport\":"+json(r)+"}}]}]}\n";
 return out;
}
}
