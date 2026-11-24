// Private, declaration/definition-scoped numeric queries. No C is executed.
#include <wetstone/abstract-interp/BitvectorInterval.hpp>
#include <wetstone/abstract-interp/Interval.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {
using wetstone::ai::BitvectorInterval;
using wetstone::ai::Interval;
using Wide = __int128;

constexpr std::string_view protocol = "wetstone-ffi-obligations 1";
constexpr std::size_t max_queries = 4096;

template<class T>
bool number(std::string_view text, T& result) {
    const auto* end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool bound(std::string_view text, Wide& result) {
    if (text.starts_with('-')) {
        std::int64_t value{};
        if (!number(text, value))
            return false;
        result = value;
    } else {
        std::uint64_t value{};
        if (!number(text, value))
            return false;
        result = value;
    }
    return true;
}

// A C mathematical range can cross zero and include UINT64_MAX. Keep the
// negative and nonnegative halves in the respective Wetstone domains without
// clamping, reinterpreting bit patterns, or converting any endpoint out of range.
Interval negative(Wide lo, Wide hi) {
    if (lo >= 0)
        return Interval::bottom();
    return {static_cast<std::int64_t>(lo), static_cast<std::int64_t>(std::min(hi, Wide(-1)))};
}

BitvectorInterval nonnegative(Wide lo, Wide hi) {
    if (hi < 0)
        return BitvectorInterval::bottom(64);
    return {64, static_cast<std::uint64_t>(std::max(lo, Wide(0))), static_cast<std::uint64_t>(hi), false};
}

std::string_view check_range(const std::array<std::string_view, 4>& fields) {
    Wide lo{}, hi{}, accepted_lo{}, accepted_hi{};
    if (!bound(fields[0], lo) || !bound(fields[1], hi) ||
        !bound(fields[2], accepted_lo) || !bound(fields[3], accepted_hi) ||
        lo > hi || accepted_lo > accepted_hi)
        return {};
    const bool contained = negative(lo, hi).leq(negative(accepted_lo, accepted_hi)) &&
        nonnegative(lo, hi).leq(nonnegative(accepted_lo, accepted_hi));
    // The source interval may be an overapproximation of actual values.
    return contained ? "proved" : "unknown";
}

std::string_view check_array(const std::array<std::string_view, 4>& fields) {
    std::uint64_t minimum{}, element_bytes{}, available_lo{}, available_hi{};
    if (!number(fields[0], minimum) || !number(fields[1], element_bytes) ||
        !number(fields[2], available_lo) || !number(fields[3], available_hi) ||
        !minimum || !element_bytes || available_lo > available_hi)
        return {};
    // floor(available_bytes / element_bytes) is monotone and exact on these
    // endpoints. Unlike multiplying the minimum by the size, it cannot wrap.
    const BitvectorInterval available{64, available_lo, available_hi, false};
    const auto capacity = available.div(BitvectorInterval::point(element_bytes, 64));
    const auto outcome = BitvectorInterval::point(minimum, 64).less_equal(capacity);
    if (outcome == BitvectorInterval::point(1, 1))
        return "proved";
    if (outcome == BitvectorInterval::point(0, 1))
        return "disproved";
    return "unknown";
}

std::string_view check(std::string_view line) {
    const auto first = line.find(' ');
    if (first == line.npos)
        return {};
    const auto name = line.substr(0, first);
    line.remove_prefix(first + 1);
    std::array<std::string_view, 4> fields;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto split = line.find(' ');
        if (line.empty() || split == 0 || (split == line.npos) != (i + 1 == fields.size()))
            return {};
        fields[i] = line.substr(0, split);
        if (split != line.npos)
            line.remove_prefix(split + 1);
    }
    if (name == "range")
        return check_range(fields);
    if (name == "array")
        return check_array(fields);
    return {};
}
} // namespace

int main() {
    // Longest valid line: command + 4 space-separated decimal bounds.
    char line[96];
    if (!std::cin.getline(line, sizeof(line)) || std::cin.eof() ||
        std::string_view(line, static_cast<std::size_t>(std::cin.gcount() - 1)) != protocol)
        return 2;
    std::cout << protocol << '\n';
    for (std::size_t row = 0; ; ++row) {
        std::cin.getline(line, sizeof(line));
        if (std::cin.eof() && std::cin.gcount() == 0)
            return std::cout ? 0 : 4;
        if (!std::cin || std::cin.eof() || row >= max_queries)
            return 2;
        const auto verdict = check({line, static_cast<std::size_t>(std::cin.gcount() - 1)});
        if (verdict.empty())
            return 2;
        if (!(std::cout << verdict << '\n'))
            return 4;
    }
}
