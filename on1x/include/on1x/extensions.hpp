#pragma once
#include <on1x/ast.hpp>
#include <filesystem>
#include <set>

namespace on1x::extensions {
struct Options {
 std::set<std::string> enabled,disabled;
 std::filesystem::path directory;
 bool execute=true;
 unsigned timeout_ms=5000;
};
std::string canonical(const std::string& name,const Location& at);
void select(std::set<std::string>& into,const std::string& names,const Location& at);
std::string describe(const Options& options,const std::string& name={});
void expand(Unit& unit,const Options& options,const std::string& origin);
}
