#include <on1x/meta.hpp>
#include <on1x/clone.hpp>
#include <on1x/discovery.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif
namespace on1x::meta {
namespace {
void sideband(std::string_view wire,const std::string& label,diagnostic::Registry* registry,const diagnostic::Catalog* selected){
 if(wire.empty())return;
 const auto& catalog=selected?*selected:diagnostic::Catalog::embedded();
 auto protocol=[&](Location where)->void{throw catalog.failure(diagnostic::Role::ProtocolFailure,std::move(where));};
 if(wire.back()!='\n')protocol({"<ppx:"+label+" sideband>"});
 std::set<std::string> registered;
 std::size_t begin=0,count=0;
 while(begin<wire.size()){
   auto end=wire.find('\n',begin);if(end==std::string_view::npos)end=wire.size();
   auto line=wire.substr(begin,end-begin);begin=end==wire.size()?end:end+1;
   if(line.empty())continue;
   if(++count>256)protocol({"<ppx:"+label+" sideband>"});
   SExpr node;
   try{node=read_sexpr(line,"<ppx:"+label+" sideband>");}
   catch(const Error&){protocol({"<ppx:"+label+" sideband>"});}
   auto fail=[&]()->void{protocol(node.where);};
   if(node.kind!=SExpr::Kind::List||node.items.empty()||node.items[0].kind!=SExpr::Kind::Symbol)fail();
   const auto& tag=node.items[0].text;
   if(tag=="register"){
    if(node.items.size()!=2||node.items[1].kind!=SExpr::Kind::String)fail();
    auto entry=catalog.ppx(node.items[1].text);if(!entry)fail();
    registered.insert(entry->id);continue;
   }
   if(tag!="emit"||node.items.size()!=3||node.items[1].kind!=SExpr::Kind::String)fail();
   auto entry=catalog.ppx(node.items[1].text);if(!entry||!registered.contains(entry->id))fail();
   Location where;
   try{where=decode_source_location(node.items[2]);}catch(const Error&){fail();}
   auto record=catalog.record(*entry,std::move(where));
    if(registry)(void)registry->emit(record);
    else {diagnostic::Registry validate;(void)validate.emit(record);}
   if(record.level.failure)throw Error(record.where,record.message,record.id);
  }
 }
}
std::string run_transformer(const std::string& program,std::string_view input,unsigned timeout_ms,const std::string& label,const std::map<std::string,std::string>& overrides,diagnostic::Registry* diagnostics,const diagnostic::Catalog* diagnostic_catalog){
 if(label.size()>128)throw Error({"<ppx>"},"PPX label exceeds 128 bytes");
 Location at{"<ppx:"+label+">"};
 auto fail=[&](const std::string& text)->void{throw Error(at,"PPX "+label+": "+text);};
 if(input.size()>max_wire_bytes||!timeout_ms||timeout_ms>60000)fail("invalid input/time limit");
 if(program.empty()||program.size()>4096)fail("program path must be 1..4096 bytes");
#if defined(__unix__) || defined(__APPLE__)
 auto path=std::filesystem::absolute(program).lexically_normal().string();
 if(!std::filesystem::is_regular_file(path))fail("program is not a regular file: "+path);
 std::vector<std::string> args;
 {
  std::ifstream file(path,std::ios::binary);char magic[2]={};file.read(magic,2);
  if(magic[0]=='#'&&magic[1]=='!'){
   std::string line;char c=0;while(file.get(c)&&c!='\n'&&line.size()<=4096)line+=c;
   if(line.size()>4096)fail("shebang exceeds 4096 bytes");
   if(!line.empty()&&line.back()=='\r')line.pop_back();
   auto begin=line.find_first_not_of(" \t");auto end=begin==std::string::npos?begin:line.find_first_of(" \t",begin);
   auto interpreter=begin==std::string::npos?std::string{}:line.substr(begin,end-begin);
   if(interpreter.empty()||interpreter[0]!='/')fail("shebang requires an absolute interpreter path");
   args.push_back(std::move(interpreter));
   if(end!=std::string::npos){auto option=line.find_first_not_of(" \t",end);if(option!=std::string::npos)args.push_back(line.substr(option));}
   args.push_back(path);
  }else args.push_back(path); // execv never falls back to /bin/sh on ENOEXEC.
 }
 std::vector<char*> argv;for(auto& arg:args)argv.push_back(arg.data());argv.push_back(nullptr);
 std::vector<std::string> environment;
    for(char** entry=environ;entry&&*entry;++entry){std::string value=*entry;if(!value.starts_with("ON1X_PPX_LABEL=")&&!value.starts_with("ON1X_PPX_PROTOCOL=")&&!value.starts_with("ON1X_PPX_DIAGNOSTICS_FD=")&&!value.starts_with("ON1X_PPX_DIAGNOSTIC_ID=")&&!value.starts_with("ON1X_PPX_DIAGNOSTIC_FAILURE=")&&!overrides.contains(value.substr(0,value.find('='))))environment.push_back(std::move(value));}
   for(const auto& [key,value]:overrides)environment.push_back(key+"="+value);
  environment.push_back("ON1X_PPX_LABEL="+label);environment.push_back("ON1X_PPX_PROTOCOL=1");environment.push_back("ON1X_PPX_DIAGNOSTICS_FD=3");
 std::vector<char*> envp;for(auto& value:environment)envp.push_back(value.data());envp.push_back(nullptr);
 struct Socket {int fd=-1;~Socket(){if(fd>=0)::close(fd);}void close(){if(fd>=0)::close(fd);fd=-1;}};
  Socket sockets[8];
  for(int i=0;i<4;++i){int fds[2];if(::socketpair(AF_UNIX,SOCK_STREAM,0,fds))fail("socketpair failed");
  sockets[i*2].fd=fds[0];sockets[i*2+1].fd=fds[1];
  for(int fd:fds)if(::fcntl(fd,F_SETFD,FD_CLOEXEC)==-1)fail("cannot set close-on-exec");
 }
 auto pid=::fork();if(pid<0)fail("fork failed");
 if(pid==0){
   if(::setpgid(0,0)||::dup2(sockets[1].fd,0)<0||::dup2(sockets[3].fd,1)<0||::dup2(sockets[5].fd,2)<0||::dup2(sockets[7].fd,3)<0)::_exit(126);
   for(auto& s:sockets)if(s.fd>3)::close(s.fd);
  ::execve(argv[0],argv.data(),envp.data());static constexpr char error[]="PPX exec failed (unsupported executable, permissions or interpreter)\n";
  (void)::write(2,error,sizeof(error)-1);::_exit(127);
 }
 (void)::setpgid(pid,pid);
 struct Child {pid_t pid;bool reaped=false;~Child(){::kill(-pid,SIGKILL);if(!reaped){::kill(pid,SIGKILL);int status;while(::waitpid(pid,&status,0)<0&&errno==EINTR){}}}} child{pid};
  sockets[1].close();sockets[3].close();sockets[5].close();sockets[7].close();
  for(int i:{0,2,4,6})if(::fcntl(sockets[i].fd,F_SETFL,O_NONBLOCK)==-1)fail("cannot set nonblocking transport");
  std::string output,errors,diagnostic_wire;std::size_t sent=0;int status=0;
 auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout_ms);
 while(true){
  if(std::chrono::steady_clock::now()>=deadline)fail("transformer timeout after "+std::to_string(timeout_ms)+" ms");
  if(!child.reaped){auto result=::waitpid(pid,&status,WNOHANG);if(result==pid)child.reaped=true;else if(result<0&&errno!=EINTR)fail("waitpid failed");}
  if(sent==input.size())sockets[0].close();
   if(child.reaped&&sockets[2].fd<0&&sockets[4].fd<0&&sockets[6].fd<0)break;
   pollfd fds[4]={{sockets[0].fd,POLLOUT,0},{sockets[2].fd,POLLIN,0},{sockets[4].fd,POLLIN,0},{sockets[6].fd,POLLIN,0}};
   int ready=::poll(fds,4,10);if(ready<0){if(errno==EINTR)continue;fail("poll failed");}
  if(sockets[0].fd>=0&&(fds[0].revents&(POLLOUT|POLLHUP|POLLERR))){
#ifdef MSG_NOSIGNAL
   auto n=::send(sockets[0].fd,input.data()+sent,input.size()-sent,MSG_NOSIGNAL);
#else
   int one=1;(void)::setsockopt(sockets[0].fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one));
   auto n=::send(sockets[0].fd,input.data()+sent,input.size()-sent,0);
#endif
   if(n>0)sent+=static_cast<std::size_t>(n);else if(n<0&&errno!=EINTR&&errno!=EAGAIN&&errno!=EWOULDBLOCK)sockets[0].close();
  }
   for(int i=1;i<4;++i){auto& socket=sockets[i*2];if(socket.fd<0||!fds[i].revents)continue;
    char buffer[8192];auto n=::recv(socket.fd,buffer,sizeof(buffer),0);
    if(n>0){auto& destination=i==1?output:i==2?errors:diagnostic_wire;auto limit=i==1?max_wire_bytes:std::size_t{65536};
     if(static_cast<std::size_t>(n)>limit-destination.size()){
      if(i==3)throw (diagnostic_catalog?*diagnostic_catalog:diagnostic::Catalog::embedded()).failure(
       diagnostic::Role::ProtocolFailure,at);
      fail(i==1?"stdout byte limit exceeded":"stderr byte limit exceeded");
     }
    destination.append(buffer,static_cast<std::size_t>(n));
   }else if(n==0)socket.close();else if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)fail("transport read failed");
  }
 }
 if(!WIFEXITED(status)||WEXITSTATUS(status)!=0){
  auto message=WIFEXITED(status)?"exit status "+std::to_string(WEXITSTATUS(status)):"terminated by signal";
  if(!errors.empty())message+="; stderr: "+errors.substr(0,4096);
  fail(message);
  }
  if(output.empty())fail("empty stdout; expected one S-expression AST");
  sideband(diagnostic_wire,label,diagnostics,diagnostic_catalog);
  return output;
