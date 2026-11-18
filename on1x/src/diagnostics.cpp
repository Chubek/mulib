#include <on1x/diagnostics.hpp>
#include <algorithm>
#include <cctype>
#include <limits>

namespace on1x::diagnostics {
namespace {
using namespace moonstone;
using namespace moonstone::syren;

std::uint32_t narrow(std::size_t value){
 return static_cast<std::uint32_t>(std::min(value,std::size_t(UINT32_MAX)));
}
std::size_t width(std::string_view text,std::size_t offset){
 auto c=static_cast<unsigned char>(text[offset]);
 if(c<128)return 1;
 std::size_t count=c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
 if(!count||count>text.size()-offset)return 0;
 for(std::size_t i=1;i<count;++i)if((static_cast<unsigned char>(text[offset+i])&0xc0)!=0x80)return 0;
 auto next=static_cast<unsigned char>(text[offset+1]);
 if((c==0xe0&&next<0xa0)||(c==0xed&&next>=0xa0)||(c==0xf0&&next<0x90)||(c==0xf4&&next>=0x90))return 0;
 return count;
}
std::string quote(std::string_view text){
 std::string valid;
 for(std::size_t i=0;i<text.size();){
  auto count=width(text,i);
  if(count){valid+=json_escape(text.substr(i,count));i+=count;}
  else {const char* hex="0123456789abcdef";auto c=static_cast<unsigned char>(text[i++]);valid+="\\u00";valid+=hex[c>>4];valid+=hex[c&15];}
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
bool located(const Location& where,std::string_view source){
 if(where.offset>source.size())return false;
 std::size_t line=1,column=1;
 for(std::size_t i=0;i<where.offset;++i)if(source[i]=='\n'){++line;column=1;}else ++column;
 return line==where.line&&column==where.column;
}
moonstone::Severity severity(diagnostic::Severity value){
 switch(value){
  case diagnostic::Severity::Note:return moonstone::Severity::Note;
  case diagnostic::Severity::Warning:return moonstone::Severity::Warning;
  case diagnostic::Severity::Error:return moonstone::Severity::Error;
 }
 std::terminate();
}
Catalog syren_catalog(const diagnostic::Catalog& source){
 Catalog result;
 for(const auto& [id,entry]:source.entries()){
  if(entry.provider.kind!=diagnostic::ProviderKind::Compiler)continue;
  Rule rule;rule.id=id;rule.name=entry.category;rule.category=entry.category;rule.phase=entry.phase;
  rule.message=entry.guidance;rule.severity=severity(entry.level.kind);
  rule.pattern=sexpr::Node::sequence({sexpr::Node::symbol(id),sexpr::Node::symbol("?message")});
  result.add(std::move(rule));
 }
 return result;
}
void metadata(Report& report,const diagnostic::CatalogEntry& entry){
 report.category=entry.category;report.phase=entry.phase;report.origin=entry.provider.origin;
 report.check=entry.check;report.remediation=entry.remediation;report.level=entry.level;
}
const diagnostic::Presentation& presentation(const Bundle& bundle){
 return bundle.diagnostics.empty()?bundle.presentation:bundle.diagnostics.front().presentation;
}
std::string edits_json(const std::vector<SourceEdit>& edits){
 std::string out="[";bool first=true;
 for(const auto& edit:edits){
  if(!first)out+=',';first=false;
  out+="{\"begin\":"+std::to_string(edit.begin)+",\"end\":"+std::to_string(edit.end)+",\"replacement\":"+quote(edit.replacement)+"}";
 }
 return out+"]";
}
std::optional<std::size_t> utf16_column(const Report& report){
 if(!report.source)return {};
 const auto& source=*report.source;auto start=report.offset;
 while(start&&source[start-1]!='\n')--start;
 std::size_t column=1;
 while(start<report.offset){
  auto count=width(source,start);if(!count||count>report.offset-start)return {};
  column+=count==4?2:1;start+=count;
 }
 return column;
}
std::string report_object(const Report& report){
 const auto& item=report.diagnostic;
 std::string out="{\"id\":"+quote(item.id)+",\"severity\":"+quote(report.level.name)+
  ",\"message\":"+quote(item.message)+",\"location\":{\"file\":"+quote(item.primary.file)+
  ",\"offset\":"+std::to_string(report.offset)+",\"line\":"+std::to_string(item.primary.line)+
  ",\"byteColumn\":"+std::to_string(item.primary.column)+"},\"category\":"+quote(report.category)+
  ",\"phase\":"+quote(report.phase)+",\"origin\":"+quote(report.origin)+",\"check\":"+quote(report.check)+
  ",\"remediation\":"+quote(report.remediation)+",\"notes\":[";
 bool first=true;
 for(const auto& note:item.notes){if(!first)out+=',';first=false;out+=quote(note);}
 out+="],\"repairs\":[";first=true;
 for(const auto& candidate:report.repairs){
  if(!first)out+=',';first=false;
  out+="{\"id\":"+quote(candidate.id)+",\"status\":"+quote(status_name(candidate.status,report.presentation))+
   ",\"reason\":"+quote(candidate.reason)+",\"edits\":"+edits_json(candidate.edits)+"}";
 }
 return out+"]}";
}
}

std::string status_name(moonstone::syren::RepairStatus status,const diagnostic::Presentation& presentation){
 using Status=moonstone::syren::RepairStatus;
 std::size_t index=3;
 switch(status){
  case Status::Candidate:index=0;break;
  case Status::Rejected:index=1;break;
  case Status::Validated:index=2;break;
  case Status::Inconclusive:index=3;break;
 }
 return presentation.repair_status[index];
}

Report describe(const Error& error,std::optional<std::string> source,const diagnostic::Catalog& catalog){
 const auto bounded=catalog.record(error);
 if(bounded.id!=catalog.compiler(error.diagnostic_id).id)
  return describe(bounded,std::nullopt,diagnostic::Catalog::embedded());
 Report report;report.offset=error.where.offset;report.presentation=catalog.presentation();
 auto span=SourceSpan{error.where.file,narrow(error.where.line),narrow(error.where.column),narrow(error.where.line),narrow(error.where.column)};
 const auto& entry=catalog.compiler(error.diagnostic_id);
 auto rules=syren_catalog(catalog);
 auto event=sexpr::Node::sequence({sexpr::Node::symbol(entry.id),sexpr::Node::symbol(error.what())});
 auto diagnosed=rules.diagnose(event,span);
 if(!diagnosed.empty()){report.diagnostic=std::move(diagnosed.front());report.diagnostic.notes.push_back(report.diagnostic.message);}
 else report.diagnostic.notes.push_back(entry.guidance);
 report.diagnostic.id=entry.id;report.diagnostic.primary=span;
 report.diagnostic.message=bounded.message;
 report.diagnostic.severity=severity(entry.level.kind);metadata(report,entry);
 if(source&&source->size()<=max_source_bytes&&located(error.where,*source))report.source=std::move(source);
 if(!report.source)return report;
 const auto& snapshot=*report.source;
 auto propose=[&](std::string id,std::size_t end,std::string replacement){
  report.repairs.push_back({std::move(id),entry.repair.candidate_description,{{report.offset,end,std::move(replacement)}},
    1,moonstone::syren::RepairStatus::Candidate,{}});
 };
 bool token_matches=!error.found.empty()&&((error.found==entry.repair.end_marker&&report.offset==snapshot.size())||
  snapshot.substr(report.offset,error.found.size())==error.found);
 // Some post-inference type checks retain the containing statement span.
 // Recover only one exact identifier on that bounded source line; validation
 // remains mandatory before a proposal can be marked validated.
 if(!token_matches&&entry.repair.kind==diagnostic::RepairKind::PrimitiveType&&!error.found.empty()&&error.found.size()<=16){
  auto identifier=[](unsigned char c){return std::isalnum(c)||c=='_';};
  auto line_end=snapshot.find('\n',report.offset);
  if(line_end==std::string::npos)line_end=snapshot.size();
  line_end=std::min(line_end,report.offset+std::size_t{512});
  std::optional<std::size_t> unique;
  for(auto found=snapshot.find(error.found,report.offset);
      found!=std::string::npos&&found+error.found.size()<=line_end;
      found=snapshot.find(error.found,found+1)){
   const bool left=found==0||!identifier(static_cast<unsigned char>(snapshot[found-1]));
   const auto after=found+error.found.size();
   const bool right=after==snapshot.size()||!identifier(static_cast<unsigned char>(snapshot[after]));
   if(!left||!right)continue;
   if(unique){unique.reset();break;}
   unique=found;
  }
  if(unique){
   auto delta=*unique-report.offset;report.offset=*unique;
   report.diagnostic.primary.column=narrow(std::size_t(report.diagnostic.primary.column)+delta);
   report.diagnostic.primary.end_column=report.diagnostic.primary.column;
   token_matches=true;
  }
 }
 if(entry.repair.kind==diagnostic::RepairKind::Token&&token_matches&&
    std::find(entry.repair_tokens.begin(),entry.repair_tokens.end(),error.expected)!=entry.repair_tokens.end()){
  propose(entry.repair.insert_id,report.offset,error.expected);
  if(error.found!=entry.repair.end_marker&&error.found.size()==1&&
     std::find(entry.repair_tokens.begin(),entry.repair_tokens.end(),error.found)!=entry.repair_tokens.end())
   propose(entry.repair.replace_id,report.offset+error.found.size(),error.expected);
 }
 if(entry.repair.kind==diagnostic::RepairKind::PrimitiveType&&token_matches&&error.found.size()<=16){
  for(const auto& name:entry.repair.replacements){
   std::vector<std::size_t> row(name.size()+1);
   for(std::size_t column=0;column<row.size();++column)row[column]=column;
   for(std::size_t line=0;line<error.found.size();++line){
    auto diagonal=row[0];row[0]=line+1;
    for(std::size_t column=1;column<row.size();++column){
     auto previous=row[column];
     row[column]=std::min({row[column]+1,row[column-1]+1,diagonal+(error.found[line]!=name[column-1])});
     diagonal=previous;
    }
   }
   if(row.back()&&row.back()<=entry.repair.maximum_distance)
    propose(diagnostic::expand(entry.repair.candidate_id,{.replacement=name}),
            report.offset+error.found.size(),name);
  }
 }
 if(!report.repairs.empty())report.diagnostic.notes.push_back(entry.repair.notice);
 return report;
}

Report describe(const diagnostic::Record& record,std::optional<std::string> source,const diagnostic::Catalog& catalog){
 Report report;report.offset=record.where.offset;report.presentation=catalog.presentation();report.level=record.level;
 report.diagnostic.id=record.id;report.diagnostic.message=record.message;report.diagnostic.severity=severity(record.level.kind);
 report.diagnostic.primary={record.where.file,narrow(record.where.line),narrow(record.where.column),narrow(record.where.line),narrow(record.where.column)};
 report.diagnostic.notes=record.notes;
 report.category=record.category;report.phase=record.phase;report.origin=record.origin;report.check=record.check;report.remediation=record.remediation;
 if(source&&source->size()<=max_source_bytes&&located(record.where,*source))report.source=std::move(source);
 return report;
}

Bundle describe(const diagnostic::Registry& registry,const diagnostic::Catalog& catalog,const SourceLookup& lookup){
 Bundle bundle;bundle.dropped=registry.dropped();bundle.presentation=catalog.presentation();
 bundle.diagnostics.reserve(registry.records().size());
 std::size_t retained_source_bytes=0;bool source_budget_exhausted=false;
 for(const auto& record:registry.records()){
  std::optional<std::string> source;
  if(lookup&&!source_budget_exhausted)try{source=lookup(record.where);}catch(const std::exception&){}
  if(source){
   const auto bytes=source->size();
   if(bytes>diagnostic::max_source_snapshot_bytes-retained_source_bytes){
    source.reset();source_budget_exhausted=true;
   }else{
    retained_source_bytes+=bytes;
    source_budget_exhausted=retained_source_bytes==diagnostic::max_source_snapshot_bytes;
   }
  }
  bundle.diagnostics.push_back(describe(record,std::move(source),catalog));
 }
 return bundle;
}

void validate(Report& report,const moonstone::syren::Validator& validator){
 using namespace moonstone;using namespace moonstone::syren;
 for(auto& candidate:report.repairs){
  bool valid=report.source.has_value()&&candidate.cost>=0&&
   !candidate.edits.empty()&&candidate.edits.size()<=16;
  std::size_t end=0,previous=std::numeric_limits<std::size_t>::max();
  for(const auto& edit:candidate.edits){
   if(!valid)break;
   valid=edit.begin>=end&&edit.begin!=previous&&edit.end>=edit.begin&&edit.end<=report.source->size()&&edit.replacement.size()<=max_source_bytes;
   previous=edit.begin;end=edit.end;
  }
  if(!valid){candidate.status=RepairStatus::Rejected;candidate.reason=report.presentation.invalid_edits;continue;}
  candidate=verify_repair(std::move(candidate),*report.source,[&](std::string_view changed){
   if(changed.size()>max_source_bytes)return ProofResult{Status::InvalidInput,report.presentation.source_limit,{},0};
   if(!validator)return ProofResult{Status::Unknown,report.presentation.missing_validator,{},0};
   try{return validator(changed);}
   catch(const std::exception& error){
    return ProofResult{Status::Unknown,diagnostic::expand(report.presentation.validator_failure,{.message=error.what()}),{},0};
   }
  });
 }
 auto rank=[](RepairStatus status){return status==RepairStatus::Validated?0:status==RepairStatus::Candidate?1:status==RepairStatus::Inconclusive?2:3;};
 std::stable_sort(report.repairs.begin(),report.repairs.end(),[&](const auto& left,const auto& right){return rank(left.status)<rank(right.status);});
 for(auto begin=report.repairs.begin();begin!=report.repairs.end();){
  auto end=std::find_if(begin,report.repairs.end(),[&](const auto& candidate){return candidate.status!=begin->status;});
  auto group=rank_repairs({begin,end});std::move(group.begin(),group.end(),begin);begin=end;
 }
}

std::string text(const Bundle& bundle,bool color){
 std::string out;const auto safe=[](std::string_view value){return diagnostic::terminal_text(value);};
 for(const auto& report:bundle.diagnostics){
  const auto& item=report.diagnostic;const auto& labels=report.presentation;
  auto shown=safe(report.level.name);
  if(color)shown="\x1b["+std::to_string(report.level.color)+"m"+shown+"\x1b[0m";
  out+=safe(item.primary.file)+":"+std::to_string(item.primary.line)+":"+std::to_string(item.primary.column)+": "+shown+": "+safe(item.message)+
   "\n  "+safe(labels.diagnostic_label)+": "+safe(item.id)+"\n";
  if(!report.category.empty())out+="  "+safe(labels.category_label)+": "+safe(report.category)+(report.phase.empty()?"":" ("+safe(report.phase)+")")+"\n";
  for(const auto& note:item.notes)out+="  "+safe(labels.note_label)+": "+safe(note)+"\n";
  if(!report.remediation.empty())out+="  "+safe(labels.help_label)+": "+safe(report.remediation)+"\n";
  for(const auto& candidate:report.repairs){
   out+="  "+safe(labels.repair_label)+"["+safe(candidate.id)+"] ("+safe(status_name(candidate.status,labels))+"): ";
   for(const auto& edit:candidate.edits)out+=safe(labels.bytes_label)+" "+std::to_string(edit.begin)+".."+std::to_string(edit.end)+" "+
    safe(labels.arrow_label)+" "+safe(quote(edit.replacement))+" ";
   if(!candidate.reason.empty())out+="("+safe(candidate.reason)+")";
   out+='\n';
  }
 }
 if(bundle.dropped)out+=safe(diagnostic::expand(presentation(bundle).dropped,{.count=std::to_string(bundle.dropped)}))+"\n";
 return out;
}
std::string text(const Report& report){
 Bundle bundle;bundle.diagnostics.push_back(report);bundle.presentation=report.presentation;return text(bundle,false);
}

std::string json(const Bundle& bundle){
 std::string out="{\"schema\":\"on1x.diagnostics/v1\",\"dropped\":"+std::to_string(bundle.dropped)+",\"diagnostics\":[";
 bool first=true;for(const auto& report:bundle.diagnostics){if(!first)out+=',';first=false;out+=report_object(report);}
 return out+"]}\n";
}
std::string json(const Report& report){
 Bundle bundle;bundle.diagnostics.push_back(report);bundle.presentation=report.presentation;return json(bundle);
}

std::string sarif(const Bundle& bundle){
 const auto& labels=presentation(bundle);
 std::string out="{\"version\":\"2.1.0\",\"$schema\":\"https://json.schemastore.org/sarif-2.1.0.json\",\"runs\":[{\"tool\":{\"driver\":{\"name\":"+
  quote(labels.sarif_tool)+"}},\"columnKind\":\"utf16CodeUnits\",\"results\":[";
 bool first=true;
 for(const auto& report:bundle.diagnostics){
  if(!first)out+=',';first=false;const auto& item=report.diagnostic;
  out+="{\"ruleId\":"+quote(item.id)+",\"level\":"+quote(report.level.sarif)+",\"message\":{\"text\":"+quote(item.message)+"}";
  if(!item.primary.file.empty()){
   out+=",\"locations\":[{\"physicalLocation\":{\"artifactLocation\":{\"uri\":"+quote(uri(item.primary.file))+
    "},\"region\":{\"startLine\":"+std::to_string(std::max(1u,item.primary.line));
   if(auto column=utf16_column(report))out+=",\"startColumn\":"+std::to_string(*column);
   out+="}}}]";
  }
  out+=",\"properties\":{\"category\":"+quote(report.category)+",\"phase\":"+quote(report.phase)+
   ",\"origin\":"+quote(report.origin)+",\"check\":"+quote(report.check)+
   ",\"remediation\":"+quote(report.remediation)+",\"on1xReport\":"+report_object(report)+"}}";
 }
 out+="],\"properties\":{\"droppedDiagnostics\":"+std::to_string(bundle.dropped)+"}}]}\n";
 return out;
}
std::string sarif(const Report& report){
 Bundle bundle;bundle.diagnostics.push_back(report);bundle.presentation=report.presentation;return sarif(bundle);
}
}
