#include <on1x/ast.hpp>
#include <fstream>
#include <filesystem>
#include <set>
#include <map>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
namespace {
void write_all(const std::string& name,const std::string& text){
 if(name.empty()||name=="-"){std::cout<<text;return;}
 std::ofstream out(name,std::ios::binary);if(!out||(out<<text).fail())throw std::runtime_error("cannot write "+name);
}
on1x::Unit parse_with_imports(const std::filesystem::path& filename,
                              std::set<std::filesystem::path>& active,
                              std::set<std::filesystem::path>& visited,
                              int depth){
 if(depth>64)throw std::runtime_error("import recursion limit exceeded");
 auto resolved=std::filesystem::absolute(filename).lexically_normal();
 if(active.contains(resolved))throw std::runtime_error("import cycle: "+resolved.string());
 if(visited.contains(resolved))return {};
 active.insert(resolved);
 auto tokens=on1x::scan(on1x::read_source_file(resolved.string()),resolved.string());
 auto current=on1x::parse(tokens);
 on1x::Unit out;
 for(const auto& name:current.imports){
  auto child=parse_with_imports(resolved.parent_path()/name,active,visited,depth+1);
  for(auto& fn:child.functions)out.functions.push_back(std::move(fn));
 }
 for(auto& fn:current.functions)out.functions.push_back(std::move(fn));
 active.erase(resolved);visited.insert(resolved);
 return out;
}
void usage(){std::cout<<"on1xc - a small strict ML-family frontend targeting ISO C11\n"
 "usage: on1xc [--check | --emit-c | --tokens | --types] INPUT.on1x [-o OUTPUT]\n"
 "       on1xc --help\n"
 "Output defaults to stdout. Use tools/on1x-build.py to link a native executable.\n";}
}
int main(int argc,char**argv){
 try{
  std::string mode="--emit-c",source,out;
  for(int i=1;i<argc;++i){std::string arg=argv[i];
   if(arg=="--help"||arg=="-h"){usage();return 0;}
   if(arg=="--check"||arg=="--emit-c"||arg=="--tokens"||arg=="--types")mode=arg;
   else if(arg=="-o"){if(++i>=argc)throw std::runtime_error("-o needs a file");out=argv[i];}
   else if(!arg.empty()&&arg[0]=='-')throw std::runtime_error("unknown option "+arg);
   else if(source.empty())source=arg;else throw std::runtime_error("too many input files");
  }
  if(source.empty()){usage();return 2;}
   if(mode=="--tokens"){
    auto tokens=on1x::scan(on1x::read_source_file(source),source);
   std::ostringstream listing;for(auto& t:tokens)listing<<t.where.line<<":"<<t.where.column<<"  "<<t.text<<"\n";
   write_all(out,listing.str());return 0;
  }
  std::set<std::filesystem::path> active,visited;
  auto unit=parse_with_imports(source,active,visited,0);
   on1x::check(unit);
   if(mode=="--check"){std::cout<<"OK: "<<unit.functions.size()<<" functions\n";return 0;}
   if(mode=="--types"){
    std::ostringstream listing;
     for(const auto& function:unit.functions){
      if(function.name.starts_with("@"))continue;
      std::vector<on1x::Type> parameters;for(const auto& parameter:function.params)parameters.push_back(parameter.type);
      auto signature=on1x::Type::function(std::move(parameters),function.result);
      std::map<std::size_t,std::size_t> variables;
      auto normalize=[&](auto&& self,const on1x::Type& type)->on1x::Type{
       if(type.kind()==on1x::Type::Generic){
        auto [variable,inserted]=variables.emplace(type.generic_identity(),variables.size());(void)inserted;
        return on1x::Type::bound(variable->second);
       }
       if(type.is_function()){
        std::vector<on1x::Type> args;for(const auto& p:type.parameters())args.push_back(self(self,p));
        return on1x::Type::function(std::move(args),self(self,type.result()));
       }
       if(type.is_tuple()){
        std::vector<on1x::Type> elements;for(const auto& p:type.elements())elements.push_back(self(self,p));
        return on1x::Type::tuple(std::move(elements));
       }
       return type;
      };
      signature=normalize(normalize,signature);
      listing<<(function.external?"extern ":"")<<function.name<<" : ";
      if(!variables.empty()){
       listing<<"forall(";for(std::size_t i=0;i<variables.size();++i){if(i)listing<<", ";listing<<"a"<<i;}listing<<") ";
      }
      listing<<on1x::type_name(signature)<<"\n";
    }
    write_all(out,listing.str());return 0;
   }
  write_all(out,on1x::emit_c(unit,source));return 0;
 }catch(const on1x::Error& e){
  std::cerr<<e.where.file<<":"<<e.where.line<<":"<<e.where.column<<": error: "<<e.what()<<"\n";return 1;
 }catch(const std::exception&e){std::cerr<<"on1xc: "<<e.what()<<"\n";return 2;}
}
