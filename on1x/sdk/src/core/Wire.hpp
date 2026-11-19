#ifndef ON1X_SDK_WIRE_INTERNAL_HPP
#define ON1X_SDK_WIRE_INTERNAL_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace on1x::sdk::wire {

inline void put16(std::uint8_t *&out, std::uint16_t value) noexcept {
    *out++ = static_cast<std::uint8_t>(value >> 8);
    *out++ = static_cast<std::uint8_t>(value);
}

inline void put32(std::uint8_t *&out, std::uint32_t value) noexcept {
    for (unsigned i = 0; i < 4; ++i) {
        *out++ = static_cast<std::uint8_t>(value >> (24 - i * 8));
    }
}

inline void put64(std::uint8_t *&out, std::uint64_t value) noexcept {
    for (unsigned i = 0; i < 8; ++i) {
        *out++ = static_cast<std::uint8_t>(value >> (56 - i * 8));
    }
}

inline std::uint16_t get16(const std::uint8_t *&in) noexcept {
    const auto value = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(in[0]) << 8) | in[1]
    );
    in += 2;
    return value;
}

inline std::uint32_t get32(const std::uint8_t *&in) noexcept {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        value = (value << 8) | *in++;
    }
    return value;
}

inline std::uint64_t get64(const std::uint8_t *&in) noexcept {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) {
        value = (value << 8) | *in++;
    }
    return value;
}

inline void put_bytes(
    std::uint8_t *&out, const void *data, std::size_t size
) noexcept {
    if (size) {
        std::memcpy(out, data, size);
        out += size;
    }
}

} // namespace on1x::sdk::wire

#endif
