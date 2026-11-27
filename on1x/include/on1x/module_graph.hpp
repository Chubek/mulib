#pragma once
#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace on1x {
struct LoaderOptions;
struct GraphFile {
  std::string id, owner, package, path, revision;
};
struct GraphModule {
  std::string id, kind, name, package;
  std::vector<std::string> fragments;
};
struct GraphSelector {
  std::string kind, name, alias;
};
struct GraphRequest {
  std::string id, source, kind, module, alias, spelling;
  std::size_t begin = 0, end = 0, relative = 0;
  std::vector<GraphSelector> selectors;
  std::vector<std::string> targets, declarations;
};
struct GraphAux {
  std::string kind, source, target, revision;
};
struct ModuleGraph {
  using Adjacency = std::map<std::string, std::set<std::string>>;
  using Pair = std::pair<std::string, std::string>;
  std::map<std::string, GraphFile> files;
  std::map<std::string, GraphModule> modules;
  std::vector<GraphRequest> requests;
  std::vector<GraphAux> auxiliary;
  std::vector<std::string> roots;
  std::string revision, policy, oml;
  struct Analysis {
    Adjacency forward, reverse;
    std::map<Pair, std::vector<std::string>> edges, internal;
    std::vector<std::vector<std::string>> components, cycles;
    std::vector<std::string> order;
    std::set<std::string> closure(const std::string &start,
                                  bool backwards = false) const {
      const auto &adj = backwards ? reverse : forward;
      std::vector<std::string> todo{start};
      std::set<std::string> seen{start};
      while (!todo.empty()) {
        auto n = todo.back();
        todo.pop_back();
        for (auto &p : adj.at(n))
          if (seen.insert(p).second)
            todo.push_back(p);
      }
      seen.erase(start);
      return seen;
    }
  };
  Analysis analyze() const {
    if (modules.size() > 1024 || files.size() > 1024 || requests.size() > 16384)
      throw std::length_error("module graph budget exceeded");
    Analysis a;
    std::set<std::string> seen;
    for (auto &[id, m] : modules) {
      a.forward[id];
      a.reverse[id];
      for (auto &f : m.fragments)
        if (files.at(f).owner != id || !seen.insert(f).second)
          throw std::invalid_argument("invalid fragment ownership");
    }
    if (seen.size() != files.size())
      throw std::invalid_argument("missing fragment membership");
    for (auto &root : roots)
      (void)files.at(root);
    seen.clear();
    std::size_t count = 0;
    for (auto &r : requests) {
      if (r.targets.empty() || !seen.insert(r.id).second || r.end < r.begin ||
          r.end - r.begin != r.spelling.size())
        throw std::invalid_argument("invalid request identity or span");
      auto &from = files.at(r.source).owner;
      std::set<std::string> owners;
      for (auto &t : r.targets) {
        if (++count > 65536)
          throw std::length_error("module graph witness budget exceeded");
        owners.insert(files.at(t).owner);
      }
      for (auto &to : owners) {
        (from == to ? a.internal : a.edges)[{from, to}].push_back(r.id);
        if (from != to) {
          a.forward.at(from).insert(to);
          a.reverse.at(to).insert(from);
        }
      }
    }
    // Kosaraju, traversing in canonical ID order. Recursion bounded by the node
    // cap.
    std::vector<std::string> finish, stack;
    seen.clear();
    std::set<std::string> active;
    auto dfs = [&](auto &&self, const std::string &n) -> void {
      seen.insert(n);
      stack.push_back(n);
      active.insert(n);
      for (auto &p : a.forward.at(n)) {
        if (!seen.contains(p))
          self(self, p);
        else if (active.contains(p) && a.cycles.empty()) {
          a.cycles.emplace_back(std::find(stack.begin(), stack.end(), p),
                                stack.end());
          a.cycles.back().push_back(p);
        }
      }
      active.erase(n);
      stack.pop_back();
      finish.push_back(n);
    };
    for (auto &[n, _] : modules)
      if (!seen.contains(n))
        dfs(dfs, n);
    if (a.cycles.empty())
      a.order = finish;
    seen.clear();
    auto collect = [&](auto &&self, const std::string &n) -> void {
      seen.insert(n);
      a.components.back().push_back(n);
      for (auto &p : a.reverse.at(n))
        if (!seen.contains(p))
          self(self, p);
    };
    for (auto i = finish.rbegin(); i != finish.rend(); ++i)
      if (!seen.contains(*i)) {
        a.components.emplace_back();
        collect(collect, *i);
        std::sort(a.components.back().begin(), a.components.back().end());
      }
    return a;
  }
  std::string serialize(bool logical, std::string_view format) const;
};
// Projection of an already canonical loader origin; does not read the filesystem.
std::string graph_file_id(const std::filesystem::path &canonical_file);
ModuleGraph inspect_module_graph(const std::filesystem::path &,
                                 const LoaderOptions &);
} // namespace on1x
#include <on1x/module_graph_format.hpp>
