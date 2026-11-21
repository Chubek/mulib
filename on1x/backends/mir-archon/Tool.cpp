#include "Bridge.hpp"

#include <cerrno>
#include <charconv>
#include <csignal>
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
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
constexpr std::size_t max_input = 16u * 1024u * 1024u;
volatile std::sig_atomic_t interrupted = 0;
void interrupt(int signal) { interrupted = signal; }
int cancelled(void *) { return interrupted != 0; }

std::string_view target_triple() {
#ifdef ON1X_ARCHON_TARGET_TRIPLE
    return ON1X_ARCHON_TARGET_TRIPLE;
#else
    return {};
#endif
}
void check_target(std::string_view requested) {
    const auto configured = target_triple();
    if (configured.empty() || requested != configured)
        throw std::runtime_error("target mismatch: requested " + std::string(requested) +
                                 ", configured " + std::string(configured));
    const std::string_view native = archon_backend_target();
    const bool match =
        (requested == "x86_64-unknown-linux-gnu" && native == "AMD64/SysVAMD64") ||
        (requested == "aarch64-unknown-linux-gnu" && native == "AArch64/AAPCS64") ||
        (requested == "riscv64-unknown-linux-gnu" &&
         (native == "RISCV64IM/LP64" || native == "RISCV64I/LP64"));
    if (!match) throw std::runtime_error("generated backend ABI does not match target");
}
std::size_t byte_limit(std::string_view text) {
    std::size_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        !value || value > max_input)
        throw std::runtime_error("assembly byte limit must be 1..16777216");
    return value;
}
std::string read_all(const std::string &name) {
#if defined(__unix__) || defined(__APPLE__)
    // Nonblocking open plus fstat rejects FIFOs/devices before they can stall
    // cancellation or perform unbounded reads.
    const int fd = ::open(name.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) throw std::runtime_error("cannot read input: " + name);
    try {
        struct stat info{};
        if (::fstat(fd, &info) || !S_ISREG(info.st_mode))
            throw std::runtime_error("MIR input must be a regular file");
        if (info.st_size < 0 || static_cast<std::uintmax_t>(info.st_size) > max_input)
            throw std::runtime_error("MIR input too large");
        std::string bytes;
        char buffer[8192];
        for (;;) {
            if (interrupted) throw std::runtime_error("cancelled");
            const auto count = ::read(fd, buffer, sizeof(buffer));
            if (count < 0 && errno == EINTR) continue;
            if (count < 0) throw std::runtime_error("input read failed");
            if (!count) break;
            const auto n = static_cast<std::size_t>(count);
            if (n > max_input - bytes.size()) throw std::runtime_error("MIR input too large");
            bytes.append(buffer, n);
        }
        ::close(fd);
        return bytes;
    } catch (...) {
        ::close(fd);
        throw;
    }
#else
    std::ifstream f(name, std::ios::binary);
    if (!f) throw std::runtime_error("cannot read input: " + name);
    std::string bytes;
    char buffer[8192];
    while (f) {
        if (interrupted) throw std::runtime_error("cancelled");
        f.read(buffer, sizeof(buffer));
        const auto n = static_cast<std::size_t>(f.gcount());
        if (n > max_input - bytes.size()) throw std::runtime_error("MIR input too large");
        bytes.append(buffer, n);
    }
    if (!f.eof()) throw std::runtime_error("input read failed");
    return bytes;
#endif
}
void write_all(const std::string &name, std::string_view bytes) {
    if (interrupted) throw std::runtime_error("cancelled");
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
            if (interrupted) throw std::runtime_error("cancelled");
            const auto n = ::write(fd, bytes.data() + offset, bytes.size() - offset);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) throw std::runtime_error("artifact write failed");
            offset += static_cast<std::size_t>(n);
        }
        if (::fsync(fd) < 0) throw std::runtime_error("artifact fsync failed");
        if (::close(fd) < 0) { fd = -1; throw std::runtime_error("artifact close failed"); }
        fd = -1;
        if (interrupted) throw std::runtime_error("cancelled");
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
    std::cerr << "usage: on1x-archon-mir --ir-input FILE [--target TRIPLE] [-o OUTPUT.s]\n"
                 "       [--pic] [--no-entry] [--max-assembly-bytes N]\n"
                 "       on1x-archon-mir --print-target | --print-backend-target\n"
                 "The input must be canonical serialized and verified On1x MIR.\n"
                 "This emits assembly only; link it with ScalarABI.c and on1x_runtime.c.\n";
}
} // namespace

int main(int argc, char **argv) try {
    std::string input;
    std::string output = "-";
    std::string target(target_triple());
    on1x::backends::archon::Options options;
    std::signal(SIGINT, interrupt);
    std::signal(SIGTERM, interrupt);
    options.cancelled = cancelled;
    if (argc == 2 && std::string_view(argv[1]) == "--print-target") {
        check_target(target);
        std::cout << target << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--print-backend-target") {
        std::cout << archon_backend_target() << '\n';
        return 0;
    }
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { help(); return 0; }
        if (arg == "--pic") options.pic = true;
        else if (arg == "--no-entry") options.emit_c_entrypoint = false;
        else if ((arg == "-o" || arg == "--ir-input" || arg == "--target" ||
                  arg == "--max-assembly-bytes") && i + 1 < argc) {
            if (arg == "-o") output = argv[++i];
            else if (arg == "--target") target = argv[++i];
            else if (arg == "--max-assembly-bytes") options.max_assembly_bytes = byte_limit(argv[++i]);
            else {
                if (!input.empty()) throw std::runtime_error("duplicate --ir-input");
                input = argv[++i];
            }
        } else throw std::runtime_error("unknown or incomplete argument: " + arg);
    }
    if (input.empty()) { help(); return 2; }
    check_target(target);
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
