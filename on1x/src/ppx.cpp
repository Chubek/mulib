#include <on1x/meta.hpp>
#include <on1x/clone.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
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
std::string run_transformer(const std::string& program,std::string_view input,unsigned timeout_ms,const std::string& label){
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
 for(char** entry=environ;entry&&*entry;++entry){std::string value=*entry;if(!value.starts_with("ON1X_PPX_LABEL=")&&!value.starts_with("ON1X_PPX_PROTOCOL="))environment.push_back(std::move(value));}
 environment.push_back("ON1X_PPX_LABEL="+label);environment.push_back("ON1X_PPX_PROTOCOL=1");
 std::vector<char*> envp;for(auto& value:environment)envp.push_back(value.data());envp.push_back(nullptr);
 struct Socket {int fd=-1;~Socket(){if(fd>=0)::close(fd);}void close(){if(fd>=0)::close(fd);fd=-1;}};
 Socket sockets[6];
 for(int i=0;i<3;++i){int fds[2];if(::socketpair(AF_UNIX,SOCK_STREAM,0,fds))fail("socketpair failed");
  sockets[i*2].fd=fds[0];sockets[i*2+1].fd=fds[1];
  for(int fd:fds)if(::fcntl(fd,F_SETFD,FD_CLOEXEC)==-1)fail("cannot set close-on-exec");
 }
 auto pid=::fork();if(pid<0)fail("fork failed");
 if(pid==0){
  if(::setpgid(0,0)||::dup2(sockets[1].fd,0)<0||::dup2(sockets[3].fd,1)<0||::dup2(sockets[5].fd,2)<0)::_exit(126);
  for(auto& s:sockets)if(s.fd>2)::close(s.fd);
  ::execve(argv[0],argv.data(),envp.data());static constexpr char error[]="PPX exec failed (unsupported executable, permissions or interpreter)\n";
  (void)::write(2,error,sizeof(error)-1);::_exit(127);
 }
 (void)::setpgid(pid,pid);
 struct Child {pid_t pid;bool reaped=false;~Child(){::kill(-pid,SIGKILL);if(!reaped){::kill(pid,SIGKILL);int status;while(::waitpid(pid,&status,0)<0&&errno==EINTR){}}}} child{pid};
 sockets[1].close();sockets[3].close();sockets[5].close();
 for(int i:{0,2,4})if(::fcntl(sockets[i].fd,F_SETFL,O_NONBLOCK)==-1)fail("cannot set nonblocking transport");
 std::string output,errors;std::size_t sent=0;int status=0;
 auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout_ms);
 while(true){
  if(std::chrono::steady_clock::now()>=deadline)fail("transformer timeout after "+std::to_string(timeout_ms)+" ms");
  if(!child.reaped){auto result=::waitpid(pid,&status,WNOHANG);if(result==pid)child.reaped=true;else if(result<0&&errno!=EINTR)fail("waitpid failed");}
  if(sent==input.size())sockets[0].close();
  if(child.reaped&&sockets[2].fd<0&&sockets[4].fd<0)break;
  pollfd fds[3]={{sockets[0].fd,POLLOUT,0},{sockets[2].fd,POLLIN,0},{sockets[4].fd,POLLIN,0}};
  int ready=::poll(fds,3,10);if(ready<0){if(errno==EINTR)continue;fail("poll failed");}
  if(sockets[0].fd>=0&&(fds[0].revents&(POLLOUT|POLLHUP|POLLERR))){
#ifdef MSG_NOSIGNAL
   auto n=::send(sockets[0].fd,input.data()+sent,input.size()-sent,MSG_NOSIGNAL);
#else
   int one=1;(void)::setsockopt(sockets[0].fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one));
   auto n=::send(sockets[0].fd,input.data()+sent,input.size()-sent,0);
#endif
   if(n>0)sent+=static_cast<std::size_t>(n);else if(n<0&&errno!=EINTR&&errno!=EAGAIN&&errno!=EWOULDBLOCK)sockets[0].close();
  }
  for(int i=1;i<3;++i){auto& socket=sockets[i*2];if(socket.fd<0||!fds[i].revents)continue;
   char buffer[8192];auto n=::recv(socket.fd,buffer,sizeof(buffer),0);
   if(n>0){auto& destination=i==1?output:errors;auto limit=i==1?max_wire_bytes:std::size_t{65536};
    if(static_cast<std::size_t>(n)>limit-destination.size())fail(i==1?"stdout byte limit exceeded":"stderr byte limit exceeded");
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
 return output;
#else
 (void)program;(void)input;fail("external PPX transport is currently implemented only for POSIX hosts");return {};
#endif
}
void rewrite(Unit& unit,const PpxOptions& options){
 if(unit.macros.empty()&&unit.ppx.empty()&&options.programs.empty()&&options.schemas.empty()){require_core(unit);return;}
 std::size_t budget=262144;auto working=clone_unit(unit,budget);
 if(!working.ppx.empty()&&!options.allow_source)throw Error(working.ppx.front().where,"source PPX execution requires --allow-source-ppx (not a sandbox)");
 std::vector<PpxRegistration> programs=working.ppx;
 for(auto& p:programs)p.program=(std::filesystem::path(p.where.file).parent_path()/p.program).lexically_normal().string();
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
 for(const auto& [label,path]:options.schemas)schema.extend(read_source_file(path),label,path);
 auto tree=encode(working);schema.validate(tree);
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
  auto output=run_transformer(program.program,write_sexpr(tree),options.timeout_ms,program.label);
  auto replacement=read_sexpr(output,"<ppx:"+program.label+" stdout>");schema.validate(replacement);extensions(replacement,false);tree=std::move(replacement);
 }
 extensions(tree,true);auto result=decode(tree);require_core(result);unit=std::move(result);
}
}
