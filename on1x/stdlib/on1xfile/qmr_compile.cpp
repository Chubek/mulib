// Use Moonstone's compiler only. Execution belongs to MuLIB Mune.
#include <qmr/Qmr.hpp>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected the template's diagnostic name");
        moonstone::qmr::Limits limits;
        limits.max_template_bytes = 64 * 1024;
        limits.max_compiled_bytes = 512 * 1024;
        limits.max_nodes = 4096;
        std::string input;
        std::array<char, 4096> buffer;
        while (std::cin) {
            std::cin.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const auto count = static_cast<std::size_t>(std::cin.gcount());
            if (count > limits.max_template_bytes - input.size())
                throw std::runtime_error("template exceeds 64 KiB");
            input.append(buffer.data(), count);
        }
        if (!std::cin.eof()) throw std::runtime_error("cannot read template");
        std::cout << moonstone::qmr::compile(input, argv[1], limits).lua_source;
        return std::cout ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "on1x-qmr-compile: " << error.what() << '\n';
        return 1;
    }
}
