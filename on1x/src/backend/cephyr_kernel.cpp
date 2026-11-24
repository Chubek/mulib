// KernelBind supplies its verified, single implementation TU on stdin. Keep
// binary stdout separate from diagnostics and never load the resulting DSO.
#include <cephyr/Cephyr.hpp>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string_view>

int main(int argc, char** argv) try {
    const bool object = (argc == 4 || argc == 5) && std::string_view(argv[1]) == "--object";
    const bool info = argc == 2 && std::string_view(argv[1]) == "--toolchain-inputs";
    if ((!object && argc != 2) || cephyr_abi_version() != 1) {
        std::cerr << "expected INCLUDE_DIR, --object INCLUDE_DIR RUNTIME_DIR [TARGET] or --toolchain-inputs, "
                     "and Cephyr embedding ABI 1\n";
        return 1;
    }
    const std::string_view target = argc == 5 ? argv[4] : "x86_64-linux-gnu";
    unsigned machine = 62;
    if (target == "aarch64-linux-gnu") machine = 183;
    else if (target == "riscv64-linux-gnu") machine = 243;
    else if (target != "x86_64-linux-gnu") {
        std::cerr << "unsupported Cephyr kernel object target\n";
        return 1;
    }
    namespace fs = std::filesystem;
    auto share = fs::canonical("/proc/self/exe").parent_path() / "../share/on1x";
    auto config = (fs::is_directory(share) ? share : fs::path(ON1X_CEPHYR_BUILD_CONFIG_DIR))
                  / "cephyr-baseline.toml";
    if (info) {
        for (auto path : {config, fs::path(ON1X_CEPHYR_RESOURCE_ROOT),
                          fs::path(ON1X_CEPHYR_ARCHON_ROOT)}) {
            auto name = fs::canonical(path).string();
            if (name.find_first_of("\r\n") != std::string::npos) return 1;
            std::cout << name << '\n';
        }
        return !std::cout;
    }
    std::string source;
    const auto end = std::istreambuf_iterator<char>{};
    for (auto it = std::istreambuf_iterator<char>(std::cin); it != end; ++it) {
        if (source.size() == 8 * 1024 * 1024) {
            std::cerr << "Cephyr kernel source exceeds the 8 MiB input limit\n";
            return 1;
        }
        source += *it;
    }
    auto arguments = std::vector<std::string>{"--config", config.string(), "--target=" + std::string(target),
        "--stdlib=cephyr", "--no-factir", "--no-sandslab"};
    if (object) arguments.insert(arguments.end(), {"-c", "-DON1X_KERNEL_ABI_V1=1",
        "-D_POSIX_C_SOURCE=200809L", "-I", argv[3]});
    else arguments.insert(arguments.end(), {"-fPIC", "-shared", "-nostdlib"});
    arguments.insert(arguments.end(), {"-I", argv[object ? 2 : 1], "-x", "c", "-", "-o", "-"});
    auto result = cephyr::compile(arguments, source, {ON1X_CEPHYR_RESOURCE_ROOT, ON1X_CEPHYR_ARCHON_ROOT});
    std::cerr << result.diagnostics();
    if (!result) return 1;
    auto output = result.output();
    if (output.size() < 64 || output.size() > 64 * 1024 * 1024 ||
        output.substr(0, 7) != "\177ELF\2\1\1" || output[16] != (object ? 1 : 3) ||
        output[17] != 0 || static_cast<unsigned char>(output[18]) != machine || output[19] != 0 ||
        (machine == 243 && output.substr(48, 6) != std::string_view("\0\0\0\0\x40\0", 6))) {
        std::cerr << "Cephyr did not return a bounded " << target << " ELF "
                  << (object ? "object" : "shared library") << '\n';
        return 1;
    }
    std::cout.write(output.data(), output.size()).flush();
    return !std::cout;
} catch (const std::exception& error) {
    std::cerr << "Cephyr kernel compilation: " << error.what() << '\n';
    return 1;
}
