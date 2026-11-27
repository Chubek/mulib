#pragma once
#include <on1x/module_graph.hpp>
#include <sstream>

namespace on1x::project_graph {
// Private staged paths are an explicit build-tool projection; they never
// replace canonical IDs or become part of the portable graph format.
inline std::string project_v3(const ModuleGraph& graph,
 const std::map<std::string,std::filesystem::path>& paths, std::string_view physical){
 using graph_format::quote;
 const auto analysis=graph.analyze();
 if(!analysis.cycles.empty()||paths.size()!=graph.files.size())
  throw std::runtime_error("invalid project graph snapshot");
 std::ostringstream out;
 out<<"{\"schema\":\"on1x.project-module-graph/v3\",\"snapshot\":"<<quote(graph.revision)
    <<",\"physical\":"<<physical<<",\"nodes\":[";
 bool first=true;
 for(const auto& id:analysis.order){
  const auto& m=graph.modules.at(id);
  if(!first)out<<',';
  first=false;
  out<<"{\"id\":"<<quote(id)<<",\"kind\":"<<quote(m.kind)<<",\"name\":"<<quote(m.name)<<",\"fragments\":[";
  for(std::size_t i=0;i<m.fragments.size();++i){if(i)out<<',';out<<quote(paths.at(m.fragments[i]).generic_string());}
  out<<"]}";
 }
 out<<"],\"edges\":[";
 first=true;
 for(const auto& [pair,witnesses]:analysis.edges){
  if(!first)out<<',';
  first=false;
  out<<"{\"source\":"<<quote(pair.first)<<",\"target\":"<<quote(pair.second)<<",\"witnesses\":[";
  for(std::size_t i=0;i<witnesses.size();++i){if(i)out<<',';out<<quote(witnesses[i]);}
  out<<"]}";
 }
 out<<"]}\n";
 return out.str();
}
}
