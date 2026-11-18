#include "bridge.h"
#include <liblr/LexerGenerator.hpp>
#include <liblr/LLTableDriven.hpp>
#include <liblr/ParserGenerator.hpp>
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "grammar_resources.hpp"

namespace {
namespace lr = firestone::lr;
std::string published;
char failure[512];
std::size_t requests = 0;
struct Rejection { const char *text; std::size_t offset; };
[[noreturn]] void reject(const char *text, std::size_t offset = 0) { throw Rejection{text, offset}; }
bool letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool digit(char c) { return c >= '0' && c <= '9'; }
bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
std::string quoted(std::string_view s) {
    std::string out = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c >= 32 && c <= 126) out += static_cast<char>(c);
        else { out += "\\x"; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out + '"';
}
std::string regex_literal(std::string_view s) {
    std::string out;
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : s) {
        out += "\\x"; out += hex[c >> 4]; out += hex[c & 15];
    }
    return out;
}
struct Reader {
    std::string_view source;
    std::size_t pos = 0, origin = 0;
    void skip() {
        while (pos < source.size()) {
            if (space(source[pos])) ++pos;
            else if (source[pos] == '#') { while (pos < source.size() && source[pos] != '\n') ++pos; }
            else break;
        }
    }
    char peek() { skip(); return pos == source.size() ? '\0' : source[pos]; }
    bool take(char c) { if (peek() != c) return false; ++pos; return true; }
    void need(char c) { if (!take(c)) reject(messages_syntax, origin + pos); }
    std::string name() {
        skip();
        const auto begin = pos;
        if (pos == source.size() || !letter(source[pos])) reject(messages_name, origin + pos);
        while (pos < source.size() && (letter(source[pos]) || digit(source[pos]))) ++pos;
        auto name = std::string(source.substr(begin, pos - begin));
        if (name.size() > limits_name_bytes || name.starts_with("ge_")) reject(messages_name, origin + begin);
        return name;
    }
    std::string literal() {
        const char quote = peek(); ++pos;
        std::string out;
        while (pos < source.size()) {
            char c = source[pos++];
            if (c == quote) {
                if (out.empty()) reject(messages_syntax, origin + pos);
                return out;
            }
            if (c == '\\') {
                if (pos == source.size()) reject(messages_syntax, origin + pos);
                c = source[pos++];
                if (c == 'n') c = '\n';
                else if (c == 'r') c = '\r';
                else if (c == 't') c = '\t';
                else if (c != '\\' && c != '\'' && c != '"') reject(messages_syntax, origin + pos);
            }
            if (!c) reject(messages_syntax, origin + pos);
            out += c;
        }
        reject(messages_syntax, origin + pos);
    }
};
struct Definition { std::string name, body; bool token, skip; std::size_t offset; };
struct Construct {
    std::string name, lexer = "dfa", parser = "lalr1", start = "TOP";
    std::vector<Definition> definitions;
    std::map<std::string, std::size_t> names;
    std::map<std::string, std::string> literals;
    std::vector<std::pair<std::string, std::string>> literal_order;
    std::map<std::string, std::string> regex_cache;
    std::set<std::string> active;
    explicit Construct(std::string_view source) {
        Reader r{source};
        if (r.name() != "grammar") reject(messages_syntax);
        name = r.name();
        std::set<std::string> options;
        while (r.take(':')) {
            auto key = r.name(); r.need('<'); auto value = r.name(); r.need('>');
            if (!options.insert(key).second) reject(messages_option, r.pos);
            if (key == "lexer" && (value == "dfa" || value == "nfa")) lexer = value;
            else if (key == "parser" && (value == "lr1" || value == "lalr1" || value == "ll1")) parser = value;
            else if (key == "start") start = value;
            else reject(messages_option, r.pos);
        }
        r.need('{');
        while (!r.take('}')) {
            const auto kind = r.name();
            if (kind != "token" && kind != "rule") reject(messages_syntax, r.pos);
            Definition d{r.name(), {}, kind == "token", false, 0};
            if (r.take(':')) {
                if (!d.token || r.name() != "skip") reject(messages_option, r.pos);
                d.skip = true;
            }
            r.need('{'); d.offset = r.pos;
            // A closing brace in quotes, a class or an escaped byte is data.
            char quote = 0; bool escaped = false, in_class = false;
            auto begin = r.pos;
            for (; r.pos < source.size(); ++r.pos) {
                char c = source[r.pos];
                if (escaped) { escaped = false; continue; }
                if (c == '\\') { escaped = true; continue; }
                if (quote) { if (c == quote) quote = 0; continue; }
                if (!in_class && (c == '\'' || c == '"')) { quote = c; continue; }
                if (!in_class && c == '#') {
                    while (r.pos < source.size() && source[r.pos] != '\n') ++r.pos;
                    if (r.pos == source.size()) break;
                    continue;
                }
                if (d.token && c == '[') in_class = true;
                if (d.token && c == ']') in_class = false;
                if (!in_class && c == '}') break;
            }
            if (r.pos == source.size()) reject(messages_syntax, r.pos);
            d.body = std::string(source.substr(begin, r.pos - begin)); ++r.pos;
            r.take(';');
            if (!names.emplace(d.name, definitions.size()).second) reject(messages_duplicate, d.offset);
            definitions.push_back(std::move(d));
            if (definitions.size() > limits_definitions) reject(messages_limit, r.pos);
        }
        if (r.peek()) reject(messages_syntax, r.pos);
        if (!options.contains("start") && !names.contains(start)) {
            for (const auto& d : definitions) if (!d.token) { start = d.name; break; }
        }
        if (!names.contains(start) || definitions[names.at(start)].token) reject(messages_reference);
    }
    const Definition& reference(Reader& r) {
        const auto key = r.name(); r.need('>');
        if (!names.contains(key)) reject(messages_reference, r.origin + r.pos);
        return definitions[names.at(key)];
    }
    std::string regex(const Definition& d) {
        if (auto it = regex_cache.find(d.name); it != regex_cache.end()) return it->second;
        if (!active.insert(d.name).second) reject(messages_recursive_token, d.offset);
        Reader r{d.body, 0, d.offset}; std::string out;
        while (char c = r.peek()) {
            if (c == '\'' || c == '"') out += regex_literal(r.literal());
            else if (r.take('<')) {
                if (r.take('[')) {
                    out += '[';
                    bool closed = false;
                    while (r.pos < r.source.size()) {
                        char b = r.source[r.pos++]; out += b;
                        if (b == '\\' && r.pos < r.source.size()) out += r.source[r.pos++];
                        else if (b == ']') { closed = true; break; }
                    }
                    if (!closed) reject(messages_syntax, r.origin + r.pos);
                    r.need('>');
                } else {
                    const auto& ref = reference(r);
                    if (!ref.token) reject(messages_reference, r.origin + r.pos);
                    out += '(' + regex(ref) + ')';
                }
            } else if (r.take('\\')) {
                if (r.pos == r.source.size()) reject(messages_syntax, r.origin + r.pos);
                c = r.source[r.pos++];
                if (c == 'd') out += "[0-9]";
                else if (c == 's') out += "[ \\t\\r\\n]";
                else if (c == 'w') out += "[A-Za-z_0-9]";
                else { out += '\\'; out += c; }
            } else if (r.take('[')) {
                out += '['; bool closed = false;
                while (r.pos < r.source.size()) {
                    char b = r.source[r.pos++]; out += b;
                    if (b == '\\' && r.pos < r.source.size()) out += r.source[r.pos++];
                    else if (b == ']') { closed = true; break; }
                }
                if (!closed) reject(messages_syntax, r.origin + r.pos);
            } else { out += c; ++r.pos; }
            if (out.size() > limits_regex) reject(messages_limit, r.origin + r.pos);
        }
        active.erase(d.name); regex_cache.emplace(d.name, out); return out;
    }
    std::string ebnf() {
        std::string out = "%start " + start + ";\n";
        for (const auto& d : definitions) {
            if (d.token) continue;
            Reader r{d.body, 0, d.offset}; out += d.name + " = ";
            std::vector<char> groups;
            while (char c = r.peek()) {
                if (c == '\'' || c == '"') {
                    auto text = r.literal();
                    auto [it, inserted] = literals.emplace(text, "literal" + std::to_string(literals.size()));
                    if (inserted) literal_order.emplace_back(it->second, text);
                    out += "'" + it->second + "' ";
                } else if (r.take('<')) {
                    const auto& ref = reference(r);
                    if (ref.skip) reject(messages_reference, r.origin + r.pos);
                    out += ref.token ? "'token_" + ref.name + "' " : ref.name + " ";
                } else {
                    if (std::string_view("()[]|?*+").find(c) == std::string_view::npos)
                        reject(messages_syntax, r.origin + r.pos);
                    if (c == '(' || c == '[') {
                        groups.push_back(c); if (groups.size() > limits_group_depth) reject(messages_limit, r.origin + r.pos);
                    }
                    if (c == ')' || c == ']') {
                        if (groups.empty() || groups.back() != (c == ')' ? '(' : '['))
                            reject(messages_syntax, r.origin + r.pos);
                        groups.pop_back();
                    }
                    out += c == '[' ? '(' : c == ']' ? ')' : c; ++r.pos;
                }
            }
            if (!groups.empty()) reject(messages_syntax, r.origin + r.pos);
            out += ";\n";
        }
        return out;
    }
};
// Four printable nibbles encode a cell + 1; zero represents an absent cell.
void cell(std::string& out, long value) {
    if (value < -1 || value >= 65534) reject(messages_limit);
    const auto n = static_cast<unsigned>(value + 1);
    for (int shift = 12; shift >= 0; shift -= 4) out += static_cast<char>('A' + ((n >> shift) & 15));
}
std::string cells(std::initializer_list<long> values) {
    std::string out; for (auto v : values) cell(out, v); return out;
}
std::vector<lr::StateId> closure(const lr::NFA& nfa, lr::StateId from) {
    std::set<lr::StateId> seen{from}; std::vector<lr::StateId> work{from};
    for (std::size_t i = 0; i < work.size(); ++i)
        for (auto next : nfa.states()[work[i]].epsilon)
            if (seen.insert(next).second) work.push_back(next);
    return {seen.begin(), seen.end()};
}
std::string compile(std::string_view source) {
    Construct spec(source);
    auto grammar = lr::readEBNF(spec.ebnf());
    if (grammar.symbols().size() > limits_symbols || grammar.productions().size() > limits_productions)
        reject(messages_limit);
    std::vector<lr::LexerRule> rules;
    for (const auto& [name, text] : spec.literal_order)
        rules.emplace_back(regex_literal(text), *grammar.findTerminal(name));
    for (const auto& d : spec.definitions) {
        if (!d.token) continue;
        const auto pattern = spec.regex(d); // Validate helper-only tokens as well.
        (void)lr::compile_regex(pattern);
        auto token = grammar.findTerminal("token_" + d.name);
        if (!token && !d.skip) continue;
        if (!token) token = grammar.terminal("token_" + d.name);
        rules.emplace_back(pattern, *token, d.skip);
    }
    if (grammar.symbols().size() > limits_symbols) reject(messages_limit);
    lr::NFA nfa;
    std::size_t regex_bytes = 0;
    for (std::size_t i = 0; i < rules.size(); ++i) {
        const auto& rule = rules[i]; regex_bytes += rule.pattern.size();
        if (regex_bytes > limits_regex) reject(messages_limit);
        auto part = lr::compile_regex(rule.pattern, {rule.token, i, rule.skip});
        const auto base = nfa.states().size();
        if (base + part.states().size() > limits_nfa_states) reject(messages_limit);
        for (std::size_t j = 0; j < part.states().size(); ++j) nfa.add_state();
        for (std::size_t j = 0; j < part.states().size(); ++j) {
            const auto& state = part.states()[j];
            for (auto to : state.epsilon) nfa.add_epsilon(base+j, base+to);
            for (const auto& edge : state.edges) nfa.add_transition(base+j, edge.bytes, base+edge.target);
            nfa.set_acceptance(base+j, state.acceptance);
        }
        nfa.add_epsilon(nfa.start(), base+part.start());
    }
    for (auto state : closure(nfa, nfa.start()))
        if (nfa.states()[state].acceptance) reject(messages_nullable);
    std::string transitions, accepts, closures, edges;
    long lex_start = 0, lex_states = 0;
    if (spec.lexer == "dfa") {
        auto dfa = lr::minimize(lr::determinize(nfa));
        if (dfa.states().size() > limits_dfa_states) reject(messages_limit);
        lex_start = static_cast<long>(dfa.start()); lex_states = static_cast<long>(dfa.states().size());
        for (const auto& state : dfa.states()) {
            for (auto to : state.transitions) cell(transitions, to == lr::no_state ? -1 : static_cast<long>(to));
            cell(accepts, state.acceptance ? static_cast<long>(state.acceptance->token) : -1);
            cell(accepts, state.acceptance && state.acceptance->skip ? 1 : 0);
            cell(accepts, state.acceptance ? static_cast<long>(state.acceptance->priority) : -1);
        }
    } else {
        lex_states = static_cast<long>(nfa.states().size());
        for (std::size_t i = 0; i < nfa.states().size(); ++i) {
            const auto closed = closure(nfa, i);
            for (std::size_t j = 0; j < nfa.states().size(); ++j)
                closures += std::binary_search(closed.begin(), closed.end(), j) ? '1' : '0';
            const auto& state = nfa.states()[i];
            cell(accepts, state.acceptance ? static_cast<long>(state.acceptance->token) : -1);
            cell(accepts, state.acceptance && state.acceptance->skip ? 1 : 0);
            cell(accepts, state.acceptance ? static_cast<long>(state.acceptance->priority) : -1);
            for (const auto& edge : state.edges) {
                for (std::size_t b = 0; b < 256; ++b) {
                    if (!edge.bytes[b]) continue;
                    auto end = b; while (end+1 < 256 && edge.bytes[end+1]) ++end;
                    edges += cells({static_cast<long>(i), static_cast<long>(b),
                                    static_cast<long>(end), static_cast<long>(edge.target)});
                    b = end;
                }
            }
        }
    }
    std::string actions, gotos, productions, rhs, names, terminals;
    long parse_states = 0;
    if (spec.parser == "ll1") {
        lr::LLTable table(grammar);
        if (!table.leftRecursion().empty()) reject(messages_left_recursion);
        if (!table.conflicts().empty()) reject(messages_conflict);
        parse_states = static_cast<long>(grammar.symbols().size());
        for (lr::Symbol row = 0; row < grammar.symbols().size(); ++row)
            for (lr::Symbol col = 0; col < grammar.symbols().size(); ++col) {
                auto choice = table.lookup(row, col);
                cell(actions, choice ? static_cast<long>(*choice) : -1);
            }
    } else {
        auto table = lr::generateLRTable(grammar, spec.parser == "lr1" ? lr::LRMode::canonicalLR1 : lr::LRMode::lalr1);
        if (table.stateCount() > limits_lr_states) reject(messages_limit);
        if (!table.conflicts().empty()) reject(messages_conflict);
        parse_states = static_cast<long>(table.stateCount());
        for (std::size_t row = 0; row < table.stateCount(); ++row)
            for (lr::Symbol col = 0; col < grammar.symbols().size(); ++col) {
                const auto& entries = table.actions(row, col);
                long kind = -1, target = -1;
                if (!entries.empty()) {
                    const auto& a = entries.front(); target = static_cast<long>(a.target);
                    kind = a.kind == lr::LRActionKind::shift ? 0 : a.kind == lr::LRActionKind::reduce ? 1 : 2;
                }
                cell(actions, kind); cell(actions, target);
                auto go = table.goTo(row, col); cell(gotos, go ? static_cast<long>(*go) : -1);
            }
    }
    for (const auto& p : grammar.productions()) {
        if (p.rhs.size() > limits_rhs) reject(messages_limit);
        productions += cells({static_cast<long>(p.lhs), static_cast<long>(p.rhs.size()), static_cast<long>(rhs.size()/4)});
        for (auto s : p.rhs) cell(rhs, static_cast<long>(s));
    }
    // Names have explicit offsets; arbitrary terminal bytes never become identifiers.
    std::string offsets;
    for (const auto& symbol : grammar.symbols()) {
        std::string name = symbol.name;
        if (symbol.terminal) {
            if (name.starts_with("token_")) name.erase(0, 6);
            else for (const auto& [internal, literal] : spec.literal_order)
                if (name == internal) { name = literal; break; }
        }
        offsets += cells({static_cast<long>(names.size()), static_cast<long>(name.size())});
        names += name; terminals += symbol.terminal ? '1' : '0';
    }
    auto config = cells({spec.lexer == "nfa" ? 1 : 0, spec.parser == "ll1" ? 1 : 0,
        lex_start, lex_states, static_cast<long>(grammar.symbols().size()), static_cast<long>(grammar.start()),
        parse_states, 0, static_cast<long>(limits_tokens),
        static_cast<long>(limits_tree_depth), static_cast<long>(statuses_ok), static_cast<long>(statuses_lexical),
        static_cast<long>(statuses_syntax), static_cast<long>(statuses_limit)});
    // Input byte/step limits exceeding the cell representation are separate literals.
    std::string plan = "(" + quoted(spec.name);
    for (const auto& [key, value] : std::vector<std::pair<std::string,std::string>>{
        {"@CONFIG@",config}, {"@TRANSITIONS@",transitions}, {"@ACCEPTS@",accepts},
        {"@CLOSURES@",closures}, {"@EDGES@",edges}, {"@ACTIONS@",actions}, {"@GOTOS@",gotos},
        {"@PRODUCTIONS@",productions}, {"@RHS@",rhs}, {"@NAMES@",names}, {"@OFFSETS@",offsets},
        {"@TERMINALS@",terminals}, {"@STEPS@",std::to_string(limits_steps)},
        {"@INPUT@",std::to_string(limits_input_bytes)}}) {
        plan += " (" + quoted(key) + " " + quoted(value) + ")";
    }
    plan += ")";
    if (plan.size() > limits_table_bytes) reject(messages_limit);
    return plan;
}
}

