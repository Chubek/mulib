#ifndef ON1X_SDK_TARGET_INTERNAL_HPP
#define ON1X_SDK_TARGET_INTERNAL_HPP

#include "On1xSDK-Target.h"

#include <string>
#include <string_view>
#include <vector>

struct On1xSDK_Target {
    std::string triple;
    std::string cpu;
    On1xSDK_DataLayout data_layout{};
    On1xSDK_ObjectFormat object_format{};
    On1xSDK_CallingConvention calling_convention{};
    std::vector<std::string> feature_strings;
    std::vector<On1xSDK_TargetFeature> feature_views;
    On1xSDK_TargetDescriptor descriptor{};
};

namespace on1x::sdk::target {

bool valid_triple(std::string_view text) noexcept;
bool valid_feature(std::string_view text) noexcept;
bool has_feature(const On1xSDK_Target &target, std::string_view feature) noexcept;
bool equal(const On1xSDK_Target &left, const On1xSDK_Target &right) noexcept;

} // namespace on1x::sdk::target

#endif
