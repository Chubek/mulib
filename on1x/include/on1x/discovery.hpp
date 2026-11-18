#pragma once
#include <on1x/meta.hpp>
#include <filesystem>

namespace on1x::discovery {
namespace fs = std::filesystem;
// Colon-separated on every host; empty entries never grant CWD lookup.
void append_paths(std::vector<fs::path>&,std::string_view);
void environment(std::vector<fs::path>&,const char*);
struct Processor {std::string name,entry,schema;};
struct Manifest {
 std::string kind,name,root=".";
 std::vector<Processor> processors;
};
Manifest manifest(const fs::path&);
// Returns an empty path for a directory which does not advertise a stdlib.
fs::path stdlib(const fs::path&);
fs::path library(const fs::path&,const std::string&,const Location&);
struct Ppx {fs::path program,schema;};
Ppx ppx(const std::string&,const std::vector<fs::path>&,const Location&);
}
