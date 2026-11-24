#include <on1x/formatter.hpp>
#include <on1x/ast.hpp>
#include <on1x/literal_scan.h>
#include <mulib-prettyemit.h>

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string_view>

namespace on1x::format {
namespace {
namespace fs = std::filesystem;
using View = std::string_view;

struct Config {
    std::size_t width = 80, tabstop = 2;
    std::size_t nested_block_newlines = 1, top_level_block_newlines = 1;
    mulib_pretty_algorithm_t algorithm = MULIB_PRETTY_FITS;
    bool allman = false;
};

bool horizontal(char c) { return c == ' ' || c == '\t' || c == '\r'; }

View trim(View s) {
    while (!s.empty() && horizontal(s.front())) s.remove_prefix(1);
    while (!s.empty() && horizontal(s.back())) s.remove_suffix(1);
    return s;
}

Config read_config(const fs::path& input) {
    Config result;
    for (auto dir = fs::weakly_canonical(fs::absolute(input).parent_path());;) {
        const auto path = dir / ".on1xfmt";
        std::error_code ec;
        const auto entry = fs::symlink_status(path, ec);
        if (entry.type() != fs::file_type::not_found) {
            const auto name = path.string();
            auto fail = [&](std::size_t offset, std::size_t line, const char* message) {
                throw Error({name, offset, line, 1}, message, "ON1X-FMT-CONFIG");
            };
            if (ec || !fs::is_regular_file(path, ec))
                fail(0, 1, ".on1xfmt must be a readable regular file");
            std::ifstream in(path, std::ios::binary);
            char bytes[4097];
            in.read(bytes, sizeof bytes);
            if (in.gcount() == sizeof bytes || !in.eof() || in.bad())
                fail(0, 1, "cannot read .on1xfmt or exceeds 4096 bytes");
            View text(bytes, static_cast<std::size_t>(in.gcount()));
            if (text.find('\0') != View::npos) fail(0, 1, "NUL in .on1xfmt");
            std::set<View> keys;
            for (std::size_t offset = 0, line = 1; offset < text.size(); ++line) {
                auto end = std::min(text.find('\n', offset), text.size());
                auto s = text.substr(offset, end - offset);
                s = trim(s.substr(0, s.find('#')));
                if (!s.empty()) {
                    auto equal = s.find('=');
                    if (equal == View::npos) fail(offset, line, "expected key = value");
                    auto key = trim(s.substr(0, equal));
                    auto val = trim(s.substr(equal + 1));
                    if (!keys.insert(key).second) fail(offset, line, "duplicate .on1xfmt key");
                    if (key == "width" || key == "tabstop" ||
                        key == "nested_block_newlines" || key == "top_level_block_newlines") {
                        std::size_t* setting;
                        std::size_t maximum;
                        const char* message;
                        if (key == "width") {
                            setting = &result.width;
                            maximum = 4096;
                            message = "width must be 1..4096";
                        } else if (key == "tabstop") {
                            setting = &result.tabstop;
                            maximum = 16;
                            message = "tabstop must be 1..16";
                        } else if (key == "nested_block_newlines") {
                            setting = &result.nested_block_newlines;
                            maximum = 16;
                            message = "nested_block_newlines must be 1..16";
                        } else {
                            setting = &result.top_level_block_newlines;
                            maximum = 16;
                            message = "top_level_block_newlines must be 1..16";
                        }
                        auto [ptr, err] = std::from_chars(val.data(), val.data() + val.size(), *setting);
                        if (err != std::errc() || ptr != val.data() + val.size() ||
                            !*setting || *setting > maximum)
                            fail(offset, line, message);
                    } else if (key == "algorithm") {
                        if (val == "fits") result.algorithm = MULIB_PRETTY_FITS;
                        else if (val == "search") result.algorithm = MULIB_PRETTY_SEARCH;
                        else fail(offset, line, "algorithm must be fits or search");
                    } else if (key == "brace_style") {
                        if (val == "same-line") result.allman = false;
                        else if (val == "allman") result.allman = true;
                        else fail(offset, line, "brace_style must be same-line or allman");
                    } else {
                        fail(offset, line, "unknown .on1xfmt key");
                    }
                }
                offset = end + 1;
            }
            return result;
        }
        auto parent = dir.parent_path();
        if (parent == dir) return result;
        dir = std::move(parent);
    }
}

enum class Kind { token, line_comment, block_comment, carrier, disabled };
bool is_comment(Kind kind) { return kind == Kind::line_comment || kind == Kind::block_comment; }

struct Part {
    Kind kind;
    View text;
    std::size_t start, end;
    Location where;
    std::size_t level = 0;
    bool top = false, in_list = false;
};

bool one_of(View text, std::initializer_list<View> words) {
    return std::find(words.begin(), words.end(), text) != words.end();
}

// Use the canonical scanner's byte spans. Only walk trivia between those
// spans; comment markers inside a literal are not trivia.
std::vector<Part> parts(const std::string& text, const std::vector<Token>& tokens, const Unit& unit,
                        std::optional<Target>& source_target) {
    std::set<std::size_t> top;
    auto starts = [&](const auto& declarations) {
        for (const auto& d : declarations) top.insert(d.where.offset);
    };
    for (const auto& item : unit.module_imports) {
        if (item.selective || item.quoted) {
            top.insert(item.where.offset);
        } else {
            // A normal import's AST records each specifier, not the statement
            // start (one `import` can declare multiple modules).
            auto it = std::lower_bound(tokens.begin(), tokens.end(), item.where.offset,
                [](const Token& t, std::size_t start) { return t.where.offset < start; });
            if (it != tokens.begin() && (it - 1)->text == "import")
                top.insert((it - 1)->where.offset);
        }
    }
    starts(unit.kernel_imports); starts(unit.foreign_imports); starts(unit.functions);
    starts(unit.features); starts(unit.ppx); starts(unit.macros); starts(unit.nominals);
    std::vector<const ModuleExtension*> carriers;
    for (const auto& item : unit.extensions) carriers.push_back(&item);
    std::size_t carrier = 0, cursor = 0, line = 1, line_start = 0;
    std::vector<Part> result;
    std::vector<char> delimiters;
    std::size_t braces = 0, brace_depth = 0;
    std::optional<Location> disabled;
    const View source(text);
    auto advance = [&](std::size_t end) {
        for (; cursor < end; ++cursor) if (text[cursor] == '\n') {
            ++line;
            line_start = cursor + 1;
        }
    };
    auto append = [&](Kind kind, std::size_t start, std::size_t end, const Location& where, bool top_level) {
        if (result.size() >= 200000)
            throw Error(where, "formatter item limit exceeded", "ON1X-FMT-LIMIT");
        result.push_back({kind, source.substr(start, end - start), start, end, where,
                          braces, top_level, delimiters.size() > brace_depth});
    };
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        if (token.where.offset < cursor)
            throw Error(token.where, "overlapping token spans", "ON1X-FMT-TOKENS");
        while (cursor < token.where.offset) {
            if (text[cursor] == '#') {
                const auto end = std::min(source.find('\n', cursor), source.size());
                if (end > token.where.offset)
                    throw Error(token.where, "comment overlaps a token", "ON1X-FMT-TOKENS");
                auto comment = source.substr(cursor, end - cursor);
                Location where{token.where.file, cursor, line, cursor - line_start + 1};
                const auto directive = trim(comment.substr(1));
                const bool own_line = trim(source.substr(line_start, cursor - line_start)).empty();
                if (own_line && directive.starts_with("on1xfmt:")) {
                    const auto command = trim(directive.substr(8));
                    if (command == "off" && !disabled)
                        disabled = Location{where.file, line_start, line, 1};
                    else if (command == "on" && disabled) {
                        append(Kind::disabled, disabled->offset, end, *disabled, false);
                        disabled.reset();
                    } else if (command == "on" || command == "off") {
                        throw Error(where, "expected paired # on1xfmt: off / # on1xfmt: on", "ON1X-FMT-DIRECTIVE");
                    } else if (!disabled) {
                        const auto equal = command.find('=');
                        if (equal == View::npos || trim(command.substr(0, equal)) != "target")
                            throw Error(where, "expected formatter directive off, on, or target = plain|latex|troff",
                                        "ON1X-FMT-DIRECTIVE");
                        if (source_target)
                            throw Error(where, "duplicate formatter target directive", "ON1X-FMT-DIRECTIVE");
                        const auto value = trim(command.substr(equal + 1));
                        if (value == "plain") source_target = Target::plain;
                        else if (value == "latex") source_target = Target::latex;
                        else if (value == "troff") source_target = Target::troff;
                        else throw Error(where, "formatter target must be plain, latex or troff", "ON1X-FMT-DIRECTIVE");
                        append(Kind::line_comment, cursor, end, where, false);
                    }
                } else if (!disabled) {
                    append(Kind::line_comment, cursor, end, where, false);
                }
                advance(end);
            } else if (horizontal(text[cursor]) || text[cursor] == '\n') {
                advance(cursor + 1);
            } else if (const auto remaining = token.where.offset - cursor;
                       on1x_comment_kind(source.data() + cursor, remaining)) {
                Location where{token.where.file, cursor, line, cursor - line_start + 1};
                const auto width = on1x_comment_width(source.data() + cursor, remaining);
                if (!width)
                    throw Error(where, "invalid block comment in scanner gap", "ON1X-FMT-TOKENS");
                const auto end = cursor + width;
                if (!disabled) append(Kind::block_comment, cursor, end, where, false);
                advance(end);
            } else {
                throw Error(token.where, "scanner gap is not whitespace or a comment", "ON1X-FMT-TOKENS");
            }
        }
        if (i == tokens.size() - 1) break; // EOF after trailing trivia.

        if (carrier < carriers.size() && carriers[carrier]->where.offset == cursor) {
            // Native extension processors can inspect the source carrier
            // itself, not just its scanner tokens. Treat it as an opaque atom.
            const auto end = carriers[carrier++]->end.offset;
            if (!disabled) append(Kind::carrier, cursor, end, token.where, true);
            advance(end);
            while (tokens[i + 1].where.offset < end) ++i;
            continue;
        }

        const View spelling(token.text);
        if (one_of(spelling, {"}", ")", "]"})) {
            const char expected = spelling == "}" ? '{' : spelling == ")" ? '(' : '[';
            if (delimiters.empty() || delimiters.back() != expected)
                throw Error(token.where, "mismatched formatting delimiter", "ON1X-FMT-TOKENS");
            delimiters.pop_back();
            if (expected == '{') {
                --braces;
                // Only on a brace close; parentheses/brackets cannot close
                // across an unmatched opening brace.
                const auto last = std::find(delimiters.rbegin(), delimiters.rend(), '{');
                brace_depth = static_cast<std::size_t>(delimiters.rend() - last);
            }
        }
        const auto end = cursor + spelling.size();
        if (end > source.size() || source.substr(cursor, spelling.size()) != spelling)
            throw Error(token.where, "token does not match its source bytes", "ON1X-FMT-TOKENS");
        if (!disabled) append(Kind::token, cursor, end, token.where, top.contains(cursor));
        if (one_of(spelling, {"{", "(", "["})) {
            delimiters.push_back(spelling.front());
            if (delimiters.size() > max_syntax_depth)
                throw Error(token.where, "formatter delimiter depth exceeded", "ON1X-FMT-LIMIT");
            if (spelling == "{") {
                ++braces;
                brace_depth = delimiters.size();
            }
        }
        advance(end);
    }
    if (disabled)
        throw Error(*disabled, "unclosed # on1xfmt: off", "ON1X-FMT-DIRECTIVE");
    return result;
}

// The parser intentionally distinguishes qualified names (x.y) from a
// spaced dot in macro syntax. Never change the two adjacency bits around a
// dot, even for uses where the current parser ignores that distinction.
bool space_between(const Part& a, const Part& b) {
    if (a.text == "." || b.text == ".") return a.end != b.start;
    if ((a.text == ":" && one_of(b.text, {":", "="})) || (a.text == ".." && b.text == "=")) return true;
    if (is_comment(a.kind) || is_comment(b.kind)) return true;
    if (one_of(b.text, {")", "]", ",", ";", ":", "?"})) return false;
    if (one_of(a.text, {"(", "[", "@", "$"})) return false;
    if (b.text == "}" && a.text == "{") return false;
    if (b.text == "(" && !one_of(a.text, {
        "if", "while", "for", "return", "with", "=", ":=", "=>", "+", "-", "*", "/", "%",
        "<", ">", "<=", ">=", "==", "!=", "&&", "||", "|", "<-", "~", "in", ",", ":"
    })) return false;
    if (b.text == "[" && (a.end == b.start || a.text == "vector")) return false;
    if (b.text == "!" && a.end == b.start) return false;
    return true;
}

bool continues_block(const Part& p) {
    return one_of(p.text, {
        "else", ";", ",", ")", "]", "?", "(", "[", ".", "=>", "=", "->", ">", ">=",
        "+", "-", "*", "/", "%", "==", "!=", "<", "<=", "&&", "||", "|", "with", "~"
    });
}

bool line_between(const Part& a, const Part& b, View input, const Config& cfg) {
    if (a.kind == Kind::disabled || b.kind == Kind::disabled) return true;
    if (a.kind == Kind::line_comment) return true;
    if (is_comment(b.kind))
        return input.substr(a.end, b.start - a.end).find('\n') != View::npos;
    if (a.kind == Kind::block_comment &&
        input.substr(a.end, b.start - a.end).find('\n') != View::npos) return true;
    if (b.top) return true;
    if (cfg.allman) {
        if (b.text == "{" && (a.text != "." || a.end != b.start)) return true;
        if (a.text == "{" && b.text == "}") return true;
        if (a.text == "}" && b.text == "else") return true;
    }
    if (a.text == "{" && b.text != "}") return true;
    if (b.text == "}" && a.text != "{") return true;
    if (a.text == ";" && !a.in_list) return true;
    if (a.text == "}" && !a.in_list && !continues_block(b)) return true;
    return false;
}

std::size_t newlines_after(const std::vector<Part>& input, std::size_t start,
                           std::size_t end, const Part& next, const Config& cfg) {
    for (auto i = end; i > start;) {
        const auto& part = input[--i];
        if (is_comment(part.kind)) continue;
        if (part.kind == Kind::token &&
            one_of(part.text, {";", ",", ")", "]", "?"}))
            continue;
        const bool closes = part.text == "}" ||
            (part.kind == Kind::carrier && !part.text.empty() && part.text.back() == '}');
        if (!closes || part.in_list || continues_block(next)) return 1;
        return part.level == 0 ? cfg.top_level_block_newlines : cfg.nested_block_newlines;
    }
    return 1;
}

mulib_pretty_style_t style(const Part& p) {
    if (is_comment(p.kind)) return MULIB_PRETTY_COMMENT;
    if (p.kind != Kind::token) return MULIB_PRETTY_NORMAL;
    const auto first = p.text.front();
    if (first == '"' || first == '`' || p.text.starts_with("'''") || p.text.starts_with("#\\") ||
        p.text.starts_with("b!\"") || p.text.starts_with("b!'''") || p.text.starts_with("b!#\\") ||
        p.text.starts_with("r!\"") || p.text.starts_with("r!'''") ||
        (first == '/' && p.text.size() >= 2 && p.text.back() == '/'))
        return MULIB_PRETTY_STRING;
    if (first >= '0' && first <= '9') return MULIB_PRETTY_NUMBER;
    if (one_of(p.text, {
        "fn", "extern", "let", "var", "return", "if", "else", "while", "for", "loop",
        "break", "continue", "true", "false", "import", "from", "as", "forall",
        "macro", "rule", "quote", "splice", "lift", "match", "with", "record", "enum",
        "taskgroup", "launch", "join", "poll", "defer", "unsafe"
    })) return MULIB_PRETTY_KEYWORD;
    return MULIB_PRETTY_NORMAL;
}

using Id = mulib_pretty_id_t;
class Document {
    std::unique_ptr<mulib_prettydoc_t, decltype(&mulib_prettydoc_free)> doc{
        mulib_prettydoc_new(), mulib_prettydoc_free};
    Location file_;
    Id space_, soft_, hard_, empty_;

