#ifndef ON1X_SDK_FACET_INTERNAL_HPP
#define ON1X_SDK_FACET_INTERNAL_HPP

#include "On1xSDK-Transformer.h"
#include "ProgramInternal.hpp"
#include "RegistryInternal.hpp"

#include <cstring>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

struct On1xSDK_Program {
    const On1xSDK_Phase phase;
    const std::vector<uint8_t> bytes;
    const on1x::Unit *const checked_unit = nullptr;
    const on1x::mir::Module *const verified_module = nullptr;
    const std::string_view source_name{};
};

struct On1xSDK_OutputSink {
    std::vector<uint8_t> bytes;
    const size_t limit;
    On1xSDK_Status status = ON1X_SDK_OK;
};

namespace on1x::sdk {

// At most max + 1 bytes are examined. Pointer validity is the C caller's
// responsibility, as with the other inputs to the SDK.
inline bool bounded_text(const char *text, size_t max, std::string_view &view) noexcept {
    if (!text) return false;
    for (size_t i = 0; i <= max; ++i) {
        if (text[i] == '\0') {
            view = {text, i};
            return true;
        }
    }
    return false;
}

inline On1xSDK_Status read_name(const char *text, std::string_view &name) noexcept {
    if (!text) return ON1X_SDK_INVALID_ARGUMENT;
    if (!bounded_text(text, ON1X_SDK_NAME_MAX, name)) return ON1X_SDK_LIMIT;
    return valid_name(name) ? ON1X_SDK_OK : ON1X_SDK_INVALID_ARGUMENT;
}

// C code may pass an out-of-range enum. Inspect its representation without
// evaluating it as a C++ enum until its validity has been established.
template <class Enum>
auto enum_value(const Enum &value) noexcept {
    std::underlying_type_t<Enum> raw;
    static_assert(sizeof raw == sizeof value);
    std::memcpy(&raw, &value, sizeof raw);
    return raw;
}

inline On1xSDK_Status callback_status(const On1xSDK_Status &status) noexcept {
    switch (enum_value(status)) {
    case ON1X_SDK_OK: return ON1X_SDK_OK;
    case ON1X_SDK_ERROR: return ON1X_SDK_ERROR;
    case ON1X_SDK_UNSUPPORTED: return ON1X_SDK_UNSUPPORTED;
    case ON1X_SDK_LIMIT: return ON1X_SDK_LIMIT;
    case ON1X_SDK_INVALID_ARGUMENT: return ON1X_SDK_INVALID_ARGUMENT;
    case ON1X_SDK_ABI_MISMATCH: return ON1X_SDK_ABI_MISMATCH;
    case ON1X_SDK_DUPLICATE: return ON1X_SDK_DUPLICATE;
    case ON1X_SDK_BUSY: return ON1X_SDK_BUSY;
    default: return ON1X_SDK_ERROR;
    }
}

template <class Fn>
On1xSDK_Status guard(Fn &&fn) noexcept {
    try {
        return fn();
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

template <class Descriptor>
struct StoredFacet {
    const std::string name;
    const Descriptor descriptor;

    StoredFacet(std::string_view key, const Descriptor &input)
        : name(key), descriptor{ON1X_SDK_ABI_VERSION, sizeof(Descriptor), name.c_str(),
                                input.phase, input.run, input.userdata} {}
};

template <class Descriptor>
On1xSDK_Status register_facet(On1xSDK_Registry *registry, Kind kind,
                              const Descriptor *descriptor, bool allow_source,
                              bool allow_verified_mir, bool allow_checked_hir = false) noexcept {
    if (!registry || !descriptor) return ON1X_SDK_INVALID_ARGUMENT;
    // A short descriptor is permitted to designate just the ABI header.
    const auto *bytes = reinterpret_cast<const unsigned char *>(descriptor);
    uint32_t version{}, size{};
    std::memcpy(&version, bytes + offsetof(Descriptor, abi_version), sizeof version);
    std::memcpy(&size, bytes + offsetof(Descriptor, struct_size), sizeof size);
    if (version != ON1X_SDK_ABI_VERSION || size < sizeof(Descriptor))
        return ON1X_SDK_ABI_MISMATCH;
    static_assert(std::is_trivially_copyable_v<Descriptor>);
    Descriptor snapshot{};
    std::memcpy(&snapshot, bytes, sizeof snapshot);

    if (!snapshot.run) return ON1X_SDK_INVALID_ARGUMENT;
    std::string_view name;
    if (const auto status = read_name(snapshot.name, name); status != ON1X_SDK_OK) return status;
    const auto phase = enum_value(snapshot.phase);
    if (!((phase == ON1X_SDK_SOURCE && allow_source) ||
          (phase == ON1X_SDK_CHECKED_HIR && allow_checked_hir) ||
          (phase == ON1X_SDK_VERIFIED_MIR && allow_verified_mir)))
        return ON1X_SDK_UNSUPPORTED;
    return guard([&] {
        return add(registry, kind, name, std::make_shared<const StoredFacet<Descriptor>>(name, snapshot));
    });
}

template <class Descriptor>
const Descriptor *find_facet(const On1xSDK_Registry *registry, Kind kind,
                             std::string_view name) noexcept {
    const auto *stored = static_cast<const StoredFacet<Descriptor> *>(find(registry, kind, name));
    return stored ? &stored->descriptor : nullptr;
}

template <class Container>
On1xSDK_Status view_bytes(const Container *object, const uint8_t **bytes, size_t *size) noexcept {
    if (!object || !bytes || !size) return ON1X_SDK_INVALID_ARGUMENT;
    *bytes = object->bytes.empty() ? nullptr : object->bytes.data();
    *size = object->bytes.size();
    return ON1X_SDK_OK;
}

template <class Callback, class Sink>
On1xSDK_Status invoke(const On1xSDK_Program *program, Callback callback, void *userdata,
                      Sink &sink) noexcept {
    const auto status = guard([&] { return callback_status(callback(userdata, program, &sink)); });
    return sink.status == ON1X_SDK_OK ? status : sink.status;
}

template <class Descriptor, class Publish>
On1xSDK_Status run_bytes(const On1xSDK_Registry *registry, Kind kind, const char *name,
                        const On1xSDK_Program *program, size_t limit, size_t max,
                        Publish &&publish) noexcept {
    std::string_view key;
    if (!registry || !program) return ON1X_SDK_INVALID_ARGUMENT;
    if (const auto status = read_name(name, key); status != ON1X_SDK_OK) return status;
    const auto *descriptor = find_facet<Descriptor>(registry, kind, key);
    if (!descriptor || descriptor->phase != program->phase) return ON1X_SDK_UNSUPPORTED;
    if (limit > max) return ON1X_SDK_LIMIT;
    On1xSDK_OutputSink sink{{}, limit};
    const auto status = invoke(program, descriptor->run, descriptor->userdata, sink);
    if (status != ON1X_SDK_OK) return status;
    return guard([&] { return publish(std::move(sink.bytes)); });
}

} // namespace on1x::sdk

#endif
