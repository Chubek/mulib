#pragma once

// Private compiler/project-driver transport, separate from the source AST wire.
#include "../tools/contract_json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace on1x::project_index {
namespace fs = std::filesystem;
namespace json = contracts::json;

struct IndexMember {
 fs::path package;
 std::string declared_name;
 std::vector<fs::path> fragments,interfaces;
 bool operator==(const IndexMember&) const = default;
};
struct Index {
 fs::path project;
 std::map<std::string,IndexMember> members;
 std::map<std::string,fs::path> programs;
};

inline std::string string_field(const json::Value& value) {
 if(value.kind!='s'||value.text.empty()||value.text.size()>4096||value.text.find('\0')!=std::string::npos)
  throw std::invalid_argument("expected a nonempty bounded project module index string");
 return value.text;
}

inline std::string module_name(const json::Value& value) {
 auto name=string_field(value);
 std::size_t part=0;
 for(unsigned char c:name){
  const bool letter=(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';
  if(c=='.'&&part){part=0;continue;}
  if((!letter&&(!part||c<'0'||c>'9'))||++part>64)
   throw std::invalid_argument("invalid dotted project module index name");
 }
 if(!part)throw std::invalid_argument("invalid dotted project module index name");
 return name;
}

inline fs::path canonical_path(const json::Value& value,bool directory) {
 fs::path path(string_field(value));
 std::error_code error;
 const auto canonical=fs::canonical(path,error);
 if(error||!path.is_absolute()||path.native()!=canonical.native()||
    !(directory?fs::is_directory(canonical,error):fs::is_regular_file(canonical,error))||error)
  throw std::invalid_argument("project module index requires a canonical existing path: "+path.string());
 return canonical;
}

inline Index read(const fs::path& input) {
 constexpr std::size_t max_bytes=8*1024*1024, max_members=1024, max_paths=4096;
 std::error_code error;
 if(!fs::is_regular_file(fs::symlink_status(input,error))||error)
  throw std::invalid_argument("project module index is not a regular file");
 const auto size=fs::file_size(input,error);
 if(error||!size||size>max_bytes)throw std::invalid_argument("project module index exceeds its byte budget");
 std::ifstream stream(input,std::ios::binary);
 std::string bytes(static_cast<std::size_t>(size),'\0');
 if(!stream.read(bytes.data(),static_cast<std::streamsize>(size))||stream.peek()!=std::char_traits<char>::eof())
  throw std::invalid_argument("could not read project module index");
 auto document=json::parse(bytes);
 if(document.kind!='o'||document.object.size()!=4||string_field(document.at("schema"))!="on1x.project-modules/v1")
  throw std::invalid_argument("unsupported project module index schema");
 const auto& members=document.at("members");
 const auto& programs=document.at("programs");
 if(members.kind!='o'||programs.kind!='o'||(members.object.empty()&&programs.object.empty())||
    members.object.size()>max_members||programs.object.size()>max_members)
  throw std::invalid_argument("invalid project module index member count");
 std::size_t paths=0;
 std::map<fs::path,std::pair<fs::path,std::string>> owners;
 std::map<std::pair<fs::path,std::string>,IndexMember> identities;
 Index answer{.project=canonical_path(document.at("project"),true),.members={},.programs={}};
 for(const auto& [visible,value]:members.object){
  json::Value visible_name;
  visible_name.kind='s';
  visible_name.text=visible;
  (void)module_name(visible_name);
  if(value.kind!='o'||value.object.size()!=4)throw std::invalid_argument("invalid project module index member");
  IndexMember member{.package=canonical_path(value.at("package"),true),
                     .declared_name=module_name(value.at("declared_name")),.fragments={},.interfaces={}};
  const auto identity=std::pair(member.package,member.declared_name);
  const auto append=[&](const json::Value& array,std::string_view extension,std::vector<fs::path>& result){
   if(array.kind!='a')throw std::invalid_argument("invalid project module index path list");
   for(const auto& item:array.array){
    if(++paths>max_paths)throw std::invalid_argument("project module index exceeds its path budget");
    auto path=canonical_path(item,false);
    auto relative=path.lexically_relative(member.package);
    if(path.extension().string()!=extension||relative.empty()||*relative.begin()=="..")
     throw std::invalid_argument("project module fragment is outside its canonical package or has the wrong extension");
    const auto [owner,inserted]=owners.emplace(path,identity);
    if((!inserted&&owner->second!=identity)||std::find(result.begin(),result.end(),path)!=result.end())
     throw std::invalid_argument("duplicate project module fragment membership: "+path.string());
    result.push_back(std::move(path));
   }
  };
  append(value.at("fragments"),".on1xm",member.fragments);
  append(value.at("interfaces"),".on1xi",member.interfaces);
  if(member.fragments.empty())throw std::invalid_argument("project module member requires an implementation fragment");
  const auto [prior,inserted]=identities.emplace(identity,member);
  if(!inserted&&prior->second!=member)
   throw std::invalid_argument("inconsistent project module aliases");
  answer.members.emplace(visible,std::move(member));
 }
 for(const auto& [key,value]:programs.object){
  json::Value program_name;
  program_name.kind='s';
  program_name.text=key;
  (void)module_name(program_name);
  auto path=canonical_path(value,false);
  auto relative=path.lexically_relative(answer.project);
  if(++paths>max_paths||path.extension()!=".on1x"||relative.empty()||*relative.begin()=="..")
   throw std::invalid_argument("invalid project program membership: "+path.string());
  answer.programs.emplace(key,std::move(path));
 }
 return answer;
}
}