#else
  (void)program;(void)input;(void)overrides;(void)diagnostics;(void)diagnostic_catalog;fail("external PPX transport is currently implemented only for POSIX hosts");return {};
#endif
}
void rewrite(Unit& unit,const PpxOptions& options){
 require_no_kernel_imports(unit);
 if(!needs_macro_expansion(unit)&&unit.ppx.empty()&&options.programs.empty()&&options.schemas.empty()){require_core(unit);return;}
 std::size_t budget=262144;auto working=clone_unit(unit,budget);
 if(!working.ppx.empty()&&!options.allow_source)throw Error(working.ppx.front().where,"source PPX execution requires --allow-source-ppx (not a sandbox)");
 std::vector<PpxRegistration> programs=working.ppx;
  for(auto& p:programs){
   auto path=std::filesystem::path(p.program);
   // Preserve source-relative explicit paths, including old bare filenames.
   auto relative=std::filesystem::path(p.where.file).parent_path()/path;
   if(path.has_parent_path()||std::filesystem::is_regular_file(relative))p.program=relative.lexically_normal().string();
  }
 programs.insert(programs.end(),options.programs.begin(),options.programs.end());
 if(programs.size()>32)throw Error({"<ppx>"},"PPX program count limit exceeded");
 auto schema=base_schema();
 std::set<std::string> labels;for(const auto& p:programs){
  if(p.label.size()>128)throw Error(p.where,"PPX label exceeds 128 bytes");
  auto label=string(p.label);label.where=p.where;schema.validate(label,"Name","$.ppx_label");
  if(!labels.insert(p.label).second)throw Error(p.where,"duplicate PPX label: "+p.label);
 }
 for(const auto& [label,path]:options.schemas){(void)path;if(!labels.contains(label))throw Error({"<ppx>"},"schema supplied for unregistered PPX label: "+label);}
 expand_macros(working);working.ppx.clear();
 if(programs.empty()){require_core(working);unit=std::move(working);return;}
  std::map<std::string,std::string> native_schemas;
  for(auto& p:programs)if(!std::filesystem::path(p.program).has_parent_path()&&
    (p.where.file=="<standard-ppx>"||!std::filesystem::is_regular_file(p.program))){
   auto found=discovery::ppx(p.program,options.paths,p.where);
   p.program=found.program.string();
   if(!found.schema.empty())native_schemas[p.label]=found.schema.string();
  }
  for(const auto& [label,path]:options.schemas)schema.extend(read_source_file(path),label,path);
  auto tree=encode(working);schema.validate(tree);
  const auto is_docstring=[](const SExpr& entry){return entry.items.front().text=="Docstring";};
  std::vector<SExpr> docs;
  std::copy_if(tree.items[2].items.begin(),tree.items[2].items.end(),std::back_inserter(docs),is_docstring);
 auto extensions=[&](const SExpr& ast,bool final){
  auto walk=[&](auto&& self,const SExpr& node)->void{
   if(node.kind!=SExpr::Kind::List)return;
   if(!node.items.empty()&&node.items[0].kind==SExpr::Kind::Symbol){
    const auto& name=node.items[0].text;auto shape=schema.nodes.find(name);
    if(final&&shape!=schema.nodes.end()&&!shape->second.owner.empty())throw Error(node.where,"unhandled PPX schema extension: "+name);
    if(name=="ExtensionExpr"||name=="Attribute"){
     const auto& label=node.items.at(1).text;if(!labels.contains(label))throw Error(node.where,"extension uses unregistered PPX label: "+label);
    }
   }for(const auto& item:node.items)self(self,item);
  };walk(walk,ast);
 };
 extensions(tree,false);
  for(const auto& program:programs){
    std::map<std::string,std::string> environment;
    if(auto it=native_schemas.find(program.label);it!=native_schemas.end())environment["ON1X_PPX_SCHEMA"]=it->second;
    const auto& catalog=options.diagnostic_catalog?*options.diagnostic_catalog:diagnostic::Catalog::embedded();
    environment["ON1X_PPX_DIAGNOSTIC_FAILURE"]=catalog.presentation().ppx_client_failure;
    if(auto id=catalog.ppx_id(program.label))environment["ON1X_PPX_DIAGNOSTIC_ID"]=*id;
     auto output=run_transformer(program.program,write_sexpr(tree),options.timeout_ms,program.label,environment,options.diagnostics,options.diagnostic_catalog);
   auto replacement=read_sexpr(output,"<ppx:"+program.label+" stdout>");schema.validate(replacement);extensions(replacement,false);
   // Docstrings are source metadata, not declarations authored by a processor.
   // Keep them available to later passes even if a pass rebuilds the Unit.
   auto& declarations=replacement.items[2].items;
   std::erase_if(declarations,is_docstring);
   declarations.insert(declarations.end(),docs.begin(),docs.end());
   if(!docs.empty())schema.validate(replacement);
   tree=std::move(replacement);
 }
  extensions(tree,true);auto result=decode(tree);
  // A generic host obligation: a later v1 transformer cannot erase/alter the
  // explicit interface generated by a trusted declaration extension. Bodies
  // still undergo normal post-PPX checking. This knows no protocol/member names.
  std::map<std::string,Function*> declarations;std::set<std::string> duplicate_declarations;
  for(auto& f:result.functions)if(!declarations.emplace(f.name,&f).second)duplicate_declarations.insert(f.name);
  for(const auto& expected:working.functions)if(expected.extension_generated){
   if(duplicate_declarations.contains(expected.name))throw Error(expected.where,"ON1X-EXT-INTERFACE: duplicate generated declaration","ON1X-EXT-INTERFACE");
   auto hit=declarations.find(expected.name);auto* actual=hit==declarations.end()?nullptr:hit->second;
   bool same=actual&&!actual->external&&actual->params.size()==expected.params.size()&&actual->result==expected.result;
   if(same)for(std::size_t i=0;i<expected.params.size();++i)if(actual->params[i].type!=expected.params[i].type)same=false;
   if(!same)throw Error(expected.where,"ON1X-EXT-INTERFACE: native generated declaration interface changed by ordinary PPX: "+expected.name,"ON1X-EXT-INTERFACE");
   actual->extension_generated=true;
  }
    result.scrutiny=std::move(working.scrutiny);
    result.extension_evidence=std::move(working.extension_evidence);
   result.source_digests=std::move(working.source_digests);
   require_core(result);unit=std::move(result);
}
}
