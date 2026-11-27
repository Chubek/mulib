#include <cstdlib>
#include <iostream>
#include <on1x/module_graph.hpp>
#include <random>
using namespace on1x;
static void check(bool ok) {
  if (!ok)
    throw std::runtime_error("graph test failed");
}
static ModuleGraph graph(unsigned n) {
  ModuleGraph g;
  for (unsigned i = 0; i < n; ++i) {
    auto k = std::to_string(i), f = "f" + k;
    g.modules[k] = {k, "named", "N", k, {f}};
    g.files[f] = {f, k, k, "N.on1xm", "rev"};
  }
  g.roots = {"f0"};
  return g;
}
static void request(ModuleGraph &g, unsigned from, unsigned to) {
  GraphRequest r;
  r.id = std::to_string(g.requests.size());
  r.kind = "quoted";
  r.source = "f" + std::to_string(from);
  r.spelling = "import \"x\";";
  r.end = r.spelling.size();
  r.targets = {"f" + std::to_string(to)};
  g.requests.push_back(r);
}
int main() {
  std::mt19937 random(4189);
  for (unsigned trial = 0; trial < 500; ++trial) {
    unsigned n = 1 + random() % 10;
    auto g = graph(n);
    bool ref[10][10] = {};
    for (unsigned i = 0; i < n; ++i)
      for (unsigned j = 0; j < n; ++j)
        if (random() % 4 == 0) {
          request(g, i, j);
          if (i != j)
            ref[i][j] = true;
        }
    for (unsigned k = 0; k < n; ++k)
      for (unsigned i = 0; i < n; ++i)
        for (unsigned j = 0; j < n; ++j)
          ref[i][j] |= ref[i][k] && ref[k][j];
    auto a = g.analyze();
    bool cycle = false;
    for (unsigned i = 0; i < n; ++i) {
      auto src = std::to_string(i);
      auto f = a.closure(src), b = a.closure(src, true);
      cycle |= ref[i][i];
      for (unsigned j = 0; j < n; ++j) {
        auto dst = std::to_string(j);
        check(f.contains(dst) == (i != j && ref[i][j]));
        check(b.contains(dst) == (i != j && ref[j][i]));
        bool same = false;
        for (auto &c : a.components)
          same |= std::count(c.begin(), c.end(), src) &&
                  std::count(c.begin(), c.end(), dst);
        check(same == (i == j || (ref[i][j] && ref[j][i])));
      }
    }
    check(cycle == a.order.empty());
    check(cycle == !a.cycles.empty());
    if (!cycle)
      for (auto &[p, _] : a.edges)
        check(std::find(a.order.begin(), a.order.end(), p.second) <
              std::find(a.order.begin(), a.order.end(), p.first));
    else {
      auto &c = a.cycles.front();
      check(c.front() == c.back());
      for (unsigned i = 1; i < c.size(); ++i)
        check(a.edges.contains({c[i - 1], c[i]}));
    }
  }
  auto g = graph(2);
  request(g, 0, 1);
  request(g, 0, 1);
  request(g, 1, 1);
  g.modules.at("1").fragments.push_back("sibling");
  g.files["sibling"] = {"sibling", "1", "1", "sibling", "rev"};
  g.requests[0].targets.push_back("sibling");
  auto a = g.analyze();
  check(a.edges.at({"0", "1"}).size() == 2 && a.internal.size() == 1);
  check(g.serialize(true, "json") == g.serialize(true, "json"));
  auto invalid = [&](auto f) {
    auto bad = g;
    f(bad);
    try {
      (void)bad.analyze();
    } catch (const std::exception &) {
      return;
    }
    throw std::runtime_error("invalid graph accepted");
  };
  invalid([](auto &x) { x.requests.push_back(x.requests.front()); });
  invalid([](auto &x) { x.requests[0].targets = {"missing"}; });
  invalid([](auto &x) { x.requests[0].end = 100; });
  invalid([](auto &x) { x.modules["1"].fragments = {"f0"}; });
  for (auto logical : {false, true})
    for (auto format : {"json", "sexpr"})
      std::cout << g.serialize(logical, format) << '\n';
}
