#pragma once
// Single structural projection for both lossless formats. Array and object tags
// keep even empty containers distinguishable in S-expressions.
namespace on1x {
namespace graph_format {
inline std::string quote(std::string_view s) {
  std::string out = "\"";
  constexpr char hex[] = "0123456789abcdef";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c < 32) {
      out += "\\u00";
      out += hex[c >> 4];
      out += hex[c & 15];
    } else
      out += c;
  }
  return out + '"';
}
struct Writer {
  bool sexpr;
  using Fields = std::vector<std::pair<std::string, std::string>>;
  std::string obj(const Fields &fields) const {
    std::string s = sexpr ? "(object" : "{";
    bool first = true;
    for (auto &[k, v] : fields) {
      if (sexpr)
        s += " (" + quote(k) + " " + v + ")";
      else {
        s += (first ? "" : ",") + quote(k) + ":" + v;
        first = false;
      }
    }
    return s + (sexpr ? ")" : "}");
  }
  std::string arr(const std::vector<std::string> &items) const {
    std::string s = sexpr ? "(array" : "[";
    bool first = true;
    for (auto &v : items) {
      s += (sexpr ? " " : first ? "" : ",") + v;
      first = false;
    }
    return s + (sexpr ? ")" : "]");
  }
  std::string strings(const std::vector<std::string> &v) const {
    std::vector<std::string> t;
    for (auto &s : v)
      t.push_back(quote(s));
    return arr(t);
  }
};
} // namespace graph_format
inline std::string ModuleGraph::serialize(bool logical,
                                          std::string_view format) const {
  using graph_format::quote;
  auto a = analyze();
  if (format == "dot") {
    std::string s = "digraph modules {\n";
    auto edge = [&](auto &f, auto &t) {
      s += "  " + quote(f) + " -> " + quote(t) + ";\n";
    };
    if (logical) {
      for (auto &[id, m] : modules)
        s += "  " + quote(id) + " [label=" + quote(m.name) + "];\n";
      for (auto &[p, _] : a.edges)
        edge(p.first, p.second);
    } else {
      for (auto &[id, f] : files)
        s += "  " + quote(id) + " [label=" + quote(f.path) + "];\n";
      for (auto &r : requests)
        for (auto &t : r.targets)
          edge(r.source, t);
    }
    return s + "}\n";
  }
  if (format != "json" && format != "sexpr")
    throw std::invalid_argument("unknown graph format");
  graph_format::Writer w{format == "sexpr"};
  std::vector<std::string> nodes, req, aux, e, in, scc, cycles, rs = roots,
                                                                packages;
  std::set<std::string> pk;
  for (auto &[_, f] : files)
    if (!f.package.empty())
      pk.insert(f.package);
  packages.assign(pk.begin(), pk.end());
  if (logical) {
    for (auto &r : rs)
      r = files.at(r).owner;
    std::sort(rs.begin(), rs.end());
    rs.erase(std::unique(rs.begin(), rs.end()), rs.end());
    for (auto &[id, m] : modules)
      nodes.push_back(w.obj({{"id", quote(id)},
                             {"kind", quote(m.kind)},
                             {"name", quote(m.name)},
                             {"package", quote(m.package)},
                             {"fragments", w.strings(m.fragments)}}));
    auto edges = [&](auto &map, auto &out) {
      for (auto &[p, v] : map)
        out.push_back(w.obj({{"source", quote(p.first)},
                             {"target", quote(p.second)},
                             {"witnesses", w.strings(v)}}));
    };
    edges(a.edges, e);
    edges(a.internal, in);
    for (auto &c : a.components)
      scc.push_back(w.strings(c));
    for (auto &c : a.cycles)
      cycles.push_back(w.strings(c));
  } else {
    for (auto &[id, f] : files)
      nodes.push_back(w.obj({{"id", quote(id)},
                             {"owner", quote(f.owner)},
                             {"package", quote(f.package)},
                             {"path", quote(f.path)},
                             {"revision", quote(f.revision)}}));
    for (auto &r : requests) {
      std::vector<std::string> selectors;
      for (auto &n : r.selectors)
        selectors.push_back(w.obj({{"kind", quote(n.kind)},
                                   {"name", quote(n.name)},
                                   {"alias", quote(n.alias)}}));
      req.push_back(w.obj({{"id", quote(r.id)},
                           {"source", quote(r.source)},
                           {"kind", quote(r.kind)},
                           {"module", quote(r.module)},
                           {"alias", quote(r.alias)},
                           {"spelling", quote(r.spelling)},
                           {"begin", quote(std::to_string(r.begin))},
                           {"end", quote(std::to_string(r.end))},
                           {"relative", quote(std::to_string(r.relative))},
                           {"selectors", w.arr(selectors)},
                           {"targets", w.strings(r.targets)},
                           {"declarations", w.strings(r.declarations)}}));
      for (auto &t : r.targets)
        e.push_back(w.obj({{"source", quote(r.source)},
                           {"target", quote(t)},
                           {"request", quote(r.id)}}));
    }
  }
  for (auto &x : auxiliary)
    aux.push_back(w.obj({{"kind", quote(x.kind)},
                         {"source", quote(x.source)},
                         {"target", quote(x.target)},
                         {"revision", quote(x.revision)}}));
  graph_format::Writer::Fields fields = {
      {"schema", quote(logical ? "on1x.module-dependency-graph/v1"
                               : "on1x.import-graph/v1")},
      {"status", quote("resolved")},
      {"snapshot", w.obj({{"id", quote(revision)},
                          {"policy", quote(policy)},
                          {"oml", quote(oml)}})},
      {"roots", w.strings(rs)},
      {"packages", w.strings(packages)},
      {"nodes", w.arr(nodes)},
      {"edges", w.arr(e)},
      {"auxiliary", w.arr(aux)},
      {"diagnostics", w.arr({})}};
  if (logical) {
    fields.emplace_back("internal", w.arr(in));
    fields.emplace_back("sccs", w.arr(scc));
    fields.emplace_back("cycles", w.arr(cycles));
    fields.emplace_back("order", w.strings(a.order));
  } else
    fields.emplace_back("requests", w.arr(req));
  return w.obj(fields);
}
} // namespace on1x
