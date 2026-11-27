#include "tables.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <set>
#include <tuple>

namespace on1x::stdffi {
namespace {
constexpr std::size_t max_spec_bytes=65536;
constexpr std::size_t max_spec_items=128;
constexpr std::string_view whitespace=" \t\r\n";
constexpr std::string_view letters="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
constexpr std::string_view digits="0123456789";

struct Form {
 std::string_view word;
 std::vector<Form> items;
};

class SpecReader {
 std::string_view text;
 const Location& at;
 std::string filename;
 std::size_t nodes=0;

 char peek(){
  while(!text.empty()){
   if(whitespace.find(text.front())!=std::string_view::npos)text.remove_prefix(1);
   else if(text.front()=='#'||text.front()==';'){
    auto end=text.find_first_of("\r\n");
    text.remove_prefix(end==std::string_view::npos?text.size():end);
   }else break;
  }
  return text.empty()?'\0':text.front();
 }

 std::string_view atom(){
  auto c=peek();
  if(c=='\0'||c=='('||c==')')fail("expected a recipe atom");
  auto word=text.substr(0,text.find_first_of(" \t\r\n();#"));
  for(unsigned char ch:word)
   if(ch<=32||ch>=127)fail("invalid recipe atom");
  text.remove_prefix(word.size());
  return word;
 }

