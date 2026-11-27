#include <on1x/diagnostic.hpp>
#include "diagnostic_catalog.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace on1x::diagnostic {
namespace {
struct Json {
 enum class Kind { Null,Boolean,Number,String,Array,Object } kind=Kind::Null;
 std::string text;
 bool boolean=false;
 std::vector<Json> array;
 std::map<std::string,Json> object;
};

struct InvalidCatalog {
 Location where;
};

bool utf8(std::string_view value){
 for(std::size_t i=0;i<value.size();){
  auto c=static_cast<unsigned char>(value[i]);
  if(c<0x80){if(c==0)return false;++i;continue;}
  std::size_t n=c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
  if(!n||n>value.size()-i)return false;
  for(std::size_t j=1;j<n;++j)if((static_cast<unsigned char>(value[i+j])&0xc0)!=0x80)return false;
  auto next=static_cast<unsigned char>(value[i+1]);
  if((c==0xe0&&next<0xa0)||(c==0xed&&next>=0xa0)||(c==0xf0&&next<0x90)||(c==0xf4&&next>=0x90))return false;
  i+=n;
 }
 return true;
}
void byte_escape(std::string& out,unsigned char value){
 static constexpr char hex[]="0123456789abcdef";
 out+="\\x";out+=hex[value>>4];out+=hex[value&15];
}
void scalar_escape(std::string& out,unsigned value){
 static constexpr char hex[]="0123456789abcdef";
 out+="\\u{";
 const unsigned digits=value>0xffff?6:4;
 for(unsigned shift=(digits-1)*4;;shift-=4){
  out+=hex[(value>>shift)&15];
  if(!shift)break;
 }
 out+='}';
}
bool terminal_control(unsigned value){
 return value<=0x1f||(value>=0x7f&&value<=0x9f)||value==0x061c||
  value==0x200e||value==0x200f||(value>=0x202a&&value<=0x202e)||
  value==0x2028||value==0x2029||(value>=0x2066&&value<=0x2069)||value==0xfeff;
}

class Reader {
 std::string_view input_;
 std::size_t pos_=0,nodes_=0;
 Location at_;
 [[noreturn]] void fail(){
  at_.offset=pos_;at_.line=at_.column=1;
  for(std::size_t i=0;i<pos_;++i)if(input_[i]=='\n'){++at_.line;at_.column=1;}else ++at_.column;
  throw InvalidCatalog{at_};
 }
 void space(){while(pos_<input_.size()&&(input_[pos_]==' '||input_[pos_]=='\t'||input_[pos_]=='\r'||input_[pos_]=='\n'))++pos_;}
 bool accept(char c){space();if(pos_<input_.size()&&input_[pos_]==c){++pos_;return true;}return false;}
 void expect(char c){if(!accept(c))fail();}
 bool word(std::string_view value){
  space();if(input_.substr(pos_,value.size())!=value)return false;
  pos_+=value.size();return true;
 }
 unsigned codepoint(){
  unsigned value=0;
  for(unsigned i=0;i<4;++i){
   if(pos_==input_.size())fail();
   auto c=input_[pos_++];unsigned digit=c>='0'&&c<='9'?unsigned(c-'0'):c>='a'&&c<='f'?unsigned(c-'a'+10):c>='A'&&c<='F'?unsigned(c-'A'+10):16;
   if(digit==16)fail();
   value=value*16+digit;
  }
  return value;
 }
 static void append_utf8(std::string& out,unsigned value){
  if(value<0x80)out+=static_cast<char>(value);
  else if(value<0x800){out+=static_cast<char>(0xc0|(value>>6));out+=static_cast<char>(0x80|(value&63));}
  else if(value<0x10000){out+=static_cast<char>(0xe0|(value>>12));out+=static_cast<char>(0x80|((value>>6)&63));out+=static_cast<char>(0x80|(value&63));}
  else{out+=static_cast<char>(0xf0|(value>>18));out+=static_cast<char>(0x80|((value>>12)&63));out+=static_cast<char>(0x80|((value>>6)&63));out+=static_cast<char>(0x80|(value&63));}
 }
 std::string string(){
  expect('"');std::string out;
  while(pos_<input_.size()){
   auto c=static_cast<unsigned char>(input_[pos_++]);
   if(c=='"'){if(!utf8(out))fail();return out;}
   if(c<32)fail();
   if(c!='\\'){out+=static_cast<char>(c);if(out.size()>65536)fail();continue;}
   if(pos_==input_.size())fail();
   c=static_cast<unsigned char>(input_[pos_++]);
   if(c=='"'||c=='\\'||c=='/')out+=static_cast<char>(c);
   else if(c=='b')out+='\b';else if(c=='f')out+='\f';else if(c=='n')out+='\n';else if(c=='r')out+='\r';else if(c=='t')out+='\t';
   else if(c=='u'){
    unsigned value=codepoint();
    if(value>=0xd800&&value<=0xdbff){
     if(pos_+2>input_.size()||input_.substr(pos_,2)!="\\u")fail();
     pos_+=2;auto low=codepoint();if(low<0xdc00||low>0xdfff)fail();
     value=0x10000+(value-0xd800)*1024+low-0xdc00;
    }else if(value>=0xdc00&&value<=0xdfff)fail();
    if(!value)fail();
    append_utf8(out,value);
   }else fail();
   if(out.size()>65536)fail();
  }
  fail();
 }
 Json value(unsigned depth){
  space();if(depth>=64||++nodes_>65536||pos_==input_.size())fail();
  Json result;
  if(input_[pos_]=='{'){
   ++pos_;result.kind=Json::Kind::Object;if(accept('}'))return result;
   do{
    space();if(pos_==input_.size()||input_[pos_]!='"')fail();
    auto key=string();expect(':');auto child=value(depth+1);
    if(!result.object.emplace(std::move(key),std::move(child)).second)fail();
   }while(accept(','));expect('}');return result;
  }
  if(input_[pos_]=='['){
   ++pos_;result.kind=Json::Kind::Array;if(accept(']'))return result;
   do{result.array.push_back(value(depth+1));}while(accept(','));expect(']');return result;
  }
  if(input_[pos_]=='"'){result.kind=Json::Kind::String;result.text=string();return result;}
  if(word("true")){result.kind=Json::Kind::Boolean;result.boolean=true;return result;}
  if(word("false")){result.kind=Json::Kind::Boolean;return result;}
  if(word("null"))return result;
  auto begin=pos_;
  if(input_[pos_]=='-')++pos_;
  if(pos_==input_.size()||input_[pos_]<'0'||input_[pos_]>'9')fail();
  if(input_[pos_]=='0')++pos_;
  else while(pos_<input_.size()&&input_[pos_]>='0'&&input_[pos_]<='9')++pos_;
  if(pos_<input_.size()&&(input_[pos_]=='.'||input_[pos_]=='e'||input_[pos_]=='E'))fail();
  result.kind=Json::Kind::Number;result.text=std::string(input_.substr(begin,pos_-begin));return result;
 }
public:
 Reader(std::string_view input,const std::string& origin):input_(input){at_.file=origin;if(input.size()>1024*1024)fail();}
 Json read(){auto result=value(0);space();if(pos_!=input_.size())fail();return result;}
};

[[noreturn]] void invalid(const std::string& origin){throw InvalidCatalog{Location{origin}};}
const Json& member(const Json& value,std::string_view key,const std::string& origin){
 if(value.kind!=Json::Kind::Object)invalid(origin);
 auto found=value.object.find(std::string(key));if(found==value.object.end())invalid(origin);return found->second;
}
const Json* optional(const Json& value,std::string_view key,const std::string& origin){
 if(value.kind!=Json::Kind::Object)invalid(origin);
 auto found=value.object.find(std::string(key));return found==value.object.end()?nullptr:&found->second;
}
std::string string_member(const Json& value,std::string_view key,const std::string& origin,bool required=true){
 auto field=optional(value,key,origin);if(!field){if(required)invalid(origin);return {};}
 if(field->kind!=Json::Kind::String)invalid(origin);
 return field->text;
}
bool bool_member(const Json& value,std::string_view key,const std::string& origin){
 const auto& field=member(value,key,origin);if(field.kind!=Json::Kind::Boolean)invalid(origin);return field.boolean;
}
std::size_t natural_member(const Json& value,std::string_view key,const std::string& origin){
 const auto& field=member(value,key,origin);std::size_t result=0;
 if(field.kind!=Json::Kind::Number||field.text.empty()||field.text[0]=='-')invalid(origin);
 auto [end,ec]=std::from_chars(field.text.data(),field.text.data()+field.text.size(),result);
 if(ec!=std::errc{}||end!=field.text.data()+field.text.size())invalid(origin);
 return result;
}
void shape(const Json& value,const std::set<std::string>& required,const std::set<std::string>& allowed,const std::string& origin){
 if(value.kind!=Json::Kind::Object)invalid(origin);
 for(const auto& key:required)if(!value.object.contains(key))invalid(origin);
 for(const auto& [key,child]:value.object){(void)child;if(!required.contains(key)&&!allowed.contains(key))invalid(origin);}
}
std::vector<std::string> strings(const Json& value,std::string_view field,const std::string& origin,std::size_t limit,bool allow_empty=false){
 const auto& list=member(value,field,origin);
 if(list.kind!=Json::Kind::Array||list.array.size()>limit)invalid(origin);
 std::vector<std::string> result;std::set<std::string> unique;
 for(const auto& item:list.array){
  if(item.kind!=Json::Kind::String||(!allow_empty&&item.text.empty())||item.text.size()>128||!unique.insert(item.text).second)invalid(origin);
  result.push_back(item.text);
 }
 return result;
}
bool diagnostic_id(std::string_view id){
 if(id.empty()||id.size()>128||id.front()<'A'||id.front()>'Z'||id.back()=='-')return false;
 bool hyphen=false;
 for(char c:id){
  if(c=='-'){if(hyphen)return false;hyphen=true;}
  else {if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')))return false;hyphen=false;}
 }
 return true;
}
bool word_name(std::string_view name){
 if(name.empty()||name.size()>64)return false;
 for(char c:name)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return false;
 return true;
}
bool template_text(std::string_view value){
 if(value.empty()||value.size()>4096)return false;
 static const std::set<std::string_view> fields={"message","expected","found","replacement","count"};
 for(std::size_t i=0;i<value.size();++i){
  if(value[i]=='}')return false;
  if(value[i]!='{')continue;
  auto end=value.find('}',i+1);if(end==std::string_view::npos||!fields.contains(value.substr(i+1,end-i-1)))return false;
  i=end;
 }
 return true;
}
std::size_t record_bytes(const Record& record){
 std::size_t total=sizeof(Record);
 auto add=[&](const std::string& value){if(value.size()>std::numeric_limits<std::size_t>::max()-total)throw std::length_error(std::string{});total+=value.size();};
 add(record.id);add(record.message);add(record.category);add(record.phase);add(record.origin);add(record.check);add(record.remediation);
 add(record.name);add(record.group);
 add(record.level.name);add(record.level.sarif);add(record.where.file);
 for(const auto& note:record.notes)add(note);
 return total;
}
template<class Enum> Enum enum_value(std::size_t value,std::size_t count,const std::string& origin){
 if(value>=count)invalid(origin);
 return static_cast<Enum>(value);
}
std::string value_for(std::string_view field,const TemplateArguments& args){
 if(field=="message")return std::string(args.message);
 if(field=="expected")return std::string(args.expected);
 if(field=="found")return std::string(args.found);
 if(field=="replacement")return std::string(args.replacement);
 if(field=="count")return std::string(args.count);
 throw std::invalid_argument(std::string{});
}
}

std::string expand(std::string_view text,const TemplateArguments& args){
 std::string out;
 for(std::size_t i=0;i<text.size();++i){
  if(text[i]!='{'){out+=text[i];continue;}
  auto end=text.find('}',i+1);if(end==std::string_view::npos)throw std::invalid_argument(std::string{});
  out+=value_for(text.substr(i+1,end-i-1),args);i=end;
  if(out.size()>16384)throw std::length_error(std::string{});
 }
 return out;
}

std::string terminal_text(std::string_view text){
 std::string out;
 for(std::size_t i=0;i<text.size();){
  const auto first=static_cast<unsigned char>(text[i]);
  if(first<0x80){
   if(first=='\n')out+="\\n";
   else if(first=='\r')out+="\\r";
   else if(first=='\t')out+="\\t";
   else if(first=='\b')out+="\\b";
   else if(first=='\f')out+="\\f";
   else if(terminal_control(first))byte_escape(out,first);
   else out+=static_cast<char>(first);
   ++i;continue;
  }
  const std::size_t count=first>=0xc2&&first<=0xdf?2:first>=0xe0&&first<=0xef?3:first>=0xf0&&first<=0xf4?4:0;
  bool valid=count&&count<=text.size()-i;
  for(std::size_t j=1;valid&&j<count;++j)valid=(static_cast<unsigned char>(text[i+j])&0xc0)==0x80;
  if(valid){
   const auto next=static_cast<unsigned char>(text[i+1]);
   valid=!((first==0xe0&&next<0xa0)||(first==0xed&&next>=0xa0)||(first==0xf0&&next<0x90)||(first==0xf4&&next>=0x90));
  }
  if(!valid){byte_escape(out,first);++i;continue;}
  unsigned value=first&((1u<<(7-count))-1);
  for(std::size_t j=1;j<count;++j)value=(value<<6)|(static_cast<unsigned char>(text[i+j])&0x3f);
  if(terminal_control(value))scalar_escape(out,value);else out.append(text.substr(i,count));
  i+=count;
 }
 return out;
}

Catalog Catalog::parse_trusted(std::string_view input,const std::string& origin){
 auto root=Reader(input,origin).read();
 shape(root,{"schema","levels","providers","repair_policies","presentation","default_scrutiny","scrutiny","diagnostics"},{},origin);
 if(string_member(root,"schema",origin)!="on1x.diagnostic-catalog/v2")invalid(origin);
 Catalog result;

 const auto& levels=member(root,"levels",origin);
 if(levels.kind!=Json::Kind::Array||levels.array.size()!=3)invalid(origin);
 std::set<std::size_t> level_kinds;
 for(const auto& item:levels.array){
  shape(item,{"name","kind","failure","sarif","color"},{},origin);
  Level level;level.name=string_member(item,"name",origin);level.sarif=string_member(item,"sarif",origin);
  auto kind=natural_member(item,"kind",origin);level.kind=enum_value<Severity>(kind,3,origin);
  level.failure=bool_member(item,"failure",origin);auto color=natural_member(item,"color",origin);
  if(!word_name(level.name)||!word_name(level.sarif)||color>107||!level_kinds.insert(kind).second)invalid(origin);
  level.color=static_cast<unsigned>(color);
  if(!result.levels_.emplace(level.name,std::move(level)).second)invalid(origin);
 }

 const auto& providers=member(root,"providers",origin);
 if(providers.kind!=Json::Kind::Array||providers.array.size()!=3)invalid(origin);
 std::set<std::size_t> provider_kinds;
 for(const auto& item:providers.array){
  shape(item,{"name","kind","origin"},{},origin);
  Provider provider;provider.name=string_member(item,"name",origin);provider.origin=string_member(item,"origin",origin);
  auto kind=natural_member(item,"kind",origin);provider.kind=enum_value<ProviderKind>(kind,3,origin);
  if(!word_name(provider.name)||!word_name(provider.origin)||!provider_kinds.insert(kind).second)invalid(origin);
  if(!result.providers_.emplace(provider.name,std::move(provider)).second)invalid(origin);
 }

 const auto& repairs=member(root,"repair_policies",origin);
 if(repairs.kind!=Json::Kind::Array||repairs.array.size()!=4)invalid(origin);
 std::set<std::size_t> repair_kinds;
 for(const auto& item:repairs.array){
  shape(item,{"name","kind","candidate_description","notice"},
        {"insert_id","replace_id","end_marker","candidate_id","maximum_distance",
         "replacements","rationale","proposal"},origin);
  RepairPolicy repair;repair.name=string_member(item,"name",origin);
  auto kind=natural_member(item,"kind",origin);repair.kind=enum_value<RepairKind>(kind,4,origin);
  repair.candidate_description=string_member(item,"candidate_description",origin);
  repair.notice=string_member(item,"notice",origin);
  repair.insert_id=string_member(item,"insert_id",origin,false);repair.replace_id=string_member(item,"replace_id",origin,false);
  repair.end_marker=string_member(item,"end_marker",origin,false);
  repair.candidate_id=string_member(item,"candidate_id",origin,false);
  repair.rationale=string_member(item,"rationale",origin,false);repair.proposal=string_member(item,"proposal",origin,false);
  if(!word_name(repair.name)||!repair_kinds.insert(kind).second||
     !template_text(repair.candidate_description)||!template_text(repair.notice))invalid(origin);
  if(repair.kind==RepairKind::Token){
   if(!word_name(repair.insert_id)||!word_name(repair.replace_id)||repair.end_marker.empty()||
      repair.end_marker.size()>128||
      !repair.candidate_id.empty()||!repair.rationale.empty()||!repair.proposal.empty()||
      optional(item,"maximum_distance",origin)||optional(item,"replacements",origin))invalid(origin);
  }else if(repair.kind==RepairKind::PrimitiveType){
   repair.maximum_distance=natural_member(item,"maximum_distance",origin);
   repair.replacements=strings(item,"replacements",origin,32);
   if(!template_text(repair.candidate_id)||!repair.maximum_distance||repair.maximum_distance>8||
      repair.replacements.empty()||!repair.insert_id.empty()||!repair.replace_id.empty()||
      !repair.end_marker.empty()||!repair.rationale.empty()||!repair.proposal.empty())invalid(origin);
  }else if(repair.kind==RepairKind::Advisory){
   if(!template_text(repair.rationale)||!template_text(repair.proposal)||!repair.insert_id.empty()||
      !repair.replace_id.empty()||!repair.end_marker.empty()||!repair.candidate_id.empty()||
      optional(item,"maximum_distance",origin)||optional(item,"replacements",origin))invalid(origin);
  }else if(!repair.insert_id.empty()||!repair.replace_id.empty()||!repair.end_marker.empty()||
           !repair.candidate_id.empty()||!repair.rationale.empty()||!repair.proposal.empty()||
           optional(item,"maximum_distance",origin)||optional(item,"replacements",origin))invalid(origin);
  if(!result.repairs_.emplace(repair.name,std::move(repair)).second)invalid(origin);
 }

 const auto& presentation=member(root,"presentation",origin);
 shape(presentation,{"command_errors","repair_status","diagnostic_label","category_label","note_label","help_label","repair_label",
  "bytes_label","arrow_label","dropped","invalid_edits","source_limit","missing_validator","validator_failure",
  "compiler_validated","source_ppx_unavailable","ppx_client_failure","already_valid","ambiguous_repairs","sarif_tool"},{},origin);
 auto statuses=strings(presentation,"repair_status",origin,4);
 if(statuses.size()!=4)invalid(origin);
 std::copy(statuses.begin(),statuses.end(),result.presentation_.repair_status.begin());
 const auto& commands=member(presentation,"command_errors",origin);
 if(commands.kind!=Json::Kind::Array||
    commands.array.size()!=result.presentation_.command_errors.size())invalid(origin);
 for(std::size_t i=0;i<commands.array.size();++i){
  if(commands.array[i].kind!=Json::Kind::String||!template_text(commands.array[i].text))invalid(origin);
  result.presentation_.command_errors[i]=commands.array[i].text;
 }
 auto assign=[&](std::string Presentation::*target,std::string_view key){
  result.presentation_.*target=string_member(presentation,key,origin);
  if(!template_text(result.presentation_.*target))invalid(origin);
 };
 assign(&Presentation::diagnostic_label,"diagnostic_label");assign(&Presentation::category_label,"category_label");
 assign(&Presentation::note_label,"note_label");assign(&Presentation::help_label,"help_label");
 assign(&Presentation::repair_label,"repair_label");assign(&Presentation::bytes_label,"bytes_label");
 assign(&Presentation::arrow_label,"arrow_label");assign(&Presentation::dropped,"dropped");
 assign(&Presentation::invalid_edits,"invalid_edits");assign(&Presentation::source_limit,"source_limit");
 assign(&Presentation::missing_validator,"missing_validator");assign(&Presentation::validator_failure,"validator_failure");
 assign(&Presentation::compiler_validated,"compiler_validated");assign(&Presentation::source_ppx_unavailable,"source_ppx_unavailable");
 assign(&Presentation::ppx_client_failure,"ppx_client_failure");assign(&Presentation::already_valid,"already_valid");
 assign(&Presentation::ambiguous_repairs,"ambiguous_repairs");assign(&Presentation::sarif_tool,"sarif_tool");

 result.default_scrutiny_=string_member(root,"default_scrutiny",origin);
 const auto& policies=member(root,"scrutiny",origin);
 if(policies.kind!=Json::Kind::Array||policies.array.empty()||policies.array.size()>32)invalid(origin);
 std::set<std::size_t> ranks;
 for(const auto& item:policies.array){
  shape(item,{"name","rank","enabled","checks","severity_overrides"},{},origin);
  ScrutinyPolicy policy;policy.name=string_member(item,"name",origin);policy.rank=natural_member(item,"rank",origin);
  policy.enabled=bool_member(item,"enabled",origin);
  if(!word_name(policy.name)||policy.rank>31||!ranks.insert(policy.rank).second)invalid(origin);
  for(auto& check:strings(item,"checks",origin,32)){if(!word_name(check))invalid(origin);policy.checks.insert(std::move(check));}
  const auto& overrides=member(item,"severity_overrides",origin);
  if(overrides.kind!=Json::Kind::Array||overrides.array.size()>8)invalid(origin);
  for(const auto& change:overrides.array){
   shape(change,{"from","to"},{},origin);auto from=string_member(change,"from",origin),to=string_member(change,"to",origin);
   if(!result.levels_.contains(from)||!result.levels_.contains(to)||!policy.severity_overrides.emplace(std::move(from),std::move(to)).second)invalid(origin);
  }
  if(!result.scrutiny_.emplace(policy.name,std::move(policy)).second)invalid(origin);
 }
 if(!result.scrutiny_.contains(result.default_scrutiny_))invalid(origin);
 for(const auto& [left_name,left]:result.scrutiny_)for(const auto& [right_name,right]:result.scrutiny_){
  (void)left_name;(void)right_name;
  if(left.rank<right.rank&&!std::includes(right.checks.begin(),right.checks.end(),left.checks.begin(),left.checks.end()))invalid(origin);
 }

 const auto& entries=member(root,"diagnostics",origin);
 if(entries.kind!=Json::Kind::Array||entries.array.empty()||entries.array.size()>512)invalid(origin);
 std::set<std::pair<std::string,std::size_t>> wetstone_keys;
 std::set<std::string> ppx_processors;
 for(const auto& item:entries.array){
  shape(item,{"id","role","provider","level","message","category","phase","guidance","remediation","repair"},
        {"match","repair_tokens","upstream","check","minimum_scrutiny","processor"},origin);
  CatalogEntry entry;
  entry.id=string_member(item,"id",origin);entry.match=string_member(item,"match",origin,false);
  entry.message=string_member(item,"message",origin);entry.category=string_member(item,"category",origin);
  entry.phase=string_member(item,"phase",origin);entry.guidance=string_member(item,"guidance",origin);
  entry.remediation=string_member(item,"remediation",origin);entry.check=string_member(item,"check",origin,false);
  entry.minimum_scrutiny=string_member(item,"minimum_scrutiny",origin,false);
  entry.ppx_processor=string_member(item,"processor",origin,false);
  auto role=natural_member(item,"role",origin);entry.role=enum_value<Role>(role,6,origin);
  auto provider_name=string_member(item,"provider",origin),level_name=string_member(item,"level",origin),repair_name=string_member(item,"repair",origin);
  auto provider=result.providers_.find(provider_name);auto level=result.levels_.find(level_name);auto repair=result.repairs_.find(repair_name);
  if(provider==result.providers_.end()||level==result.levels_.end()||repair==result.repairs_.end())invalid(origin);
  entry.provider=provider->second;entry.level=level->second;entry.repair=repair->second;
  if(!diagnostic_id(entry.id)||(!entry.match.empty()&&!diagnostic_id(entry.match))||
     !template_text(entry.message)||!word_name(entry.category)||!word_name(entry.phase)||
     !template_text(entry.guidance)||!template_text(entry.remediation))invalid(origin);
  if(auto token_field=optional(item,"repair_tokens",origin)){
   (void)token_field;entry.repair_tokens=strings(item,"repair_tokens",origin,32);
  }
  if((entry.repair.kind==RepairKind::Token)!=!entry.repair_tokens.empty())invalid(origin);
  if(entry.role!=Role::None){
   if(entry.provider.kind!=ProviderKind::Compiler||!entry.match.empty()||!entry.check.empty()||
      !entry.minimum_scrutiny.empty()||!entry.ppx_processor.empty()||optional(item,"upstream",origin))invalid(origin);
   if(entry.role!=Role::SourceFallback&&expand(entry.message).empty())invalid(origin);
   if(!result.roles_.emplace(role,entry.id).second)invalid(origin);
  }
  if(entry.provider.kind==ProviderKind::Compiler){
   if(optional(item,"upstream",origin)||!entry.check.empty()||!entry.minimum_scrutiny.empty()||!entry.ppx_processor.empty())invalid(origin);
  }else if(entry.provider.kind==ProviderKind::Wetstone){
   if(entry.role!=Role::None||!entry.match.empty()||!entry.ppx_processor.empty()||!word_name(entry.check)||
      !result.scrutiny_.contains(entry.minimum_scrutiny))invalid(origin);
   const auto& upstream=member(item,"upstream",origin);shape(upstream,{"rule","level","adapter"},{},origin);
   entry.source_rule=string_member(upstream,"rule",origin);
   auto source_level=natural_member(upstream,"level",origin),adapter=natural_member(upstream,"adapter",origin);
   entry.source_level=enum_value<UpstreamLevel>(source_level,2,origin);
   entry.adapter=enum_value<AnalysisAdapter>(adapter,3,origin);entry.has_upstream=true;
   if(entry.source_rule.empty()||entry.source_rule.size()>128||!wetstone_keys.emplace(entry.source_rule,source_level).second)invalid(origin);
  }else{
   if(entry.role!=Role::None||!entry.match.empty()||optional(item,"upstream",origin)||!entry.check.empty()||
      !entry.minimum_scrutiny.empty()||!word_name(entry.ppx_processor)||!ppx_processors.insert(entry.ppx_processor).second)invalid(origin);
  }
  auto [inserted,unique]=result.entries_.emplace(entry.id,std::move(entry));if(!unique)invalid(origin);
  auto& stored=inserted->second;
  if(!stored.match.empty()&&!result.compiler_.emplace(stored.match,stored.id).second)invalid(origin);
 }
 for(unsigned role=1;role<6;++role)if(!result.roles_.contains(role))invalid(origin);
 return result;
}

Catalog Catalog::parse(std::string_view input,const std::string& origin){
 try{return parse_trusted(input,origin);}
 catch(const InvalidCatalog& invalid){throw embedded().failure(Role::CatalogFailure,invalid.where);}
}

Catalog Catalog::load(const std::filesystem::path& path){
 try{
  std::error_code ec;
  if(!std::filesystem::is_regular_file(path,ec)||ec)throw InvalidCatalog{Location{path.string()}};
  auto size=std::filesystem::file_size(path,ec);if(ec||size>1024*1024)throw InvalidCatalog{Location{path.string()}};
  std::ifstream stream(path,std::ios::binary);if(!stream)throw InvalidCatalog{Location{path.string()}};
  std::string text(static_cast<std::size_t>(size),'\0');
  if(size&&!stream.read(text.data(),static_cast<std::streamsize>(size)))throw InvalidCatalog{Location{path.string()}};
  char extra=0;if(stream.get(extra))throw InvalidCatalog{Location{path.string()}};
  return parse_trusted(text,path.string());
 }catch(const InvalidCatalog& invalid){throw embedded().failure(Role::CatalogFailure,invalid.where);}
 catch(const std::exception&){throw embedded().failure(Role::CatalogFailure,Location{path.string()});}
}

const Catalog& Catalog::embedded(){
 static const Catalog catalog=[]{
  try{return parse_trusted(embedded_diagnostic_catalog,embedded_diagnostic_catalog_origin);}
  catch(...){std::terminate();}
 }();
 return catalog;
}

const CatalogEntry* Catalog::find(std::string_view id)const{
 auto found=entries_.find(std::string(id));return found==entries_.end()?nullptr:&found->second;
}
const CatalogEntry& Catalog::role(Role role_value)const{
 return entries_.at(roles_.at(static_cast<unsigned>(role_value)));
}
const CatalogEntry& Catalog::compiler(std::string_view legacy_id)const{
 auto found=compiler_.find(std::string(legacy_id));
 if(found!=compiler_.end())return entries_.at(found->second);
 if(auto direct=find(legacy_id))return *direct;
 return role(Role::SourceFallback);
}
const CatalogEntry* Catalog::wetstone(std::string_view rule_value,UpstreamLevel level_value)const{
 for(const auto& [id,entry]:entries_){
  (void)id;
  if(entry.provider.kind==ProviderKind::Wetstone&&entry.source_rule==rule_value&&entry.source_level==level_value)return &entry;
 }
 return nullptr;
}
const CatalogEntry* Catalog::ppx(std::string_view id)const{
 auto entry=find(id);return entry&&entry->provider.kind==ProviderKind::Ppx?entry:nullptr;
}
std::optional<std::string> Catalog::ppx_id(std::string_view processor)const{
 for(const auto& [id,entry]:entries_)
  if(entry.provider.kind==ProviderKind::Ppx&&entry.ppx_processor==processor)return id;
 std::optional<std::string> only;
 for(const auto& [id,entry]:entries_)if(entry.provider.kind==ProviderKind::Ppx){
  if(only)return {};
  only=id;
 }
 return only;
}
const ScrutinyPolicy& Catalog::scrutiny(std::string_view name,const Location& at)const{
 auto found=scrutiny_.find(std::string(name));if(found==scrutiny_.end())throw failure(Role::ScrutinyFailure,at);return found->second;
}
const Level& Catalog::level(std::string_view name)const{return levels_.at(std::string(name));}
Level Catalog::level(const ScrutinyPolicy& policy,const Level& source)const{
 auto found=policy.severity_overrides.find(source.name);return found==policy.severity_overrides.end()?source:level(found->second);
}
bool Catalog::selected(const ScrutinyPolicy& policy,const CatalogEntry& entry)const{
 if(!policy.enabled||entry.provider.kind!=ProviderKind::Wetstone)return false;
 auto minimum=scrutiny_.find(entry.minimum_scrutiny);
 return minimum!=scrutiny_.end()&&policy.rank>=minimum->second.rank&&policy.checks.contains(entry.check);
}
std::set<AnalysisAdapter> Catalog::adapters(const ScrutinyPolicy& policy)const{
 std::set<AnalysisAdapter> out;
 for(const auto& [id,entry]:entries_){(void)id;if(selected(policy,entry))out.insert(entry.adapter);}
 return out;
}
bool Catalog::advisory_repairs(const ScrutinyPolicy& policy)const{
 return std::any_of(entries_.begin(),entries_.end(),[&](const auto& item){
  return selected(policy,item.second)&&item.second.repair.kind==RepairKind::Advisory;
 });
}
std::string Catalog::render(const CatalogEntry& entry,const TemplateArguments& args)const{return expand(entry.message,args);}
Record Catalog::record(const CatalogEntry& entry,Location where,const TemplateArguments& args)const{
 Record record;record.id=entry.id;record.message=render(entry,args);record.where=std::move(where);record.level=entry.level;
 record.category=entry.category;record.phase=entry.phase;record.origin=entry.provider.origin;record.check=entry.check;
 record.remediation=entry.remediation;record.notes.push_back(entry.guidance);return record;
}
Record Catalog::record(const Error& error)const{
 if(error.diagnostic_id=="ON1X-IMPORT-ALIAS"&&!find(error.diagnostic_id))
  return embedded().record(error);
 const auto& entry=compiler(error.diagnostic_id);
 try{
  auto bounded=record(entry,error.where,{error.what(),error.expected,error.found,{},{}});
  if(error.diagnostic_id=="ON1X-IMPORT-ALIAS"){
   bounded.name="invalid-import-alias";bounded.group="imports";
  }
  if(bounded.message.empty())throw std::length_error(std::string{});
  return bounded;
 }
 catch(const std::length_error&){
  const auto& trusted=embedded();
  return trusted.record(trusted.role(Role::ReportingFailure),error.where);
 }
}
Error Catalog::failure(Role role_value,Location where)const{
 const auto& entry=role(role_value);return Error(std::move(where),render(entry),entry.id);
}
std::string Catalog::command_failure(CommandFailure kind,const TemplateArguments& args)const{
 return expand(presentation_.command_errors.at(static_cast<std::size_t>(kind)),args);
}

Registry::Registry(std::size_t max_count,std::size_t max_bytes):max_count_(max_count),max_bytes_(max_bytes){
 if(!max_count||max_count>65536||!max_bytes||max_bytes>16*1024*1024)
  throw std::invalid_argument(std::string{});
}
bool Registry::emit(Record record){
 if(!diagnostic_id(record.id)||record.message.empty()||record.message.size()>16384||
    record.where.file.size()>4096||record.where.offset>max_source_bytes||
    !record.where.line||!record.where.column||record.where.line>max_source_bytes+1||record.where.column>max_source_bytes+1||
    record.notes.size()>32||(!record.name.empty()&&!word_name(record.name))||
    (!record.group.empty()&&!word_name(record.group))||!word_name(record.level.name)||!word_name(record.level.sarif)||
    record.level.color>107||static_cast<unsigned>(record.level.kind)>2)
  throw Catalog::embedded().failure(Role::ProtocolFailure,record.where);
 for(const auto& note:record.notes)if(note.size()>4096)throw Catalog::embedded().failure(Role::ProtocolFailure,record.where);
 auto key=record.id+'\0'+record.where.file+'\0'+std::to_string(record.where.offset)+'\0'+record.message;
 if(!keys_.insert(key).second)return false;
 auto bytes=record_bytes(record);
 if(records_.size()>=max_count_||bytes>max_bytes_-std::min(max_bytes_,bytes_)){
  keys_.erase(key);++dropped_;return false;
 }
 bytes_+=bytes;records_.push_back(std::move(record));return true;
}
bool Registry::has_errors()const noexcept{
 return std::any_of(records_.begin(),records_.end(),[](const auto& record){return record.level.failure;});
}

std::string text(const Registry& registry,const Catalog& catalog,bool color){
 std::string out;const auto& presentation=catalog.presentation();
 const auto safe=[](std::string_view value){return terminal_text(value);};
 for(const auto& record:registry.records()){
  auto shown=safe(record.level.name);
  if(color)shown="\x1b["+std::to_string(record.level.color)+"m"+shown+"\x1b[0m";
  out+=safe(record.where.file)+":"+std::to_string(record.where.line)+":"+std::to_string(record.where.column)+": "+
   shown+": "+safe(record.message)+"\n  "+safe(presentation.diagnostic_label)+": "+safe(record.id)+"\n";
  if(!record.category.empty())out+="  "+safe(presentation.category_label)+": "+safe(record.category)+
   (record.phase.empty()?"":" ("+safe(record.phase)+")")+"\n";
  for(const auto& note:record.notes)out+="  "+safe(presentation.note_label)+": "+safe(note)+"\n";
  if(!record.remediation.empty())out+="  "+safe(presentation.help_label)+": "+safe(record.remediation)+"\n";
 }
 if(registry.dropped())out+=safe(expand(presentation.dropped,{.count=std::to_string(registry.dropped())}))+"\n";
 return out;
}
}
