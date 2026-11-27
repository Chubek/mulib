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
    if (modules.size() > 1024 || files.size() > 1024 || requests.size() > 16384 ||
        roots.size() > 1024 || auxiliary.size() > 16384)
      throw std::length_error("module graph budget exceeded");
    Analysis a;
    std::set<std::string> seen;
    for (auto &[id, m] : modules) {
      if (id.empty() || m.id != id || m.fragments.empty() ||
          (m.kind != "named" && m.kind != "program" &&
           m.kind != "initializer" && m.kind != "legacy"))
        throw std::invalid_argument("invalid module identity or kind");
      if (m.fragments.size() > 1024)
        throw std::length_error("module graph fragment budget exceeded");
      a.forward[id];
      a.reverse[id];
      for (auto &f : m.fragments)
        if (f.empty() || files.at(f).id != f || files.at(f).owner != id ||
            files.at(f).package != m.package || !seen.insert(f).second)
          throw std::invalid_argument("invalid fragment ownership");
    }
    if (seen.size() != files.size())
      throw std::invalid_argument("missing fragment membership");
    seen.clear();
    for (auto &root : roots) {
      if (!seen.insert(root).second)
        throw std::invalid_argument("duplicate graph root");
      (void)files.at(root);
    }
    for (const auto &x : auxiliary) {
      if (x.kind == "signature") {
        if (!modules.contains(x.source) || x.target.empty() || x.revision.empty() ||
            files.contains(x.target) || modules.contains(x.target))
          throw std::invalid_argument("invalid signature input");
      } else if (x.kind == "prelude") {
        (void)files.at(x.source);
        (void)files.at(x.target);
      } else
        throw std::invalid_argument("invalid auxiliary relation kind");
    }
    seen.clear();
    std::size_t count = 0;
    for (auto &r : requests) {
      if (r.id.empty() || r.targets.empty() || !seen.insert(r.id).second || r.end < r.begin ||
          r.end - r.begin != r.spelling.size())
        throw std::invalid_argument("invalid request identity or span");
      if (r.kind != "quoted" && r.kind != "named" && r.kind != "program" &&
          r.kind != "selective" && r.kind != "relative")
        throw std::invalid_argument("invalid request kind");
      if (r.targets.size() > 1024 || r.selectors.size() > 16384 ||
          r.declarations.size() > 65536)
        throw std::length_error("module graph request budget exceeded");
      for (const auto &selector : r.selectors)
        if (selector.name.empty() ||
            (selector.kind != "name" && selector.kind != "operator"))
          throw std::invalid_argument("invalid graph selector");
      auto &from = files.at(r.source).owner;
      std::set<std::string> owners, targets;
      for (auto &t : r.targets) {
        if (!targets.insert(t).second)
          throw std::invalid_argument("duplicate physical request target");
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
                                  const LoaderOptions &,
                                  const std::filesystem::path &protected_output = {});
} // namespace on1x
#include <on1x/module_graph_format.hpp>
