#include "archvirt_driver.hpp"
#include <on1x/archvirt_cache.hpp>
#include "../archvirt/linked_format.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <system_error>
#include <vector>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace on1x::archvirt_driver {
namespace {
namespace fs = std::filesystem;
constexpr auto profile = "on1x-domios-v1-x86_64-kvm";

struct Assets {
    fs::path bin = fs::canonical("/proc/self/exe").parent_path();
    bool build = [&] {
        std::error_code ec;
        return fs::equivalent(bin, fs::path(ON1X_ARCHVIRT_BUILD_BIN), ec);
    }();
    fs::path source = build ? fs::path(ON1X_ARCHVIRT_BUILD_SOURCE) : bin / "../share/on1x/archvirt";
    fs::path archive = build ? fs::path(ON1X_ARCHVIRT_BUILD_ARCHIVE) : source / "on1x-archvirt-runtime.a";
    fs::path launcher = bin / (build ? "on1x-archvirt-wrapper" : "../libexec/on1x/on1x-archvirt-wrapper");
    fs::path runner = bin / (build ? "on1x-archvirt-run" : "../libexec/on1x/on1x-archvirt-run");
};

std::string read_binary(const fs::path& path, std::size_t limit, const char* kind) {
    if (!fs::is_regular_file(path))
        throw std::runtime_error(std::string("ArchVirt ") + kind + " unavailable: " + path.string());
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(limit))
        throw std::runtime_error(std::string("ArchVirt ") + kind + " is unreadable or exceeds its size limit");
    std::string bytes(static_cast<std::size_t>(size), '\0');
    file.seekg(0);
    if (!file.read(bytes.data(), size) || file.peek() != std::char_traits<char>::eof())
        throw std::runtime_error(std::string("cannot read ArchVirt ") + kind);
    return bytes;
}

std::string runtime_archive(const Assets& assets) {
    auto archive = read_binary(assets.archive, ON1X_VM_GUEST_MAX, "guest runtime archive");
    if (!archive.starts_with("!<arch>\n"))
        throw std::runtime_error("invalid ArchVirt guest runtime archive");
    return archive;
}

