#include <on1x/discovery.hpp>
#include "proxy_policy.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

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
void row_error(const std::string& file,std::string message,std::size_t first,std::size_t last){
 throw Error({file,0,first},"invalid On1xfile manifest: "+message+
             (first==last?"":" (lines "+std::to_string(first)+"-"+std::to_string(last)+")"));
}
template<class Finish>
void package_rows(std::string_view text,const std::string& file,Finish finish_row){
 bool forced=false;
 std::string row;
 std::size_t start=0,end=0;
 // Match the build driver's head words, including directives not evaluated
 // by discovery. An explicit physical continuation can mask one head.
 constexpr std::string_view heads[]={
  "on1xfile","kind","name","directory","vm","scrutiny","scrutiny-work",
  "optimization-profile","optimization","backend","target","dependency","profile",
  "profile-arg","command","define","dotenv","kernel","kernel-path","source","library","resource","interface",
  "compiler","assembler","linker","assembler-arg","linker-arg",
  "proxy-http","proxy-https","proxy-any","proxy-none"
 };
 auto finish=[&](){
  if(row.empty())return;
  const auto line=std::string_view(row);
  const auto word_end=line.find_first_of(" \t\r");
  const auto key=line.substr(0,word_end);
  const auto value=word_end==std::string_view::npos?std::string_view{}:trim(line.substr(word_end));
  finish_row(key,value,start,end);
 };
 if(text.find('\0')!=std::string_view::npos)
  throw Error({file},"invalid On1xfile manifest: contains NUL");
 for(std::size_t number=1;!text.empty();++number){
  const auto newline=text.find('\n');
  auto line=trim(text.substr(0,newline));
  text=newline==std::string_view::npos?std::string_view{}:text.substr(newline+1);
  if(line.empty()||line.front()=='#')continue;
  const auto word_end=line.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-");
  const auto key=line.substr(0,word_end);
  const bool head=!forced&&std::find(std::begin(heads),std::end(heads),key)!=std::end(heads);
  if(head){finish();row.clear();}
  if(row.empty())start=number;
  end=number;
  if(head&&word_end!=std::string_view::npos&&std::string_view(" \t\r#\\").find(line[word_end])==std::string_view::npos)
   row_error(file,"manifest fields must be separated",start,end);
  bool quoted=false,continued=false;
  for(std::size_t i=0;i<line.size();++i){
   if(line[i]=='"')quoted=!quoted;
   else if(line[i]=='\\'){
    if(quoted){if(i+1<line.size())++i;}
    else{
     if(i+1!=line.size())row_error(file,"a continuation backslash must be followed only by whitespace",start,end);
     if(i==0)row_error(file,"a continuation backslash must follow a field on the same line",start,end);
     line=trim(line.substr(0,i));
     continued=true;
     break;
    }
   }else if(line[i]=='#'&&!quoted){
    line=trim(line.substr(0,i));
    break;
   }
  }
  if(quoted)row_error(file,"unterminated quoted string",start,end);
  if(!row.empty())row+=' ';
  row+=line;
  forced=continued;
 }
 if(forced)row_error(file,"dangling continuation backslash",start,end);
 finish();
}
// Import discovery reads literal package identity, not root-only policy.
Manifest parse_package(std::string_view text,const std::string& file){
 auto fail=[&](std::string message)->void{throw Error({file},"invalid On1xfile manifest: "+message);};
 Manifest result;
 std::set<std::string> fields;
 bool header=false;
 std::size_t vm_start=0,vm_end=0;
 package_rows(text,file,[&](std::string_view key,std::string_view value,std::size_t start,std::size_t end){
  auto invalid=[&](std::string message){row_error(file,std::move(message),start,end);};
  if(!header){
   if(key!="on1xfile"||value!="1")invalid("expected on1xfile 1");
   header=true;
   return;
  }
  if(key=="on1xfile")invalid("duplicate on1xfile header");
  if(key=="vm"&&!vm_start){vm_start=start;vm_end=end;}
  if(key!="kind"&&key!="name"&&key!="directory")return;
  if(!fields.insert(std::string(key)).second)invalid("duplicate "+std::string(key));
  if(key!="kind"){
   if(value.empty()||value.front()!='"')invalid("expected quoted "+std::string(key));
   const auto quote_end=value.find('"',1);
   if(quote_end==std::string_view::npos)invalid("unterminated "+std::string(key));
   if(!trim(value.substr(quote_end+1)).empty())invalid("invalid value for "+std::string(key));
   value=value.substr(1,quote_end-1);
   if(value.empty()||value.size()>1024||value.find('\\')!=std::string_view::npos)
    invalid("invalid literal "+std::string(key));
  }
  if(key=="kind"){
   if(value!="library"&&value!="executable"&&value!="complex")invalid("invalid project kind");
   result.kind=value;
  }else if(key=="name"){
   constexpr std::string_view initial="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
   if(value.size()>64||initial.find(value.front())==std::string_view::npos||
      value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789-")!=std::string_view::npos)
    invalid("invalid package name");
   result.name=value;
  }
  else result.root=value;
 });
 if(!header)fail("expected on1xfile 1");
 if(result.name.empty())fail("invalid package name");
 if(result.kind.empty())fail("invalid project kind");
 // An executable's root may also be on an import search path, but only the
 // project driver can interpret its launch policy. Libraries cannot declare it.
 if(vm_start&&result.kind=="library")
  row_error(file,"vm policy is only allowed in the root On1xfile of an executable or complex project",vm_start,vm_end);
 return result;
}
#if defined(__unix__) || defined(__APPLE__)
std::string read_root_project(){
 constexpr std::size_t limit=65536;
 const auto fd=::open("On1xfile",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
 if(fd<0){
  if(errno==ENOENT)return {};
  throw Error({"On1xfile"},"cannot open root On1xfile for proxy policy (expected a regular file)");
 }
 struct Close {int fd;~Close(){::close(fd);}} close{fd};
 struct stat info{};
 if(::fstat(fd,&info)<0||!S_ISREG(info.st_mode)||info.st_size>static_cast<off_t>(limit))
  throw Error({"On1xfile"},"expected a regular root On1xfile of at most 65536 bytes");
 std::string text;
 char buffer[4096];
 for(;;){
  const auto n=::read(fd,buffer,sizeof buffer);
  if(!n)return text;
  if(n<0){
   if(errno==EINTR)continue;
   throw Error({"On1xfile"},"cannot read root On1xfile for proxy policy");
  }
  if(static_cast<std::size_t>(n)>limit-text.size())
   throw Error({"On1xfile"},"root On1xfile byte limit exceeded");
  text.append(buffer,static_cast<std::size_t>(n));
 }
}

// Decode the JSON spelling of a quoted proxy field. Proxy URLs are printable
// ASCII (non-ASCII characters in the URI must be percent-encoded).
std::string proxy_field(std::string_view value){
 if(value.size()<2||value.front()!='"'||value.back()!='"')return {};
 value=value.substr(1,value.size()-2);
 std::string out;
 for(std::size_t i=0;i<value.size();++i){
  unsigned char c=value[i];
  if(c=='"')return {};
  if(c=='\\'){
   if(++i==value.size())return {};
   c=value[i];
   if(c=='u'){
    if(value.size()-i<=4)return {};
    unsigned n=0;
    for(unsigned j=0;j<4;++j){
     const auto hex=value[++i];
     unsigned digit=hex>='0'&&hex<='9'?hex-'0':hex>='a'&&hex<='f'?hex-'a'+10:
                    hex>='A'&&hex<='F'?hex-'A'+10:16;
     if(digit>15)return {};
     n=(n<<4)+digit;
    }
    if(n>0x7e)return {};
    c=static_cast<unsigned char>(n);
   }else if(c!='"'&&c!='\\'&&c!='/')return {};
  }
  if(c<0x21||c>0x7e||out.size()==4096)return {};
  out+=static_cast<char>(c);
 }
 return out;
}
proxy::Policy root_proxy_policy(){
 auto text=read_root_project();
 const auto begin=trim(text);
 if(begin.starts_with('(')||begin.starts_with(';')||text.find("proxy-")==std::string::npos)return {};
 const std::string file="On1xfile";
 proxy::Policy result;
 bool header=false;
 package_rows(text,file,[&](std::string_view key,std::string_view value,std::size_t start,std::size_t end){
  auto invalid=[&](std::string message){row_error(file,std::move(message),start,end);};
  if(!header){
   if(key!="on1xfile"||value!="1")invalid("expected on1xfile 1");
   header=true;
   return;
  }
  if(key=="on1xfile")invalid("duplicate on1xfile header");
  if(key=="proxy-none"){
   if(!value.empty())invalid("proxy-none takes no fields");
   if(result.none)invalid("duplicate proxy-none");
   if(!result.http.empty()||!result.https.empty()||!result.any.empty())
    invalid("proxy-none cannot be combined with other proxy directives");
   result.none=true;
   return;
  }
  auto* field=key=="proxy-http"?&result.http:key=="proxy-https"?&result.https:key=="proxy-any"?&result.any:nullptr;
  if(!field)return;
  if(result.none)invalid("proxy-none cannot be combined with other proxy directives");
  if(!field->empty())invalid("duplicate "+std::string(key));
  *field=proxy_field(value);
  if(field->empty())invalid(std::string(key)+" requires one nonempty quoted proxy URL");
 });
 return result;
}
#endif
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

namespace on1x::proxy {
Policy current_project(){
#if defined(__unix__) || defined(__APPLE__)
 try{return discovery::root_proxy_policy();}
 catch(const Error& error){
  // A source diagnostic would quote the manifest line, which could contain
  // proxy credentials. The parser's reason itself never includes any field.
  throw std::runtime_error("On1xfile:"+std::to_string(error.where.line)+": "+error.what());
 }
#else
 return {};
#endif
}
}
