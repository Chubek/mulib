#ifndef ON1X_SDK_METADATA_INTERNAL_HPP
#define ON1X_SDK_METADATA_INTERNAL_HPP

#include "RegistryInternal.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <string>
#include <utility>

namespace on1x::sdk::metadata {

inline On1xSDK_Status bounded(const char* text, size_t max, std::string_view& out) noexcept {
    out = {};
    if (!text) return ON1X_SDK_INVALID_ARGUMENT;
    for (size_t size = 0; size <= max; ++size) {
        if (text[size] == '\0') {
            out = {text, size};
            return ON1X_SDK_OK;
        }
    }
    return ON1X_SDK_LIMIT;
}

inline bool printable(std::string_view text) noexcept {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return c >= 0x20 && c <= 0x7e;
    });
}

inline bool lower(char c) noexcept { return c >= 'a' && c <= 'z'; }
inline bool digit(char c) noexcept { return c >= '0' && c <= '9'; }
inline bool letter(char c) noexcept { return lower(c) || (c >= 'A' && c <= 'Z'); }
inline bool alnum(char c) noexcept { return letter(c) || digit(c); }

inline bool valid_target(std::string_view target) noexcept {
    bool start = true;
    bool separated = false;
    for (char c : target) {
        if (start) {
            if (!alnum(c)) return false;
            start = false;
        } else if (c == '-') {
            separated = true;
            start = true;
        } else if (!alnum(c) && c != '_' && c != '.') {
            return false;
        }
    }
    return separated && !start;
}

template <class Descriptor>
On1xSDK_Status check(const On1xSDK_Registry* registry, const Descriptor* desc) noexcept {
    if (!registry || !desc) return ON1X_SDK_INVALID_ARGUMENT;
    const auto* bytes = reinterpret_cast<const unsigned char*>(desc);
    uint32_t version{}, size{};
    std::memcpy(&version, bytes + offsetof(Descriptor, abi_version), sizeof version);
    std::memcpy(&size, bytes + offsetof(Descriptor, struct_size), sizeof size);
    if (version != ON1X_SDK_ABI_VERSION || size < sizeof(Descriptor))
        return ON1X_SDK_ABI_MISMATCH;
    return ON1X_SDK_OK;
}

// Capture the first bad field without allocating or reading any later fields' strings.
struct Validation {
    On1xSDK_Status status = ON1X_SDK_OK;

    std::string_view field(const char* input, size_t max,
                           bool (*valid)(std::string_view) noexcept, bool optional = false) noexcept {
        if (status != ON1X_SDK_OK || (optional && !input)) return {};
        std::string_view out;
        status = bounded(input, max, out);
        if (status == ON1X_SDK_OK && !valid(out)) status = ON1X_SDK_INVALID_ARGUMENT;
        return out;
    }

    std::string_view name(const char* input, bool optional = false) noexcept {
        return field(input, ON1X_SDK_NAME_MAX, valid_name, optional);
    }

    std::string_view text(const char* input, size_t max, bool optional = false) noexcept {
        return field(input, max, printable, optional);
    }
};

template <class Descriptor>
using Field = std::pair<const char* Descriptor::*, std::string_view>;

// Stable backing storage: descriptors are allocated once and published by aliasing shared_ptr.
template <class Descriptor, size_t Fields>
struct Storage {
    Descriptor descriptor;
    std::array<std::string, Fields> strings;

    Storage(const Descriptor& input, const std::array<Field<Descriptor>, Fields>& fields)
        : descriptor(input) {
        descriptor.struct_size = sizeof(Descriptor); // Unknown trailing bytes were not copied.
        for (size_t i = 0; i < Fields; ++i) {
            const auto& [member, text] = fields[i];
            if (!text.empty()) {
                strings[i].assign(text);
                descriptor.*member = strings[i].c_str();
            } else {
                descriptor.*member = nullptr;
            }
        }
    }
};

template <class Owner>
On1xSDK_Status publish(On1xSDK_Registry* registry, Kind kind, const std::shared_ptr<Owner>& owner) {
    const auto* desc = &owner->descriptor;
    return add(registry, kind, desc->name, std::shared_ptr<const void>(owner, desc));
}

template <class Fn>
On1xSDK_Status guard(Fn&& fn) noexcept {
    try {
        return fn();
    } catch (const std::bad_alloc&) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

template <class Descriptor, size_t Fields>
On1xSDK_Status register_fields(On1xSDK_Registry* registry, Kind kind, const Descriptor& input,
                               const std::array<Field<Descriptor>, Fields>& fields) noexcept {
    return guard([&] {
        return publish(registry, kind, std::make_shared<Storage<Descriptor, Fields>>(input, fields));
    });
}

template <class Descriptor>
const Descriptor* lookup(const On1xSDK_Registry* registry, Kind kind, const char* name) noexcept {
    std::string_view key;
    if (bounded(name, ON1X_SDK_NAME_MAX, key) != ON1X_SDK_OK || !valid_name(key)) return nullptr;
    return static_cast<const Descriptor*>(find(registry, kind, key));
}

} // namespace on1x::sdk::metadata

#endif
