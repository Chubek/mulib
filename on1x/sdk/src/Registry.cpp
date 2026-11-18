#include "On1xSDK-Registry.h"
#include "RegistryInternal.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <string>
#include <utility>
#include <vector>

struct On1xSDK_Host {}; // ABI v1 has no host operations.

namespace {

constexpr size_t max_extensions = 1024;
constexpr size_t max_entries_per_kind = 1024;
constexpr size_t num_kinds = static_cast<size_t>(on1x::sdk::Kind::ExecEnv) + 1;

struct Entry {
    std::string name;
    std::shared_ptr<const void> descriptor;
};

struct ExtensionEntry {
    std::string name;
    std::string version;
    On1xSDK_RegisterFn register_all;
};

// Do not use strlen on unbounded caller data or read past an early NUL.
On1xSDK_Status bounded_string(const char* text, std::string_view& out) noexcept {
    if (!text) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    for (size_t n = 0; n <= ON1X_SDK_NAME_MAX; ++n) {
        if (text[n] == '\0') {
            out = {text, n};
            return ON1X_SDK_OK;
        }
    }
    return ON1X_SDK_LIMIT;
}

bool valid_version(std::string_view version) noexcept {
    return !version.empty() && std::all_of(version.begin(), version.end(), [](unsigned char c) {
        return c >= 0x20 && c <= 0x7e;
    });
}

bool letter(unsigned char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

} // namespace

struct On1xSDK_Registry {
    std::array<std::vector<Entry>, num_kinds> entries;
    std::vector<ExtensionEntry> extensions;
    bool registering = false;
};

namespace {

// Append-only during register_all, so a failed registration only needs to
// remove its suffixes. Commit and rollback neither allocate nor move any
// descriptor. Allocate/copy the extension record *before* entering the callback.
class Registration {
public:
    Registration(On1xSDK_Registry& registry, ExtensionEntry extension) : registry_(registry) {
        registry_.extensions.push_back(std::move(extension));
        for (size_t i = 0; i < num_kinds; ++i) {
            old_sizes_[i] = registry_.entries[i].size();
        }
        registry_.registering = true;
    }

    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;

    ~Registration() {
        if (!committed_) {
            for (size_t i = 0; i < num_kinds; ++i) {
                auto& entries = registry_.entries[i];
                while (entries.size() > old_sizes_[i]) {
                    entries.pop_back();
                }
            }
            registry_.extensions.pop_back();
        }
        registry_.registering = false;
    }

    void commit() noexcept { committed_ = true; }

private:
    On1xSDK_Registry& registry_;
    std::array<size_t, num_kinds> old_sizes_{};
    bool committed_ = false;
};

On1xSDK_Status invoke(On1xSDK_RegisterFn callback, On1xSDK_Registry* registry) noexcept {
    const On1xSDK_Host host{};
    try {
        switch (auto status = callback(&host, registry); status) {
        case ON1X_SDK_OK:
        case ON1X_SDK_ERROR:
        case ON1X_SDK_UNSUPPORTED:
        case ON1X_SDK_LIMIT:
        case ON1X_SDK_INVALID_ARGUMENT:
        case ON1X_SDK_ABI_MISMATCH:
        case ON1X_SDK_DUPLICATE:
        case ON1X_SDK_BUSY:
            return status;
        }
    } catch (const std::bad_alloc&) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        // A linked C++ callback must not unwind through the C ABI.
    }
    return ON1X_SDK_ERROR;
}

} // namespace

