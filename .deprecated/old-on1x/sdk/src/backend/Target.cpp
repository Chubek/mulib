#include "On1xSDK-Target.h"
#include "TargetInternal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string_view>

namespace {

constexpr std::size_t target_required_size = offsetof(On1xSDK_TargetDescriptor, features);

bool power_of_two(std::uint32_t value) noexcept {
    return value && !(value & (value - 1u));
}

On1xSDK_Status validate_layout(const On1xSDK_DataLayout *layout) noexcept {
    if (!layout) return ON1X_SDK_INVALID_ARGUMENT;
    std::uint32_t abi{}, size{};
    std::memcpy(&abi, &layout->abi_version, sizeof abi);
    std::memcpy(&size, &layout->struct_size, sizeof size);
    if (abi != ON1X_SDK_TARGET_ABI_VERSION || size < sizeof(On1xSDK_DataLayout))
        return ON1X_SDK_ABI_MISMATCH;

    On1xSDK_DataLayout value{};
    std::memcpy(&value, layout, sizeof value);
    if (value.byte_order != ON1X_SDK_BYTE_ORDER_LITTLE &&
        value.byte_order != ON1X_SDK_BYTE_ORDER_BIG)
        return ON1X_SDK_INVALID_ARGUMENT;
    if ((value.pointer_bits != 16u && value.pointer_bits != 32u &&
         value.pointer_bits != 64u && value.pointer_bits != 128u) ||
        !power_of_two(value.pointer_alignment) || value.pointer_alignment < 8u ||
        value.pointer_alignment > 1024u ||
        !power_of_two(value.integer_alignment) || value.integer_alignment < 8u ||
        value.integer_alignment > 1024u ||
        !power_of_two(value.stack_alignment) || value.stack_alignment < 8u ||
        value.stack_alignment > 4096u)
        return ON1X_SDK_INVALID_ARGUMENT;
    return ON1X_SDK_OK;
}

bool valid_object(On1xSDK_ObjectFormat format) noexcept {
    return format >= ON1X_SDK_OBJECT_FORMAT_NONE &&
           format <= ON1X_SDK_OBJECT_FORMAT_WASM;
}

bool valid_calling(On1xSDK_CallingConvention convention) noexcept {
    return convention >= ON1X_SDK_CALLING_CONVENTION_C &&
           convention <= ON1X_SDK_CALLING_CONVENTION_WASM;
}

bool text_view(const char *data, std::size_t size, std::size_t limit,
               bool optional, std::string_view &out) noexcept {
    out = {};
    if (size > limit || (size && !data) || (!size && !optional)) return false;
    if (!size) return true;
    out = {data, size};
    return std::none_of(out.begin(), out.end(), [](char c) { return c == '\0'; });
}

bool token(std::string_view text, bool dot, bool plus) noexcept {
    if (text.empty()) return false;
    const auto first = static_cast<unsigned char>(text.front());
    if (!((first >= 'a' && first <= 'z') || (first >= '0' && first <= '9')))
        return false;
    return std::all_of(text.begin() + 1, text.end(), [&](char raw) {
        const auto c = static_cast<unsigned char>(raw);
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
               (dot && c == '.') || (plus && c == '+');
    });
}

std::string_view architecture(std::string_view triple) noexcept {
    return triple.substr(0, triple.find('-'));
}

std::string_view component(std::string_view triple, std::size_t wanted) noexcept {
    std::size_t begin = 0;
    for (std::size_t index = 0;; ++index) {
        const auto end = triple.find('-', begin);
        if (index == wanted)
            return triple.substr(begin, end == triple.npos ? triple.size() - begin : end - begin);
        if (end == triple.npos) return {};
        begin = end + 1;
    }
}

On1xSDK_Status validate_known_profile(std::string_view triple,
                                      const On1xSDK_DataLayout &layout,
                                      On1xSDK_ObjectFormat object,
                                      On1xSDK_CallingConvention calling) noexcept {
    const auto arch = architecture(triple);
    const auto os = component(triple, 2);
    const auto require = [&](std::uint32_t bits, On1xSDK_ByteOrder order,
                             On1xSDK_ObjectFormat expected_object,
                             On1xSDK_CallingConvention expected_calling) {
        return layout.pointer_bits == bits && layout.byte_order == order &&
               object == expected_object && calling == expected_calling;
    };
    if (arch == "x86_64" || arch == "amd64") {
        if (!require(64u, ON1X_SDK_BYTE_ORDER_LITTLE, ON1X_SDK_OBJECT_FORMAT_ELF,
                     ON1X_SDK_CALLING_CONVENTION_SYSV_AMD64))
            return ON1X_SDK_INVALID_ARGUMENT;
    } else if (arch == "aarch64") {
        if (!require(64u, ON1X_SDK_BYTE_ORDER_LITTLE, ON1X_SDK_OBJECT_FORMAT_ELF,
                     ON1X_SDK_CALLING_CONVENTION_AAPCS64))
            return ON1X_SDK_INVALID_ARGUMENT;
    } else if (arch == "riscv64" || arch == "riscv64gc") {
        if (!require(64u, ON1X_SDK_BYTE_ORDER_LITTLE, ON1X_SDK_OBJECT_FORMAT_ELF,
                     ON1X_SDK_CALLING_CONVENTION_RISCV_LP64))
            return ON1X_SDK_INVALID_ARGUMENT;
    } else if (arch == "wasm32") {
        if (!require(32u, ON1X_SDK_BYTE_ORDER_LITTLE, ON1X_SDK_OBJECT_FORMAT_WASM,
                     ON1X_SDK_CALLING_CONVENTION_WASM))
            return ON1X_SDK_INVALID_ARGUMENT;
    }
    if ((os == "linux" || os == "freebsd") && object != ON1X_SDK_OBJECT_FORMAT_ELF)
        return ON1X_SDK_INVALID_ARGUMENT;
    return ON1X_SDK_OK;
}

template <class Function>
On1xSDK_Status guarded(Function &&function) noexcept {
    try {
        return function();
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

} // namespace

namespace on1x::sdk::target {

bool valid_triple(std::string_view text) noexcept {
    if (text.empty() || text.size() > ON1X_SDK_MAX_TARGET_BYTES) return false;
    std::size_t components = 0;
    std::size_t begin = 0;
    for (;;) {
        const auto end = text.find('-', begin);
        const auto part = text.substr(begin, end == text.npos ? text.size() - begin : end - begin);
        if (!token(part, true, false)) return false;
        ++components;
        if (end == text.npos) break;
        begin = end + 1;
    }
    return components >= 3 && components <= 5;
}

bool valid_feature(std::string_view text) noexcept {
    return text.size() <= ON1X_SDK_MAX_TARGET_FEATURE_BYTES && token(text, true, true);
}

bool has_feature(const On1xSDK_Target &target, std::string_view feature) noexcept {
    const auto found = std::lower_bound(
        target.feature_strings.begin(), target.feature_strings.end(), feature,
        [](const std::string &left, std::string_view right) {
            return std::string_view(left) < right;
        });
    return found != target.feature_strings.end() && std::string_view(*found) == feature;
}

bool equal(const On1xSDK_Target &left, const On1xSDK_Target &right) noexcept {
    return left.triple == right.triple && left.cpu == right.cpu &&
           left.data_layout.byte_order == right.data_layout.byte_order &&
           left.data_layout.pointer_bits == right.data_layout.pointer_bits &&
           left.data_layout.pointer_alignment == right.data_layout.pointer_alignment &&
           left.data_layout.integer_alignment == right.data_layout.integer_alignment &&
           left.data_layout.stack_alignment == right.data_layout.stack_alignment &&
           left.object_format == right.object_format &&
           left.calling_convention == right.calling_convention &&
           left.feature_strings == right.feature_strings;
}

} // namespace on1x::sdk::target

extern "C" {

On1xSDK_Status on1x_sdk_data_layout_validate(const On1xSDK_DataLayout *layout) {
    return validate_layout(layout);
}

On1xSDK_Status on1x_sdk_target_create(const On1xSDK_TargetDescriptor *descriptor,
                                      On1xSDK_Target **out_target) {
    if (!out_target) return ON1X_SDK_INVALID_ARGUMENT;
    *out_target = nullptr;
    if (!descriptor) return ON1X_SDK_INVALID_ARGUMENT;

    std::uint32_t abi{}, size{};
    std::memcpy(&abi, &descriptor->abi_version, sizeof abi);
    std::memcpy(&size, &descriptor->struct_size, sizeof size);
    if (abi != ON1X_SDK_TARGET_ABI_VERSION || size < target_required_size)
        return ON1X_SDK_ABI_MISMATCH;

    On1xSDK_TargetDescriptor input{};
    std::memcpy(&input, descriptor, std::min<std::size_t>(size, sizeof input));
    std::string_view triple, cpu;
    if (!text_view(input.triple, input.triple_size, ON1X_SDK_MAX_TARGET_BYTES, false, triple) ||
        !on1x::sdk::target::valid_triple(triple) ||
        !text_view(input.cpu, input.cpu_size, ON1X_SDK_MAX_CPU_BYTES, true, cpu) ||
        (!cpu.empty() && !token(cpu, true, true)))
        return ON1X_SDK_INVALID_ARGUMENT;
    if (const auto status = validate_layout(&input.data_layout); status != ON1X_SDK_OK)
        return status;
    if (!valid_object(input.object_format) || !valid_calling(input.calling_convention))
        return ON1X_SDK_INVALID_ARGUMENT;
    if (const auto status =
            validate_known_profile(triple, input.data_layout, input.object_format,
                                   input.calling_convention);
        status != ON1X_SDK_OK)
        return status;

    const bool has_features =
        size >= offsetof(On1xSDK_TargetDescriptor, feature_count) + sizeof(input.feature_count);
    if (!has_features) {
        input.features = nullptr;
        input.feature_count = 0;
    }
    if (input.feature_count > ON1X_SDK_MAX_TARGET_FEATURES ||
        (input.feature_count && !input.features))
        return input.feature_count > ON1X_SDK_MAX_TARGET_FEATURES ? ON1X_SDK_LIMIT
                                                                 : ON1X_SDK_INVALID_ARGUMENT;

    return guarded([&] {
        auto target = std::make_unique<On1xSDK_Target>();
        target->triple.assign(triple);
        target->cpu.assign(cpu);
        target->data_layout = input.data_layout;
        target->data_layout.struct_size = sizeof(On1xSDK_DataLayout);
        target->object_format = input.object_format;
        target->calling_convention = input.calling_convention;
        target->feature_strings.reserve(input.feature_count);
        for (std::size_t index = 0; index < input.feature_count; ++index) {
            std::string_view feature;
            if (!text_view(input.features[index].data, input.features[index].size,
                           ON1X_SDK_MAX_TARGET_FEATURE_BYTES, false, feature) ||
                !on1x::sdk::target::valid_feature(feature))
                return ON1X_SDK_INVALID_ARGUMENT;
            target->feature_strings.emplace_back(feature);
        }
        std::sort(target->feature_strings.begin(), target->feature_strings.end());
        if (std::adjacent_find(target->feature_strings.begin(),
                               target->feature_strings.end()) !=
            target->feature_strings.end())
            return ON1X_SDK_INVALID_ARGUMENT;

        target->feature_views.reserve(target->feature_strings.size());
        for (const auto &feature : target->feature_strings)
            target->feature_views.push_back({feature.data(), feature.size()});
        target->descriptor = {
            ON1X_SDK_TARGET_ABI_VERSION,
            sizeof(On1xSDK_TargetDescriptor),
            target->triple.data(),
            target->triple.size(),
            target->cpu.empty() ? nullptr : target->cpu.data(),
            target->cpu.size(),
            target->data_layout,
            target->object_format,
            target->calling_convention,
            target->feature_views.empty() ? nullptr : target->feature_views.data(),
            target->feature_views.size()
        };
        *out_target = target.release();
        return ON1X_SDK_OK;
    });
}

void on1x_sdk_target_destroy(On1xSDK_Target *target) {
    delete target;
}

On1xSDK_Status on1x_sdk_target_view(const On1xSDK_Target *target,
                                    const On1xSDK_TargetDescriptor **out_descriptor) {
    if (!out_descriptor) return ON1X_SDK_INVALID_ARGUMENT;
    *out_descriptor = nullptr;
    if (!target) return ON1X_SDK_INVALID_ARGUMENT;
    *out_descriptor = &target->descriptor;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_target_has_feature(const On1xSDK_Target *target,
                                           const char *feature, size_t feature_size,
                                           uint32_t *out_present) {
    if (!out_present) return ON1X_SDK_INVALID_ARGUMENT;
    *out_present = 0;
    std::string_view view;
    if (!target ||
        !text_view(feature, feature_size, ON1X_SDK_MAX_TARGET_FEATURE_BYTES, false, view) ||
        !on1x::sdk::target::valid_feature(view))
        return ON1X_SDK_INVALID_ARGUMENT;
    *out_present = on1x::sdk::target::has_feature(*target, view) ? 1u : 0u;
    return ON1X_SDK_OK;
}

On1xSDK_Status on1x_sdk_target_equal(const On1xSDK_Target *left,
                                     const On1xSDK_Target *right,
                                     uint32_t *out_equal) {
    if (!out_equal) return ON1X_SDK_INVALID_ARGUMENT;
    *out_equal = 0;
    if (!left || !right) return ON1X_SDK_INVALID_ARGUMENT;
    *out_equal = on1x::sdk::target::equal(*left, *right) ? 1u : 0u;
    return ON1X_SDK_OK;
}

} // extern "C"