    Id checked(Id id) const {
        if (id == MULIB_PRETTY_NONE)
            throw Error(file_, "cannot build PrettyPrint document", "ON1X-FMT-DOCUMENT");
        return id;
    }

public:
    explicit Document(std::string name) : file_{std::move(name), 0, 1, 1} {
        space_ = checked(mulib_pretty_text(doc.get(), " "));
        soft_ = checked(mulib_pretty_softline(doc.get()));
        hard_ = checked(mulib_pretty_hardline(doc.get()));
        empty_ = checked(mulib_pretty_text(doc.get(), ""));
    }
    Id cat(Id a, Id b) const { return checked(mulib_pretty_cat(doc.get(), a, b)); }
    Id sequence(std::span<const Id> ids) const {
        if (ids.empty()) return empty_;
        if (ids.size() == 1) return ids.front();
        auto middle = ids.size() / 2;
        return cat(sequence(ids.first(middle)), sequence(ids.subspan(middle)));
    }
    Id group(Id id) const { return checked(mulib_pretty_group(doc.get(), id)); }
    Id nest(std::size_t base, Id id) const { return checked(mulib_pretty_nest(doc.get(), base, id)); }
    Id literal(const Part& p) const {
        auto id = mulib_pretty_verbatim(doc.get(), std::string(p.text).c_str());
        if (id == MULIB_PRETTY_NONE)
            throw Error(p.where, "cannot preserve this text (invalid UTF-8, control byte or allocation failure)",
                        "ON1X-FMT-DOCUMENT");
        const auto annotation = style(p);
        return annotation == MULIB_PRETTY_NORMAL ? id :
            checked(mulib_pretty_annotate(doc.get(), annotation, id));
    }
    Id hard() const { return hard_; }
    Id space() const { return space_; }
    Id soft() const { return soft_; }
    int layout(Id root, const Config& cfg, mulib_pretty_layout_t& result) const {
        mulib_pretty_options_t options;
        mulib_pretty_options_init(&options);
        options.width = cfg.width;
        options.algorithm = cfg.algorithm;
        options.max_output = max_source_bytes;
        return mulib_pretty_layout(doc.get(), root, &options, &result);
    }
};

Id make_document(Document& doc, const std::vector<Part>& input, View original, const Config& cfg) {
    std::vector<Id> lines;
    std::size_t previous_start = 0;
    for (std::size_t start = 0; start < input.size();) {
        const auto& first = input[start];
        auto end = start + 1;
        while (end < input.size() && !line_between(input[end - 1], input[end], original, cfg)) ++end;
        const std::size_t indent = first.kind == Kind::disabled ? 0 : first.level * cfg.tabstop;
        std::vector<Id> text{doc.literal(first)};
        bool can_wrap = false;
        for (auto i = start + 1; i < end; ++i) {
            const auto& a = input[i - 1];
            const auto& b = input[i];
            if (space_between(a, b)) {
                const bool soft = a.text == "," && a.in_list && b.kind == Kind::token;
                can_wrap |= soft;
                text.push_back(soft ? doc.soft() : doc.space());
            }
            text.push_back(doc.literal(b));
        }
        auto body = doc.sequence(text);
        if (can_wrap) body = doc.group(doc.nest(cfg.tabstop, body));
        if (start != 0) {
            const auto count = newlines_after(input, previous_start, start, first, cfg);
            std::vector<Id> prefix(count - 1, doc.hard());
            prefix.push_back(doc.nest(indent, doc.cat(doc.hard(), body)));
            body = doc.sequence(prefix);
        }
        lines.push_back(body);
        previous_start = start;
        start = end;
    }
    if (!lines.empty()) lines.push_back(doc.hard());
    return doc.sequence(lines);
}

void verify_tokens(const std::vector<Token>& before, const std::string& formatted, const std::string& name) {
    auto after = scan(formatted, name);
    auto fail = [&](const Location& where) {
        throw Error(where, "layout would change source tokens or dot adjacency", "ON1X-FMT-TOKENS");
    };
    if (before.size() != after.size()) fail(before.front().where);
    for (std::size_t i = 0; i < before.size(); ++i) {
        if (before[i].text != after[i].text) fail(before[i].where);
        if (i && (before[i - 1].text == "." || before[i].text == ".")) {
            auto adjacent = [i](const auto& v) {
                return v[i - 1].where.offset + v[i - 1].text.size() == v[i].where.offset;
            };
            if (adjacent(before) != adjacent(after) && i + 1 != before.size()) fail(before[i].where);
        }
    }
}
} // namespace

