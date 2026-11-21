#include "Bridge.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
constexpr std::size_t max_input = 16u * 1024u * 1024u;
std::string read_all(const std::string &name) {
    std::ifstream f(name, std::ios::binary);
    if (!f) throw std::runtime_error("cannot read input: " + name);
    std::string bytes;
    char buffer[8192];
    while (f) {
        f.read(buffer, sizeof(buffer));
        const auto n = static_cast<std::size_t>(f.gcount());
        if (n > max_input - bytes.size()) throw std::runtime_error("MIR input too large");
        bytes.append(buffer, n);
    }
    if (!f.eof()) throw std::runtime_error("input read failed");
    return bytes;
}
void write_all(const std::string &name, std::string_view bytes) {
    if (name == "-") {
        std::cout.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!std::cout) throw std::runtime_error("stdout write failed");
        return;
    }
#if defined(__unix__) || defined(__APPLE__)
    // Atomic publication: a failed compiler, crash, or short write cannot
    // expose a partial assembly artifact at the requested output path.
    std::string temporary = name + ".tmp.XXXXXX";
    int fd = mkstemp(temporary.data());
    if (fd < 0) throw std::runtime_error("cannot create output temporary file");
    try {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto n = ::write(fd, bytes.data() + offset, bytes.size() - offset);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) throw std::runtime_error("artifact write failed");
            offset += static_cast<std::size_t>(n);
        }
        if (::fsync(fd) < 0) throw std::runtime_error("artifact fsync failed");
        if (::close(fd) < 0) { fd = -1; throw std::runtime_error("artifact close failed"); }
        fd = -1;
        if (::rename(temporary.c_str(), name.c_str()) < 0)
            throw std::runtime_error("artifact rename failed");
    } catch (...) {
        if (fd >= 0) ::close(fd);
        std::remove(temporary.c_str());
        throw;
    }
#else
    throw std::runtime_error("atomic output file publication requires a POSIX host; use -o -");
#endif
}
void help() {
    std::cerr << "usage: on1x-archon-mir --ir-input FILE [-o OUTPUT.s] [--pic] [--no-entry]\n"
                 "The input must be canonical serialized and verified On1x MIR.\n"
                 "This emits assembly only; link it with ScalarABI.c and on1x_runtime.c.\n";
}
} // namespace

int main(int argc, char **argv) try {
    std::string input;
    std::string output = "-";
    on1x::backends::archon::Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { help(); return 0; }
        if (arg == "--pic") options.pic = true;
        else if (arg == "--no-entry") options.emit_c_entrypoint = false;
        else if ((arg == "-o" || arg == "--ir-input") && i + 1 < argc) {
            if (arg == "-o") output = argv[++i];
            else input = argv[++i];
        } else throw std::runtime_error("unknown or incomplete argument: " + arg);
    }
    if (input.empty()) { help(); return 2; }
    const auto module = on1x::mir::deserialize(read_all(input), input);
    const auto result = on1x::backends::archon::compile(module, options);
    write_all(output, result.assembly);
    return 0;
} catch (const on1x::backends::archon::Error &error) {
    std::cerr << error.where.file << ':' << error.where.line << ':'
              << error.where.column << ": " << error.what() << '\n';
    return 1;
} catch (const on1x::Error &error) {
    std::cerr << error.where.file << ':' << error.where.line << ':'
              << error.where.column << ": " << error.what() << '\n';
    return 1;
} catch (const std::exception &error) {
    std::cerr << "on1x-archon-mir: " << error.what() << '\n';
    return 1;
}