extern "C" bool on1x_grammar_compile(const char *source) {
    published.clear(); failure[0] = 0;
    try {
        if (!source) reject(messages_input);
        std::size_t length = 0;
        while (length <= limits_source && source[length]) ++length;
        if (length > limits_source) reject(messages_input);
        if (++requests > limits_grammars) reject(messages_limit);
        published = compile(std::string_view(source, length)); return true;
    } catch (const Rejection& e) {
        std::snprintf(failure, sizeof failure, "%s @ %zu", e.text, e.offset);
    } catch (const std::invalid_argument&) { std::snprintf(failure, sizeof failure, "%s", messages_liblr);
    } catch (const std::exception&) { std::snprintf(failure, sizeof failure, "%s", messages_internal);
    } catch (...) { std::snprintf(failure, sizeof failure, "%s", messages_internal); }
    published.clear(); return false;
}
extern "C" const char *on1x_grammar_plan(void) { return published.c_str(); }
extern "C" const char *on1x_grammar_error(void) { return failure; }
extern "C" const char *on1x_grammar_template(void) { return reinterpret_cast<const char*>(machinery); }
extern "C" const char *on1x_grammar_message(const char *key) {
    if (key && std::string_view(key) == "marker") return messages_marker;
    if (key && std::string_view(key) == "argument") return messages_argument;
    if (key && std::string_view(key) == "collision") return messages_collision;
    return messages_internal;
}
