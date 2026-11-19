#pragma once

#include "On1xSDK-Analyzer.h"
#include "On1xSDK-Backend.h"
#include "On1xSDK-PluginManifest.h"
#include "On1xSDK-PluginProtocol.h"
#include "On1xSDK-PluginSignature.h"
#include "On1xSDK-SharedMemory.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace on1x::sdk_plugin {

inline constexpr unsigned version = ON1X_SDK_PLUGIN_PROTOCOL_MAJOR;
inline constexpr unsigned setup_version = 1;
inline constexpr unsigned max_image = ON1X_SDK_PLUGIN_IMAGE_MAX_BYTES;
inline constexpr unsigned max_frame = ON1X_SDK_PLUGIN_CONTROL_MAX_BYTES;
inline constexpr unsigned timeout_ms = 5000;
inline constexpr std::uint64_t request_id = 1;

inline void put(std::string &buffer, std::uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        buffer.push_back(static_cast<char>(value >> (24 - i * 8)));
}

inline void put(std::string &buffer, std::string_view value)
{
    if (value.size() > UINT32_MAX)
        throw std::runtime_error("plugin field length exceeded");
    put(buffer, static_cast<std::uint32_t>(value.size()));
    buffer.append(value);
}

struct Reader {
    std::string_view rest;

    std::uint32_t take() {
        if (rest.size() < 4)
            throw std::runtime_error("truncated plugin data field");
        std::uint32_t value = 0;
        for (unsigned char byte : rest.substr(0, 4))
            value = (value << 8) | byte;
        rest.remove_prefix(4);
        return value;
    }

    std::string_view take(std::size_t limit) {
        const auto size = take();
        if (size > limit || size > rest.size())
            throw std::runtime_error("invalid plugin data field length");
        const auto value = rest.substr(0, size);
        rest.remove_prefix(size);
        return value;
    }

    std::string_view fixed(std::size_t size) {
        if (size > rest.size())
            throw std::runtime_error("truncated plugin data field");
        const auto value = rest.substr(0, size);
        rest.remove_prefix(size);
        return value;
    }

    void finish() const {
        if (!rest.empty())
            throw std::runtime_error("trailing bytes in plugin data");
    }
};

inline std::string encode_message(
    std::uint16_t kind,
    On1xSDK_Status status = ON1X_SDK_OK,
    std::uint64_t id = 0,
    std::string_view payload = {},
    std::uint16_t flags = 0
) {
    const On1xSDK_ProtocolMessage message{
        ON1X_SDK_PLUGIN_PROTOCOL_MAJOR,
        ON1X_SDK_PLUGIN_PROTOCOL_MINOR,
        kind,
        flags,
        status,
        id,
        0,
        0,
        0,
        {reinterpret_cast<const std::uint8_t *>(payload.data()),
         payload.size()}
    };
    On1xSDK_OwnedBytes encoded{};
    const auto result = on1x_sdk_protocol_encode(
        &message, on1x_sdk_default_allocator(), &encoded
    );
    if (result != ON1X_SDK_OK)
        throw std::runtime_error("cannot encode plugin control message");
    try {
        std::string copy(
            reinterpret_cast<const char *>(encoded.data), encoded.size
        );
        on1x_sdk_owned_bytes_release(&encoded);
        return copy;
    } catch (...) {
        on1x_sdk_owned_bytes_release(&encoded);
        throw;
    }
}

inline On1xSDK_ProtocolMessage decode_message(std::string_view frame)
{
    On1xSDK_ProtocolMessage message{};
    const auto status = on1x_sdk_protocol_decode(
        {reinterpret_cast<const std::uint8_t *>(frame.data()), frame.size()},
        max_frame - ON1X_SDK_PLUGIN_PROTOCOL_HEADER_WIRE_SIZE,
        &message
    );
    if (status != ON1X_SDK_OK)
        throw std::runtime_error("invalid plugin control message");
    return message;
}

struct Setup {
    std::uint32_t mode = 0;
    std::string hook;
    On1xSDK_SharedMemoryDescriptor input{};
    On1xSDK_SharedMemoryDescriptor output{};
};

inline std::string encode_setup(
    std::uint32_t mode,
    std::string_view hook,
    const On1xSDK_SharedMemoryDescriptor &input,
    const On1xSDK_SharedMemoryDescriptor &output
) {
    std::string payload;
    payload.reserve(
        12 + hook.size() +
        2 * ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE
    );
    put(payload, setup_version);
    put(payload, mode);
    put(payload, hook);
    const auto old_size = payload.size();
    payload.resize(
        old_size + 2 * ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE
    );
    if (on1x_sdk_shared_memory_descriptor_encode(
            &input, payload.data() + old_size,
            ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE) !=
            ON1X_SDK_OK ||
        on1x_sdk_shared_memory_descriptor_encode(
            &output,
            payload.data() + old_size +
                ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE,
            ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE) !=
            ON1X_SDK_OK)
        throw std::runtime_error("cannot encode plugin data-plane setup");
    return payload;
}

inline Setup decode_setup(std::string_view payload)
{
    Reader reader{payload};
    if (reader.take() != setup_version)
        throw std::runtime_error("unknown plugin setup version");
    Setup setup;
    setup.mode = reader.take();
    setup.hook = std::string(reader.take(ON1X_SDK_NAME_MAX));
    const auto input = reader.fixed(
        ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE
    );
    const auto output = reader.fixed(
        ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE
    );
    reader.finish();
    if (setup.mode > 1 ||
        on1x_sdk_identifier_validate(
            {setup.hook.data(), setup.hook.size()}) != ON1X_SDK_OK ||
        on1x_sdk_shared_memory_descriptor_decode(
            input.data(), input.size(), &setup.input) != ON1X_SDK_OK ||
        on1x_sdk_shared_memory_descriptor_decode(
            output.data(), output.size(), &setup.output) != ON1X_SDK_OK)
        throw std::runtime_error("invalid plugin setup");
    return setup;
}

inline bool same_descriptor(
    const On1xSDK_SharedMemoryDescriptor &left,
    const On1xSDK_SharedMemoryDescriptor &right
) noexcept {
    return left.schema_major == right.schema_major &&
        left.schema_minor == right.schema_minor &&
        left.flags == right.flags &&
        left.owner_context == right.owner_context &&
        left.handle == right.handle &&
        left.kind == right.kind &&
        left.state == right.state &&
        left.platform_id == right.platform_id &&
        left.reserved == right.reserved &&
        left.capacity == right.capacity &&
        left.size == right.size &&
        on1x_sdk_digest_equal(
            left.digest_sha256, right.digest_sha256);
}

} // namespace on1x::sdk_plugin
