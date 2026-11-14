/* Machine-readable differential lexer probe, built against each scanner.
 * Hex text avoids ambiguities from newlines, quotes, or non-UTF-8 source bytes.
 * These binaries are not installed as production tools. */
#include <on1x/ast.hpp>
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    try {
        for (const auto &t : on1x::scan(on1x::read_source_file(argv[1]), argv[1])) {
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
