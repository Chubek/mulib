#ifndef ON1X_SDK_ARTIFACT_INTERNAL_HPP
#define ON1X_SDK_ARTIFACT_INTERNAL_HPP

#include "On1xSDK-Artifact.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct On1xSDK_Artifact {
    std::vector<uint8_t> bytes;
    std::string provider_name;
    std::string target_triple;
    On1xSDK_ArtifactInfo info{};
};

namespace on1x::sdk::backend {

On1xSDK_Status make_artifact(std::vector<uint8_t> bytes,
                             On1xSDK_ArtifactKind kind,
                             std::uint32_t input_schema,
                             std::uint32_t flags,
                             std::string_view provider_name,
                             std::string_view target_triple,
                             On1xSDK_Artifact **out) noexcept;

} // namespace on1x::sdk::backend

#endif
