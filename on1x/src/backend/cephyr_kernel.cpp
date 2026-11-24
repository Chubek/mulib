// KernelBind supplies its verified, single implementation TU on stdin. Keep
// binary stdout separate from diagnostics and never load the resulting DSO.
#include <cephyr/Cephyr.hpp>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string_view>

int main(int argc, char** argv) try {
    if (argc != 2 || !std::filesystem::is_directory(argv[1]) || cephyr_abi_version() != 1) {
        std::cerr << "expected kernel include directory and Cephyr embedding ABI 1\n";
        return 1;
    }
    namespace fs = std::filesystem;
    auto share = fs::canonical("/proc/self/exe").parent_path() / "../share/on1x";
    auto config = (fs::is_directory(share) ? share : fs::path(ON1X_CEPHYR_BUILD_CONFIG_DIR))
                  / "cephyr-baseline.toml";
    std::string source;
    const auto end = std::istreambuf_iterator<char>{};
    for (auto it = std::istreambuf_iterator<char>(std::cin); it != end; ++it) {
        if (source.size() == 8 * 1024 * 1024) {
            std::cerr << "Cephyr kernel source exceeds the 8 MiB input limit\n";
            return 1;
        }
        source += *it;
    }
    auto result = cephyr::compile({"--config", config.string(), "--target=x86_64-linux-gnu",
        "--stdlib=cephyr", "--no-factir", "--no-sandslab", "-fPIC", "-shared", "-nostdlib",
        "-I", argv[1], "-x", "c", "-", "-o", "-"}, source,
        {ON1X_CEPHYR_RESOURCE_ROOT, ON1X_CEPHYR_ARCHON_ROOT});
    std::cerr << result.diagnostics();
    if (!result) return 1;
    auto output = result.output();
    if (output.size() < 64 || output.size() > 64 * 1024 * 1024 ||
        output.substr(0, 7) != "\177ELF\2\1\1" ||
        output.substr(16, 4) != std::string_view("\3\0\76\0", 4)) {
        std::cerr << "Cephyr did not return a bounded AMD64 ELF shared library\n";
        return 1;
    }
    std::cout.write(output.data(), output.size()).flush();
    return !std::cout;
} catch (const std::exception& error) {
    std::cerr << "Cephyr kernel compilation: " << error.what() << '\n';
    return 1;
}
