#pragma once
#include <on1x/ast.hpp>

namespace on1x::regex {
inline constexpr std::size_t max_pattern_bytes = 256;
struct Edge { unsigned first, last; std::size_t target; };
struct State { std::vector<Edge> edges; bool accepts = false; };
struct Span { std::size_t begin, end; }; // Relative Unicode scalar offsets.
struct Program {
 std::vector<State> states;
 std::vector<Span> groups; // Does not include whole-match group zero.
 bool capturable = true;
};
class Cache {
 std::map<std::string,Program> patterns;
 std::size_t total_states = 0, total_edges = 0, work_left = 32 * 1024 * 1024;
public:
 const Program& get(const std::string& literal,const Location& at);
};
void check_capture(const Program& program,const Location& at);
}