class Scratch {
public:
    Scratch() {
        auto name = (fs::temp_directory_path() / "on1x-archvirt-build-XXXXXX").string();
        if (!::mkdtemp(name.data()))
            throw std::system_error(errno, std::generic_category(), "cannot stage ArchVirt build");
        directory = name;
    }
    ~Scratch() {
        std::error_code ignored;
        fs::remove_all(directory, ignored);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;
    fs::path directory;
};

void write_binary(const fs::path& path, std::string_view bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
    if (!file) throw std::runtime_error("cannot write private ArchVirt guest build input");
}

int run_tool(std::vector<std::string> args) {
    std::vector<char*> argv;
    for (auto& arg : args) argv.push_back(arg.data());
    argv.push_back(nullptr);
    pid_t pid;
    if (const auto error = ::posix_spawn(&pid, argv[0], nullptr, nullptr, argv.data(), environ))
        throw std::system_error(error, std::generic_category(), "cannot start ArchVirt tool");
    int status;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            throw std::system_error(errno, std::generic_category(), "cannot wait for ArchVirt tool");
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

struct StagedImage {
    Scratch scratch;
    fs::path path = scratch.directory / "program.elf";
    explicit StagedImage(std::string_view bytes) { write_binary(path, bytes); }
    int invoke(bool execute) const {
        std::vector<std::string> args{Assets{}.runner.string()};
        if (!execute) args.emplace_back("--check");
        args.push_back(path.string());
        return run_tool(std::move(args));
    }
};

int remember_and_run(const Guest& guest, const StagedImage& staged) {
    auto key = archvirt_cache::publish({profile, guest.archive_sha256}, guest.image);
    std::cerr << "on1xc: ArchVirt cache key: " << key << '\n';
    return staged.invoke(true);
}
}

Guest compile_guest(std::string_view generated) {
    Assets assets;
    Scratch scratch;
    const auto source = scratch.directory / "program.c";
    const auto archive = scratch.directory / "runtime.a";
    const auto output = scratch.directory / "program.elf";
    const auto runtime = runtime_archive(assets);
    write_binary(source, generated);
    write_binary(archive, runtime);
    if (run_tool({ON1X_ARCHVIRT_PYTHON, "-IB", (assets.source / "compile.py").string(),
                 "--clang", ON1X_ARCHVIRT_CLANG, "--lld", ON1X_ARCHVIRT_LLD,
                 "--runtime-include", (assets.source.parent_path() / "runtime").string(),
                 "--archvirt-include", ON1X_ARCHVIRT_INCLUDE, "--runtime-archive", archive.string(),
                 source.string(), "-o", output.string()}))
        throw GuestBuildError("ArchVirt guest build failed; no host executable or VM fallback was produced");
    return {read_binary(output, ON1X_VM_GUEST_MAX, "guest ELF"), archvirt_cache::sha256(runtime)};
}

std::string linked_program(std::string_view guest, bool always) {
    if (guest.empty() || guest.size() > ON1X_VM_GUEST_MAX)
        throw std::runtime_error("ArchVirt guest size exceeds the launcher profile");
    auto output = read_binary(Assets{}.launcher, ON1X_VM_LAUNCHER_MAX, "linked launcher template");
    if (!output.starts_with("\177ELF"))
        throw std::runtime_error("invalid ArchVirt linked launcher template");
    output.append(guest);
    output.append(ON1X_VM_FOOTER_MAGIC, ON1X_VM_FOOTER_MAGIC_SIZE);
    output.push_back(always ? ON1X_VM_ALWAYS : ON1X_VM_OPT_IN);
    const auto length = static_cast<std::uint64_t>(guest.size());
    for (unsigned i = 0; i < 8; ++i)
        output.push_back(static_cast<char>((length >> (i * 8)) & 0xffu));
    return output;
}

int cache_and_run(const Guest& guest) {
    return remember_and_run(guest, StagedImage(guest.image));
}

int run_elf(std::string_view path) {
    Guest guest{read_binary(fs::path(path), ON1X_VM_GUEST_MAX, "guest ELF"), {}};
    const StagedImage staged(guest.image);
    if (const auto status = staged.invoke(false))
        return status;
    guest.archive_sha256 = archvirt_cache::sha256(runtime_archive(Assets{}));
    return remember_and_run(guest, staged);
}

int rerun(std::string_view key) {
    if (!archvirt_cache::valid_key(key))
        throw std::runtime_error("ArchVirt cache: invalid key: expected 64 lowercase hexadecimal characters");
    const auto archive = runtime_archive(Assets{});
    const auto image = archvirt_cache::load({profile, archvirt_cache::sha256(archive)}, key);
    return StagedImage(image).invoke(true);
}

void clear_cache() { archvirt_cache::clear(); }

void print_build_inputs() {
    Assets a;
    std::vector<fs::path> files{
        a.source / "compile.py", a.source / "guest.ld", a.source / "include/string.h",
        a.source.parent_path() / "runtime/on1x_runtime.h", a.archive, a.launcher,
        fs::path(ON1X_ARCHVIRT_INCLUDE) / "archvirt_boot.h",
        ON1X_ARCHVIRT_PYTHON, ON1X_ARCHVIRT_CLANG, ON1X_ARCHVIRT_LLD,
    };
    std::set<std::string> paths;
    for (auto& file : files) {
        auto path = fs::absolute(file).lexically_normal();
        const auto name = path.string();
        if (name.find_first_of("\n\r") != name.npos || !fs::is_regular_file(path))
            throw std::runtime_error("invalid or missing ArchVirt build input: " + name);
        paths.insert(name);
    }
    for (const auto& path : paths) std::cout << path << '\n';
    if (!std::cout) throw std::runtime_error("cannot write ArchVirt build inputs");
}
}
