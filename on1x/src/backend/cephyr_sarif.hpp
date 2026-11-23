#pragma once

#include "../../tools/contract_json.hpp"
#include <on1x/ast.hpp>
#include <iterator>

namespace on1x::cephyr {
namespace json = on1x::contracts::json;

inline std::string file_uri(std::string_view name) {
    std::string out = name.starts_with('/') ? "file://" : "";
    constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned char c : name) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '/' || c == '-' || c == '.' || c == '_' || c == '~') out += static_cast<char>(c);
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}

inline void write_json(std::string& out, const json::Value& v) {
    if (v.kind == 'o' || v.kind == 'a') {
        out += v.kind == 'o' ? '{' : '[';
        bool first = true;
        auto separator = [&] { if (!first) out += ','; first = false; };
        for (const auto& [key, value] : v.object) {
            separator(); out += json::quote(key); out += ':'; write_json(out, value);
        }
        for (const auto& value : v.array) { separator(); write_json(out, value); }
        out += v.kind == 'o' ? '}' : ']';
    } else {
        out += v.kind == 's' ? json::quote(v.text) : v.kind == 'n' ? "null" : v.text;
    }
}

// Preserve Cephyr's primary C coordinates. A checked HIR statement can span
// many generated tokens, so its related location carries only an original line.
inline std::string source_sarif(std::string_view raw, std::string_view generated_name,
                                const std::map<std::size_t,Location>& origins,
                                const std::map<std::string,std::string>& snapshots,
                                std::string_view frontend = {}) {
    auto doc = json::parse(raw);
    if (doc.at("version").text != "2.1.0" || doc.at("runs").kind != 'a' ||
        doc.at("runs").array.size() != 1) throw std::runtime_error("invalid Cephyr SARIF document");
    auto& run = doc.object.at("runs").array.front();
    if (run.at("results").kind != 'a') throw std::runtime_error("missing Cephyr SARIF results");
    const auto generated_uri = file_uri(generated_name);
    std::map<std::string,std::vector<std::size_t>> line_starts;
    for (auto& diagnostic : run.object.at("results").array) {
        if (!diagnostic.has("locations") || diagnostic.at("locations").array.empty()) continue;
        const auto& physical = diagnostic.at("locations").array.front().at("physicalLocation");
        if (physical.at("artifactLocation").at("uri").text != generated_uri || !physical.has("region")) continue;
        const auto c_line = json::natural(physical.at("region").at("startLine"));
        auto it = origins.upper_bound(c_line);
        if (it == origins.begin()) continue;
        const auto& origin = std::prev(it)->second;
        auto source = snapshots.find(origin.file);
        if (source == snapshots.end() || origin.offset > source->second.size() ||
            !origin.line || diagnostic.has("relatedLocations")) continue;
        auto [index, fresh] = line_starts.try_emplace(origin.file);
        auto& starts = index->second;
        if (fresh) {
            starts.push_back(0);
            for (std::size_t i=0;i<source->second.size();++i)
                if (source->second[i]=='\n') starts.push_back(i+1);
        }
        const auto line = origin.line;
        if (line > starts.size() || origin.offset < starts[line-1] ||
            (line < starts.size() && origin.offset >= starts[line])) continue;
        diagnostic.object["relatedLocations"] = json::parse(
            "[{\"id\":1,\"message\":{\"text\":\"Enclosing On1x HIR origin\"},"
            "\"physicalLocation\":{\"artifactLocation\":{\"uri\":" + json::quote(file_uri(origin.file)) +
            "},\"region\":{\"startLine\":" + std::to_string(line) + "}}}]");
    }
    if (!frontend.empty()) {
        auto on1x = json::parse(frontend);
        auto& runs = doc.object.at("runs").array;
        runs.insert(runs.begin(), on1x.at("runs").array.begin(), on1x.at("runs").array.end());
    }
    std::string result;
    write_json(result, doc);
    return result + '\n';
}
}
