#include <on1x/ast.hpp>
#include <on1x/generated_scan.h>
#include <algorithm>
#include <future>
#include <iostream>
#include <string_view>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void location(const on1x::Location& at, const std::string& source, std::size_t offset) {
    auto line = std::count(source.begin(), source.begin() + offset, '\n') + 1;
    auto previous = offset ? source.rfind('\n', offset - 1) : std::string::npos;
    auto column = previous == std::string::npos ? offset + 1 : offset - previous;
    require(at.file == "scanner-test", "lost source filename");
    require(at.offset == offset, "incorrect byte offset");
    require(at.line == static_cast<std::size_t>(line), "incorrect line");
    require(at.column == column, "incorrect byte column");
}

void rejects(const std::string& source, std::size_t offset, std::string_view message) {
    try {
        on1x::scan(source, "scanner-test");
    } catch (const on1x::Error& error) {
        location(error.where, source, offset);
        require(error.what() == message, "incorrect scanner diagnostic");
        return;
    }
    throw std::runtime_error("invalid source was accepted");
}

void repeated_scans() {
    for (int i = 0; i < 32; ++i) {
        rejects("# comment\r\n \t@", 13, "invalid token");
        rejects("abc \"bad\\q\"", 4, "invalid token");
        auto tokens = on1x::scan("# comment\nabc->123 /", "scanner-test");
        require(tokens.size() == 5, "stale parser state or lost tokens");
        require(tokens[0].text == "abc" && tokens[1].text == "->" &&
                tokens[2].text == "123" && tokens[3].text == "/" &&
                tokens[4].text == "<eof>", "incorrect repeated token stream");
        require(tokens[0].where.offset == 10, "stale parser offsets");
    }
}
}

int main() {
    try {
        // The internal C scanner must honor its explicit length, even without
        // a trailing NUL. The last invalid byte is outside the requested range.
        const char input[] = {'a', '+', '1', '@'};
        struct Capture { std::size_t count = 0; bool valid = true; } capture;
        auto collect = [](void* context, const char* text, std::size_t offset) noexcept {
            auto& out = *static_cast<Capture*>(context);
            const char expected[] = {'a', '+', '1'};
            ++out.count;
            if (offset >= 3 || text[0] != expected[offset] || text[1] != '\0')
                out.valid = false;
        };
        std::size_t error = 0;
        require(on1x_generated_scan(input, 3, collect, &capture, &error) == 0,
                "bounded C scanner rejected valid input");
        require(capture.count == 3 && capture.valid, "bounded C scanner read outside its input");
        require(on1x_generated_scan(input, 4, collect, &capture, &error) == 1 && error == 3,
                "bounded C scanner lost error offset");

        std::string source = "# comment\r\n \tname_9 012 \"snow: \xe9\x9b\xaa\" <= >= == != && || -> / []\n# end";
        auto saved = on1x::scan(source, "scanner-test");
        const std::vector<std::string> expected = {
            "name_9", "012", "\"snow: \xe9\x9b\xaa\"", "<=", ">=", "==", "!=",
            "&&", "||", "->", "/", "[", "]"
        };
        require(saved.size() == expected.size() + 1, "incorrect token count");
        std::size_t cursor = 0;
        for (std::size_t i = 0; i < expected.size(); ++i) {
            auto offset = source.find(expected[i], cursor);
            require(offset != std::string::npos, "broken test input");
            require(saved[i].text == expected[i], "incorrect token text");
            location(saved[i].where, source, offset);
            cursor = offset + expected[i].size();
        }
        location(saved.back().where, source, source.size());

        // Returned tokens must not reference the destroyed ASDL arena or source.
        source.assign(source.size(), 'x');
        repeated_scans();
        for (std::size_t i = 0; i < expected.size(); ++i)
            require(saved[i].text == expected[i], "token storage does not own its text");

        rejects(std::string("#\n\"a\0b\"", 7), 4, "NUL in source");
        rejects("\"raw\nnewline\"", 0, "invalid token");
        rejects(std::string(on1x::max_source_bytes + 1, ' '), 0, "source exceeds 2 MiB limit");
        auto limit = on1x::scan(std::string(on1x::max_source_bytes, ' '), "scanner-test");
        require(limit.size() == 1 && limit[0].where.offset == on1x::max_source_bytes,
                "source limit is not inclusive");

        // The generated parser has process-global state; scan() must serialize it.
        std::vector<std::future<void>> workers;
        for (int i = 0; i < 4; ++i)
            workers.push_back(std::async(std::launch::async, repeated_scans));
        for (auto& worker : workers) worker.get();
        std::cout << "Scanner API: owned tokens, locations, limits, repeated/concurrent scans PASS\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
