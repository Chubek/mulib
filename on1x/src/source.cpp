#include <on1x/ast.hpp>
#include <algorithm>
#include <array>
#include <fstream>

namespace on1x {
std::string read_source_file(const std::string& filename) {
    std::ifstream input(filename, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open " + filename);
    std::string source;
    std::array<char, 8192> chunk;
    // Read at most one byte beyond the limit, including for non-regular files.
    while (input) {
        auto count = std::min(chunk.size(), max_source_bytes + 1 - source.size());
        input.read(chunk.data(), static_cast<std::streamsize>(count));
        source.append(chunk.data(), static_cast<std::size_t>(input.gcount()));
        if (source.size() > max_source_bytes)
            throw Error({filename, 0, 1, 1}, "source exceeds 2 MiB limit");
    }
    if (input.bad()) throw std::runtime_error("cannot read " + filename);
    return source;
}
}
