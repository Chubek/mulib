#ifndef ON1X_SDK_TOOLCHAIN_INTERNAL_HPP
#define ON1X_SDK_TOOLCHAIN_INTERNAL_HPP

#include "On1xSDK-Toolchain.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace on1x::sdk::toolchain {

enum class Flavor { Legacy, Provider };

struct StoredToolchain {
    Flavor flavor{Flavor::Legacy};
    std::string name;
    std::string target_text;
    std::string driver_id;
    std::string version;
    On1xSDK_Toolchain legacy{};

    std::shared_ptr<On1xSDK_Target> target;
    std::int32_t priority{};
    std::uint32_t roles{};
    std::array<std::uint8_t, ON1X_SDK_TOOL_DIGEST_BYTES> digest{};

    void refresh_legacy() noexcept;
};

bool nonzero_digest(const std::uint8_t *digest) noexcept;

} // namespace on1x::sdk::toolchain

#endif