std::string file(const fs::path& input, const Options& options) {
    const auto name = input.string();
    std::error_code ec;
    if (!fs::is_regular_file(input, ec))
        throw Error({name, 0, 1, 1}, "expected regular On1x source file", "ON1X-FMT-INPUT");
    const auto cfg = read_config(input);
    const auto source = read_source_file(name);
    const auto tokens = scan(source, name);
    if (tokens.size() > 200001)
        throw Error({name, 0, 1, 1}, "formatter token limit exceeded", "ON1X-FMT-LIMIT");
    const auto unit = parse(tokens, options.enabled_extensions);
    std::optional<Target> source_target;
    const auto items = parts(source, tokens, unit, source_target);
    Document doc(name);
    const auto root = make_document(doc, items, source, cfg);
    struct Layout {
        mulib_pretty_layout_t value{};
        ~Layout() { mulib_pretty_layout_free(&value); }
    } layout;
    if (!doc.layout(root, cfg, layout.value))
        throw Error({name, 0, 1, 1}, "PrettyPrint layout failed (size, work, or indentation limit)",
                    "ON1X-FMT-LIMIT");
    const std::string plain(layout.value.text, layout.value.length);
    verify_tokens(tokens, plain, name);
    const auto mode = options.target.value_or(source_target.value_or(Target::plain));
    if (mode == Target::plain) return plain;
    if (mode != Target::latex && mode != Target::troff)
        throw Error({name, 0, 1, 1}, "unknown formatting target", "ON1X-FMT-TARGET");
    const auto target = mode == Target::latex ? MULIB_PRETTY_LATEX : MULIB_PRETTY_TROFF;
    std::unique_ptr<char, decltype(&std::free)> markup(mulib_pretty_emit(&layout.value, target), std::free);
    if (!markup)
        throw Error({name, 0, 1, 1}, "cannot emit PrettyPrint markup", "ON1X-FMT-DOCUMENT");
    return markup.get();
}
} // namespace on1x::format
