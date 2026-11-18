#include <ld.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

namespace fs = std::filesystem;

using Registrar = std::pair<std::string_view, std::string_view>;
constexpr std::array registrars{
    Registrar{"c11", "on1x_builtin_c11_register"},
    Registrar{"mir-c11", "on1x_builtin_mir_c11_register"},
    Registrar{"mir-eval", "on1x_builtin_mir_eval_register"},
    Registrar{"mir-text", "on1x_builtin_mir_text_register"},
    Registrar{"mir-cfg", "on1x_builtin_mir_cfg_register"},
};

std::string render(std::string_view manifest) {
    std::array<bool, registrars.size()> seen{};
    std::size_t count = 0;
    std::string result = "// Generated from the On1x Yellowstone Lua backend manifest.\n";
    while (!manifest.empty()) {
        const auto nl = manifest.find('\n');
        const auto tab = manifest.find('\t');
        if (nl == manifest.npos || tab >= nl)
            throw std::runtime_error("malformed Yellowstone backend manifest");
        const auto name = manifest.substr(0, tab);
        const auto symbol = manifest.substr(tab + 1, nl - tab - 1);
        manifest.remove_prefix(nl + 1);

        std::size_t i = 0;
        for (; i < registrars.size(); ++i)
            if (registrars[i] == Registrar{name, symbol}) break;
        if (i == registrars.size())
            throw std::runtime_error("unknown built-in backend/registrar pair: " + std::string(name));
        if (seen[i]) throw std::runtime_error("duplicate built-in backend: " + std::string(name));
        seen[i] = true;
        ++count;

        result += "ON1X_BUILTIN_REGISTRAR(\"";
        result += name;
        result += "\", ";
        result += symbol;
        result += ")\n";
    }
    if (count != registrars.size())
        throw std::runtime_error("missing required built-in backend registration");
    return result;
}

void publish(const fs::path &output, const std::string &contents) {
    auto temporary = output;
    temporary += ".tmp";
    try {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << contents;
        out.close();
        if (!out) throw std::runtime_error("cannot write registrar map");
        fs::rename(temporary, output);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw;
    }
}

} // namespace

int main(int argc, char **argv) try {
    if (argc != 3) throw std::runtime_error("usage: on1x-backend-manifest script.lua output.inc");
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) throw std::runtime_error("cannot open Lua backend spec");
    std::array<char, 16 * 1024 + 1> source{};
    in.read(source.data(), source.size());
    if (in.bad()) throw std::runtime_error("cannot read Lua backend spec");
    if (!in.eof()) throw std::runtime_error("Lua backend spec exceeds 16 KiB");

    uint8_t *bytes = nullptr;
    std::size_t size = 0;
    char *error = nullptr;
    const auto status = qld_backend_manifest(source.data(), static_cast<std::size_t>(in.gcount()),
                                             &bytes, &size, &error);
    const std::unique_ptr<char, decltype(&qld_free_string)> diagnostic(error, qld_free_string);
    const auto free_bytes = [size](uint8_t *data) { qld_free_bytes(data, size); };
    const std::unique_ptr<uint8_t, decltype(free_bytes)> manifest(bytes, free_bytes);
    if (status != 0) throw std::runtime_error(error ? error : "Yellowstone backend manifest failed");
    if (!bytes && size) throw std::runtime_error("invalid Yellowstone backend manifest buffer");
    const auto text = size ? std::string_view(reinterpret_cast<const char *>(bytes), size)
                           : std::string_view{};
    publish(argv[2], render(text));
    return 0;
} catch (const std::exception &error) {
    std::cerr << "on1x-backend-manifest: " << error.what() << '\n';
    return 1;
}
