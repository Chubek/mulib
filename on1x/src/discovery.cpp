#include <on1x/discovery.hpp>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>

namespace on1x::discovery {
namespace {
bool regular(const fs::path& p){std::error_code ec;return fs::is_regular_file(p,ec);}
std::string bounded(const fs::path& path,std::size_t limit){
 std::ifstream in(path,std::ios::binary);
 if(!in)throw Error({path.string()},"cannot read On1xfile manifest");
 std::string text;char buffer[4096];
 while(in){in.read(buffer,sizeof buffer);auto n=static_cast<std::size_t>(in.gcount());
  if(n>limit-text.size())throw Error({path.string()},"On1xfile manifest byte limit exceeded");
  text.append(buffer,n);
 }
 if(!in.eof())throw Error({path.string()},"cannot read On1xfile manifest");
 return text;
}
bool name(const std::string& text){
 return !text.empty()&&text.size()<=128&&text!="."&&text!=".."&&
  text.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==std::string::npos;
}
Manifest parse_legacy(std::string_view text,const std::string& file){
 auto tree=meta::read_sexpr(text,file);
 auto fail=[&]()->void{throw Error({file},"invalid On1xfile manifest (expected version 1)");};
 if(tree.kind!=meta::SExpr::Kind::List||tree.items.size()<4||tree.items.size()>128)fail();
 auto atom=[&](const meta::SExpr& item,const std::string& value){
  return item.kind==meta::SExpr::Kind::Symbol&&item.text==value;
 };
 if(!atom(tree.items[0],"On1xfile")||!atom(tree.items[1],"1"))fail();
 Manifest result;std::set<std::string> fields,processors;
 for(std::size_t i=2;i<tree.items.size();++i){
  const auto& item=tree.items[i];
  if(item.kind!=meta::SExpr::Kind::List||item.items.size()<2||item.items[0].kind!=meta::SExpr::Kind::Symbol)fail();
  const auto& key=item.items[0].text;
  for(std::size_t j=1;j<item.items.size();++j)
   if(item.items[j].kind!=meta::SExpr::Kind::String||item.items[j].text.empty()||item.items[j].text.size()>4096||item.items[j].text.find('\0')!=std::string::npos)fail();
  const auto& value=item.items[1].text;
  if(key=="processor"){
   if(item.items.size()<3||item.items.size()>4||!name(value)||!processors.insert(value).second)fail();
   result.processors.push_back({value,item.items[2].text,item.items.size()==4?item.items[3].text:""});
   continue;
  }
  if(item.items.size()!=2||!fields.insert(key).second)fail();
  if(key=="kind")result.kind=value;
  else if(key=="name")result.name=value;
  else if(key=="root")result.root=value;
  else if(key!="protocol"||value!="1")fail();
 }
 if(!name(result.name)||(result.kind!="stdlib"&&result.kind!="library"&&result.kind!="ppx"))fail();
 if(result.kind=="stdlib"&&result.name!="std")fail();
 if(result.kind=="ppx"){
  if(!fields.contains("protocol")||fields.contains("root"))fail();
 }else if(fields.contains("protocol")||!result.processors.empty())fail();
 return result;
}
std::string_view trim(std::string_view text){
 const auto start=text.find_first_not_of(" \t\r\n");
 if(start==std::string_view::npos)return {};
 return text.substr(start,text.find_last_not_of(" \t\r\n")-start+1);
}
// Compiler discovery only reads literal package identity. Project commands,
// profiles, templates, and directory roles are owned by the build engine.
Manifest parse_package(std::string_view text,const std::string& file){
 auto fail=[&](std::string message)->void{throw Error({file},"invalid On1xfile manifest: "+message);};
 Manifest result;
 std::set<std::string_view> fields;
 bool header=false;
 if(text.find('\0')!=std::string_view::npos)fail("contains NUL");
 for(;!text.empty();){
  const auto newline=text.find('\n');
  auto line=trim(text.substr(0,newline));
  text=newline==std::string_view::npos?std::string_view{}:text.substr(newline+1);
  if(line.empty()||line.front()=='#')continue;
  const auto word_end=line.find_first_of(" \t#");
  const auto key=line.substr(0,word_end);
  auto value=word_end==std::string_view::npos?std::string_view{}:trim(line.substr(word_end));
  if(!header){
   if(key!="on1xfile"||trim(value.substr(0,value.find('#')))!="1")fail("expected on1xfile 1");
   header=true;
   continue;
  }
  if(key!="kind"&&key!="name"&&key!="directory")continue;
  if(!fields.insert(key).second)fail("duplicate "+std::string(key));
  if(key=="kind")value=trim(value.substr(0,value.find('#')));
  else{
   if(value.empty()||value.front()!='"')fail("expected quoted "+std::string(key));
   const auto end=value.find('"',1);
   if(end==std::string_view::npos)fail("unterminated "+std::string(key));
   const auto suffix=trim(value.substr(end+1));
   if(!suffix.empty()&&suffix.front()!='#')fail("invalid value for "+std::string(key));
   value=value.substr(1,end-1);
   if(value.empty()||value.size()>1024||value.find('\\')!=std::string_view::npos)
    fail("invalid literal "+std::string(key));
  }
  if(key=="kind")result.kind=value;
  else if(key=="name")result.name=value;
  else result.root=value;
 }
 constexpr std::string_view initial="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
 if(result.name.empty()||result.name.size()>64||initial.find(result.name.front())==std::string_view::npos||
    result.name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789-")!=std::string::npos)
  fail("invalid package name");
 if(result.kind!="library"&&result.kind!="executable"&&result.kind!="complex")fail("invalid project kind");
 return result;
}
fs::path local(const fs::path& base,const std::string& value,bool file){
 auto path=fs::path(value);
 if(path.is_absolute()||std::find(path.begin(),path.end(),fs::path(".."))!=path.end())
  throw Error({(base/"On1xfile").string()},"nonlocal On1xfile manifest path: "+value);
 std::error_code ec;auto root=fs::canonical(base,ec);
 if(ec)throw Error({base.string()},"missing manifest directory");
 auto resolved=fs::canonical(base/path,ec);
 if(ec)throw Error({(base/"On1xfile").string()},"missing manifest input: "+value);
 auto relative=resolved.lexically_relative(root);
 if(relative.empty()||*relative.begin()==".."||(file?!regular(resolved):!fs::is_directory(resolved)))
  throw Error({(base/"On1xfile").string()},"missing/escaping manifest input: "+value);
 return resolved;
}
Ppx in_manifest(const fs::path& root,const std::string& requested){
 if(!regular(root/"On1xfile"))return {};
 auto m=manifest(root/"On1xfile");
 if(m.kind!="ppx")return {};
 for(const auto& p:m.processors)if(p.name==requested)
  return {local(root,p.entry,true),p.schema.empty()?fs::path{}:local(root,p.schema,true)};
 return {};
}
Ppx script(const fs::path& path,const std::string& requested){
 if(!regular(path))return {};
 std::ifstream in(path,std::ios::binary);
 auto line=[&](){
  std::string text;char c;
  while(in.get(c)&&c!='\n'){
   if(text.size()>=4096)throw Error({path.string()},"PPX manifest/shebang line exceeds 4096 bytes");
   text+=c;
  }
  if(!text.empty()&&text.back()=='\r')text.pop_back();
  return text;
 };
 if(!line().starts_with("#!"))throw Error({path.string()},"discovered native PPX requires an On1xfile manifest");
 auto second=line();std::string_view metadata;
 for(std::string_view marker:{"# on1x: ","// on1x: ","-- on1x: ","; on1x: "})
  if(second.starts_with(marker)){metadata=std::string_view(second).substr(marker.size());break;}
 if(metadata.empty())throw Error({path.string(),0,2},"discovered script PPX requires second-line on1x manifest");
  auto m=parse_legacy(metadata,path.string()+":2");
 if(m.kind!="ppx"||m.name!=requested||!m.processors.empty())
  throw Error({path.string(),0,2},"PPX manifest name/kind does not match: "+requested);
 return {fs::canonical(path),{}};
}
}
void append_paths(std::vector<fs::path>& into,std::string_view text){
 if(text.size()>65536)throw Error({"<search-path>"},"search path byte limit exceeded");
 while(!text.empty()){
  auto colon=text.find(':');auto part=text.substr(0,colon);
  if(!part.empty()){
   if(part.size()>4096||into.size()>=256)throw Error({"<search-path>"},"search path count/entry limit exceeded");
   auto path=fs::path(part).lexically_normal();
   if(std::find(into.begin(),into.end(),path)==into.end())into.push_back(std::move(path));
  }
  if(colon==std::string_view::npos)break;
  text.remove_prefix(colon+1);
 }
}
void environment(std::vector<fs::path>& into,const char* key){if(const char* value=std::getenv(key))append_paths(into,value);}
Manifest manifest(const fs::path& path){
 auto text=bounded(path,65536);
 const auto start=trim(text);
 return start.starts_with('(')||start.starts_with(';')?parse_legacy(text,path.string()):parse_package(text,path.string());
}
fs::path stdlib(const fs::path& path){
 for(const auto& dir:{path,path/"stdlib"}){
  if(!regular(dir/"On1xfile"))continue;
  auto m=manifest(dir/"On1xfile");
  if(m.kind=="stdlib"||(m.kind=="library"&&m.name=="std"))return local(dir,m.root,false);
 }
 return {};
}
fs::path library(const fs::path& root,const std::string& module,const Location& at){
 auto dot=module.find('.');auto first=module.substr(0,dot);
 for(const auto& dir:{root,root/first}){
  if(!regular(dir/"On1xfile"))continue;
  auto m=manifest(dir/"On1xfile");
  if(m.kind!="library"||m.name!=first)continue;
  auto base=local(dir,m.root,false);
  if(dot==std::string::npos)return regular(base/"__init__.on1x")?base/"__init__.on1x":base;
  auto tail=module.substr(dot+1);std::replace(tail.begin(),tail.end(),'.',fs::path::preferred_separator);
  base/=tail;auto file=base;file+=".on1x";auto init=base/"__init__.on1x";
  if(regular(file)&&regular(init))throw Error(at,"ambiguous module: "+module);
  if(regular(file))return file;
  if(regular(init))return init;
  std::error_code ec;if(fs::is_directory(base,ec))return base;
 }
 return {};
}
Ppx ppx(const std::string& requested,const std::vector<fs::path>& roots,const Location& at){
 if(!name(requested))throw Error(at,"invalid PPX discovery name: "+requested);
 if(roots.size()>256)throw Error(at,"PPX search path count limit exceeded");
 for(const auto& root:roots){
  auto result=in_manifest(root,requested);
  if(result.program.empty())result=in_manifest(root/requested,requested);
  if(!result.program.empty())return result;
  for(const auto& suffix:{"",".py",".sh",".lua",".pl",".rb",".js"}){
   auto path=root/(requested+suffix);
   if(regular(path))return script(path,requested);
  }
 }
 throw Error(at,"PPX not found in configured search paths: "+requested);
}
}
