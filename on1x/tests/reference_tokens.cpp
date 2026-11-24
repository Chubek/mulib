/* Machine-readable differential lexer probe, built against each scanner.
 * Hex text avoids ambiguities from newlines, quotes, or non-UTF-8 source bytes.
 * These binaries are not installed as production tools. */
#include <on1x/ast.hpp>
#include <algorithm>
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    try {
        std::vector<on1x::Docstring> docs;
        auto tokens = on1x::scan(on1x::read_source_file(argv[1]), argv[1], &docs);
        for (const auto &d : docs)
            tokens.push_back({std::string("{") + d.marker + d.text + "}", d.where});
        std::stable_sort(tokens.begin(), tokens.end(), [](const auto& a, const auto& b) {
            return a.where.offset < b.where.offset;
        });
        for (const auto &t : tokens) {
            std::cout << t.where.offset << ':' << t.where.line << ':' << t.where.column << ' ';
            for (unsigned char byte : t.text) {
                constexpr char hex[] = "0123456789abcdef";
                std::cout << hex[byte >> 4] << hex[byte & 15];
            }
            std::cout << '\n';
        }
    } catch (const on1x::Error &e) {
        std::cerr << e.where.offset << ':' << e.where.line << ':' << e.where.column
                  << ": error: " << e.what() << '\n';
        return 1;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
    return 0;
}