namespace on1x::sdk {

bool valid_name(std::string_view name) noexcept {
    if (name.empty() || name.size() > ON1X_SDK_NAME_MAX) {
        return false;
    }
    if (!letter(static_cast<unsigned char>(name.front())) && name.front() != '_') {
        return false;
    }
    return std::all_of(name.begin() + 1, name.end(), [](unsigned char c) {
        return letter(c) || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    });
}

On1xSDK_Status add(On1xSDK_Registry* registry, Kind kind, std::string_view name,
                   std::shared_ptr<const void> descriptor) {
    const auto i = static_cast<size_t>(kind);
    if (!registry || i >= num_kinds || !valid_name(name) || !descriptor || !descriptor.use_count()) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (find(registry, kind, name)) {
        return ON1X_SDK_DUPLICATE;
    }
    auto& entries = registry->entries[i];
    if (entries.size() >= max_entries_per_kind) {
        return ON1X_SDK_LIMIT;
    }
    entries.push_back({std::string{name}, std::move(descriptor)});
    return ON1X_SDK_OK;
}

const void* find(const On1xSDK_Registry* registry, Kind kind, std::string_view name) noexcept {
    const auto i = static_cast<size_t>(kind);
    if (!registry || i >= num_kinds || !valid_name(name)) {
        return nullptr;
    }
    const auto& entries = registry->entries[i];
    const auto iter = std::find_if(entries.begin(), entries.end(), [name](const Entry& entry) {
        return entry.name == name;
    });
    return iter == entries.end() ? nullptr : iter->descriptor.get();
}

const void* at(const On1xSDK_Registry* registry, Kind kind, size_t index) noexcept {
    const auto i = static_cast<size_t>(kind);
    if (!registry || i >= num_kinds || index >= registry->entries[i].size()) {
        return nullptr;
    }
    return registry->entries[i][index].descriptor.get();
}

size_t count(const On1xSDK_Registry* registry, Kind kind) noexcept {
    const auto i = static_cast<size_t>(kind);
    return registry && i < num_kinds ? registry->entries[i].size() : 0;
}

} // namespace on1x::sdk

extern "C" On1xSDK_Status on1x_sdk_registry_create(On1xSDK_Registry** out) {
    if (!out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    *out = nullptr;
    try {
        *out = new On1xSDK_Registry;
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc&) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_registry_destroy(On1xSDK_Registry* registry) {
    if (registry && !registry->registering) {
        delete registry;
    }
}

extern "C" On1xSDK_Status on1x_sdk_registry_register(On1xSDK_Registry* registry,
                                                    const On1xSDK_Extension* extension) {
    if (!registry || !extension) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (registry->registering) {
        return ON1X_SDK_BUSY;
    }

    // A short descriptor may contain only its ABI header, not a full C++
    // object. Read those bytes before touching the rest of the descriptor.
    const auto* bytes = reinterpret_cast<const unsigned char*>(extension);
    uint32_t abi_version{}, struct_size{};
    std::memcpy(&abi_version, bytes + offsetof(On1xSDK_Extension, abi_version), sizeof abi_version);
    std::memcpy(&struct_size, bytes + offsetof(On1xSDK_Extension, struct_size), sizeof struct_size);
    if (abi_version != ON1X_SDK_ABI_VERSION || struct_size < sizeof(On1xSDK_Extension)) {
        return ON1X_SDK_ABI_MISMATCH;
    }
    On1xSDK_Extension descriptor{};
    std::memcpy(&descriptor, bytes, sizeof descriptor);
    if (!descriptor.register_all) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    std::string_view name, version;
    if (const auto status = bounded_string(descriptor.name, name); status != ON1X_SDK_OK) {
        return status;
    }
    if (!on1x::sdk::valid_name(name)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (const auto status = bounded_string(descriptor.version, version); status != ON1X_SDK_OK) {
        return status;
    }
    if (!valid_version(version)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }

    try {
        const auto& extensions = registry->extensions;
        if (std::any_of(extensions.begin(), extensions.end(), [name](const ExtensionEntry& entry) {
                return entry.name == name;
            })) {
            return ON1X_SDK_DUPLICATE;
        }
        if (extensions.size() >= max_extensions) {
            return ON1X_SDK_LIMIT;
        }

        Registration registration(*registry,
                                  {std::string{name}, std::string{version}, descriptor.register_all});
        const auto status = invoke(registry->extensions.back().register_all, registry);
        if (status == ON1X_SDK_OK) {
            registration.commit();
        }
        return status;
    } catch (const std::bad_alloc&) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}