 std::vector<Form> list(std::size_t depth){
  if(depth>40||peek()!='(')fail("expected a list within the recipe depth limit");
  text.remove_prefix(1);
  std::vector<Form> result;
  while(peek()!=')'){
   if(++nodes>16384)fail("recipe node limit exceeded");
   if(peek()=='(')result.push_back({{},list(depth+1)});
   else result.push_back({atom(),{}});
  }
  text.remove_prefix(1);
  return result;
 }

public:
 SpecReader(std::string_view bytes,const Location& where,std::string file)
  :text(bytes),at(where),filename(std::move(file)){}
 [[noreturn]] void fail(std::string why,const char* code="ON1X-STDFFI-SPEC") const {
  throw Error(at,"stdffi file '"+filename+"': "+why,code);
 }
 std::vector<Form> document(){
  const bool oversized=text.size()>max_spec_bytes;
  if(peek()!='(')fail("expected (ffispec 1 ...)");
  // Read only the prologue of a catalogue: the rest is not our language.
  auto original=text;
  text.remove_prefix(1);
  auto format=atom();
  auto version=atom();
  if(format=="ffi-catalogue"&&version=="1")
   fail("ffi-catalogue 1 is header metadata, not an executable ffispec","ON1X-STDFFI-CATALOGUE");
  if(oversized)fail("recipe byte limit exceeded");
  if(format!="ffispec"||version!="1")fail("expected (ffispec 1 ...)");
  text=original;
  auto result=list(0);
  if(peek()!='\0'||!text.empty())fail("trailing input after recipe");
  return result;
 }
};

std::string_view word(const Form& form){return form.word;} // Lists have no atom spelling.
std::string_view tag(const Form& form){return form.items.empty()?std::string_view{}:word(form.items[0]);}
bool identifier(std::string_view name){
 if(name.empty()||name.size()>64||letters.find(name.front())==letters.npos)return false;
 return std::all_of(name.begin()+1,name.end(),[](char ch){return letters.find(ch)!=letters.npos||digits.find(ch)!=digits.npos;});
}
bool selector(std::string_view name){
 if(name=="*")return true;
 constexpr std::string_view chars="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789";
 return !name.empty()&&name.size()<=128&&chars.find(name.front())!=chars.npos&&
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789.-")==name.npos;
}

bool matches_expression(const Form& form,std::size_t index,const std::vector<std::string_view>& params){
 const auto& n=tables::nodes.at(index);
 using enum tables::Code;
 std::string_view op;
 switch(n.op){
  case Bool:return word(form)==(n.immediate?"true":"false");
  case Param:return word(form)==params.at(static_cast<std::size_t>(n.immediate));
  case Integer:{
   auto spelling=word(form);
   if(spelling.empty())return false;
   auto magnitude=spelling.front()=='-'?spelling.substr(1):spelling;
   if(magnitude.size()>1&&magnitude.front()=='0')return false;
   std::int64_t value=0;
   const auto* end=spelling.data()+spelling.size();
   auto parsed=std::from_chars(spelling.data(),end,value);
   return parsed.ec==std::errc{}&&parsed.ptr==end&&value==n.immediate&&
          std::find(params.begin(),params.end(),spelling)==params.end();
  }
  case Lt:op="lt";break;
  case Le:op="le";break;
  case Gt:op="gt";break;
  case Ge:op="ge";break;
  case Eq:op="eq";break;
  case And:op="and";break;
  case Or:op="or";break;
  case Add:op="add";break;
  case Sub:op="sub";break;
  case If:op="if";break;
 }
 return !op.empty()&&tag(form)==op&&form.items.size()==(n.op==If?4u:3u)&&
        matches_expression(form.items[1],n.a,params)&&matches_expression(form.items[2],n.b,params)&&
        (n.op!=If||matches_expression(form.items[3],n.c,params));
}

struct Selectors {
 std::string_view backend,target;
 void apply(const Form& field){
  auto value=word(field.items[1]);
  (tag(field)=="backend"?backend:target)=value=="*"?std::string_view{}:value;
 }
};

const Form& get(const std::map<std::string_view,const Form*>& fields,std::string_view key){
 static const Form absent;
 auto it=fields.find(key);
 return it==fields.end()?absent:*it->second;
}

std::string_view value(const Form& field){return field.items.size()==2?word(field.items[1]):std::string_view{};}

// Directive syntax does not grant authority: require the exact effective set
// in the compiled recipe, independent of spelling/order of equivalent scopes.
bool matches_directives(const std::vector<const Form*>& options,Selectors inherited,
                        std::span<const tables::Directive> expected){
 using Entry=std::tuple<std::string_view,std::string_view,std::string_view,std::string_view,bool>;
 std::set<Entry> actual;
 for(const auto* field:options){
  const auto& args=field->items;
  if(args.size()<2||word(args[1]).empty())return false;
  auto selected=inherited;
  std::set<std::string_view> seen;
  for(std::size_t i=2;i<args.size();++i){
   auto kind=tag(args[i]);
   if(!seen.insert(kind).second)return false;
   if(kind=="speculative"&&args[i].items.size()==1)continue;
   if((kind!="backend"&&kind!="target")||!selector(value(args[i])))return false;
   selected.apply(args[i]);
  }
  if(!actual.emplace(selected.backend,selected.target,tag(*field),word(args[1]),
                     seen.contains("speculative")).second)return false;
 }
 return actual.size()==expected.size()&&std::all_of(expected.begin(),expected.end(),[&](const auto& d){
  return actual.contains(Entry{d.backend,d.target,d.kind,d.action,d.speculative});
 });
}
}

SelectedLibrary SelectedLibrary::load(const std::filesystem::path& path,std::string_view kernel,const Location& at){
 std::ifstream input(path,std::ios::binary);
 std::string bytes(max_spec_bytes+1,'\0');
 if(input)input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
 bytes.resize(static_cast<std::size_t>(input.gcount()));
 SpecReader reader(bytes,at,path.string());
 if(input.bad()||(!input.eof()&&input.fail()))reader.fail("cannot read recipe file");
 if(bytes.find('\0')!=std::string::npos)reader.fail("NUL in recipe file");
 auto document=reader.document();
 if(document.size()>2+max_spec_items)reader.fail("recipe declaration limit exceeded");
 std::map<std::string_view,const Form*> fields;
 std::vector<const Form*> declarations;
 Selectors defaults{};
 for(std::size_t i=2;i<document.size();++i){
  const auto& item=document[i];
  auto key=tag(item);
  if(key=="export"){declarations.push_back(&item);continue;}
  if((key!="kernel"&&key!="header"&&key!="sha256"&&key!="backend"&&key!="target")||
     value(item).empty()||!fields.emplace(key,&item).second)
   reader.fail("invalid or duplicate recipe field");
  if(key=="backend"||key=="target"){
   if(!selector(value(item)))reader.fail("invalid recipe selector");
   defaults.apply(item);
  }
 }
 if(value(get(fields,"kernel"))!=kernel)
  reader.fail("recipe kernel does not match stdffi."+std::string(kernel));
 if(declarations.empty())reader.fail("expected at least one export");
 const auto compiled=exports();
 auto library=std::find_if(compiled.begin(),compiled.end(),[&](const auto& e){return e.kernel==kernel;});
 if(library==compiled.end())
  reader.fail("no compiled recipe for stdffi."+std::string(kernel),"ON1X-STDFFI-KERNEL");
 if(value(get(fields,"header"))!=library->header||value(get(fields,"sha256"))!=library->sha256)
  reader.fail("recipe header or SHA-256 differs from the compiled recipe");

 SelectedLibrary selected;
 selected.path_=path;
 for(const auto* declaration:declarations){
  const auto& items=declaration->items;
  if(items.size()<3||!identifier(word(items[1]))||!identifier(word(items[2])))
   reader.fail("expected an export name and C symbol");
  if(items.size()>3+max_spec_items)reader.fail("export field limit exceeded");
  auto member=word(items[1]);
  auto info=std::find_if(compiled.begin(),compiled.end(),[&](const auto& e){return e.kernel==kernel&&e.member==member;});
  if(info==compiled.end()||word(items[2])!=info->symbol)reader.fail("export differs from the compiled recipe: "+std::string(member));
  auto index=static_cast<std::size_t>(info-compiled.begin());
  if(selected.find(member))reader.fail("duplicate export "+std::string(member));
  const auto& recipe=tables::recipes[index];
  auto inherited=defaults;
  std::map<std::string_view,const Form*> options;
  std::vector<const Form*> directives;
  for(std::size_t i=3;i<items.size();++i){
   auto key=tag(items[i]);
   if(key=="hint"||key=="rewrite"){directives.push_back(&items[i]);continue;}
   if((key!="params"&&key!="guard"&&key!="body"&&key!="backend"&&key!="target")||
      !options.emplace(key,&items[i]).second)
    reader.fail("invalid or duplicate field in export "+std::string(member));
   if(key=="backend"||key=="target"){
    if(!selector(value(items[i])))reader.fail("invalid export selector");
    inherited.apply(items[i]);
   }
  }
  std::vector<std::string_view> params;
  const auto& param_items=get(options,"params").items;
  if(param_items.size()!=info->arity+1)
   reader.fail("signature differs from the compiled recipe for "+std::string(member));
  for(std::size_t i=1;i<param_items.size();++i){
   auto name=word(param_items[i]);
   if(!identifier(name)||name=="true"||name=="false"||
      std::find(params.begin(),params.end(),name)!=params.end())
    reader.fail("invalid or duplicate recipe parameter");
   params.push_back(name);
  }
  const auto& guard=get(options,"guard").items;
  const auto& body=get(options,"body").items;
  if(guard.size()!=2||body.size()!=2||!matches_expression(guard[1],recipe.guard,params)||
     !matches_expression(body[1],recipe.body,params))
   reader.fail("guard or body differs from the compiled recipe for "+std::string(member));
  if(!matches_directives(directives,inherited,std::span(tables::directives).subspan(recipe.directives_begin,recipe.directives_count)))
   reader.fail("directives differ from the compiled recipe for "+std::string(member));
  selected.exports_.push_back(index);
 }
 return selected;
}
}
