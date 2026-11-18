#pragma once

#include <On1xSDK.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace on1x::sdk_plugin {
inline constexpr unsigned version = 1;
inline constexpr unsigned max_image = 16 * 1024 * 1024;
inline constexpr unsigned max_frame = ON1X_SDK_MAX_ARTIFACT_BYTES + 1024;
inline constexpr unsigned timeout_ms = 5000;
inline constexpr std::string_view ready = "on1x-plugin/1";

inline void put(std::string &buffer, std::uint32_t value)
{
    for (int shift : {24, 16, 8, 0})
        buffer.push_back(static_cast<char>(value >> shift));
}

inline void put(std::string &buffer, std::string_view value)
{
    put(buffer, static_cast<std::uint32_t>(value.size()));
    buffer.append(value);
}

struct Reader {
    std::string_view rest;

    std::uint32_t take() {
        if (rest.size() < 4) throw std::runtime_error("truncated plugin frame");
        std::uint32_t value = 0;
        for (unsigned char ch : rest.substr(0, 4))
            value = (value << 8) | ch;
        rest.remove_prefix(4);
        return value;
    }

    std::string_view take(std::size_t limit) {
        const auto size = take();
        if (size > limit || size > rest.size())
            throw std::runtime_error("invalid plugin frame field length");
        const auto value = rest.substr(0, size);
        rest.remove_prefix(size);
        return value;
    }

    void finish() const {
        if (!rest.empty()) throw std::runtime_error("trailing bytes in plugin frame");
    }
};
}
