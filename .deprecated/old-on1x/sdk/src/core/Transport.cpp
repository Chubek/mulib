#include "On1xSDK-Transport.h"

#include "mulib-ipc-framed.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>

namespace {

On1xSDK_Status map_error(int error) noexcept {
    switch (error) {
    case 0:
        return ON1X_SDK_OK;
    case MULIB_SB_INVALID:
        return ON1X_SDK_INVALID_ARGUMENT;
    case MULIB_SB_UNSUPPORTED:
        return ON1X_SDK_UNSUPPORTED;
    case MULIB_SB_DENIED:
        return ON1X_SDK_CAPABILITY_DENIED;
    case MULIB_SB_LIMIT:
    case MULIB_SB_NOMEM:
        return ON1X_SDK_LIMIT;
    case MULIB_SB_TIMEOUT:
        return ON1X_SDK_DEADLINE_EXCEEDED;
    case MULIB_SB_PROTOCOL:
        return ON1X_SDK_PROTOCOL_ERROR;
    case MULIB_SB_STATE:
        return ON1X_SDK_INVALID_STATE;
    case MULIB_SB_EOF:
        return ON1X_SDK_IO_ERROR;
    default:
        return ON1X_SDK_IO_ERROR;
    }
}

On1xSDK_Status deadline_ms(
    std::uint64_t absolute_ns, std::int64_t &out
) noexcept {
    if (!absolute_ns) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    const auto now_ns = on1x_sdk_monotonic_now_ns();
    if (!now_ns) {
        return ON1X_SDK_IO_ERROR;
    }
    if (absolute_ns <= now_ns) {
        return ON1X_SDK_DEADLINE_EXCEEDED;
    }
    const auto rounded =
        absolute_ns / UINT64_C(1000000) +
        (absolute_ns % UINT64_C(1000000) != 0);
    if (rounded > static_cast<std::uint64_t>(
                      std::numeric_limits<std::int64_t>::max())) {
        return ON1X_SDK_LIMIT;
    }
    out = static_cast<std::int64_t>(rounded);
    return ON1X_SDK_OK;
}

} // namespace

struct On1xSDK_Transport {
    On1xSDK_Context *context = nullptr;
    mulib_ipc_framed_t framed{};
    std::uint32_t max_control = 0;
};

extern "C" On1xSDK_Status on1x_sdk_transport_create(
    const On1xSDK_TransportConfig *config, On1xSDK_Transport **out
) {
    if (out) {
        *out = nullptr;
    }
    if (!config || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    const auto status = on1x_sdk_abi_header_validate(
        config, sizeof(On1xSDK_TransportConfig),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (!config->context || config->connected_stream_fd < 0 ||
        config->max_control_bytes <
            ON1X_SDK_PLUGIN_PROTOCOL_HEADER_WIRE_SIZE ||
        config->max_control_bytes > ON1X_SDK_PLUGIN_CONTROL_MAX_BYTES) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    try {
        auto *transport = new On1xSDK_Transport;
        const auto error = mulib_ipc_framed_init(
            &transport->framed, config->connected_stream_fd,
            config->max_control_bytes
        );
        if (error) {
            delete transport;
            return map_error(error);
        }
        transport->context = config->context;
        on1x_sdk_context_retain(transport->context);
        transport->max_control = config->max_control_bytes;
        *out = transport;
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" void on1x_sdk_transport_destroy(On1xSDK_Transport *transport) {
    if (!transport) {
        return;
    }
    on1x_sdk_context_release(transport->context);
    delete transport;
}

extern "C" On1xSDK_Status on1x_sdk_transport_send(
    On1xSDK_Transport *transport, On1xSDK_ByteView frame,
    std::uint64_t absolute_deadline_ns,
    const On1xSDK_Cancellation *cancellation,
    On1xSDK_Budget *budget
) {
    if (!transport || (!frame.data && frame.size) ||
        frame.size > transport->max_control) {
        return frame.size > (transport ? transport->max_control : 0)
            ? ON1X_SDK_LIMIT : ON1X_SDK_INVALID_ARGUMENT;
    }
    auto status = on1x_sdk_cancellation_poll(cancellation);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (budget) {
        status = on1x_sdk_budget_consume(
            budget, ON1X_SDK_BUDGET_IPC, 1
        );
        if (status == ON1X_SDK_OK) {
            status = on1x_sdk_budget_consume(
                budget, ON1X_SDK_BUDGET_BYTES, frame.size
            );
        }
        if (status != ON1X_SDK_OK) {
            return status;
        }
    }
    std::int64_t deadline = 0;
    status = deadline_ms(absolute_deadline_ns, deadline);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    return map_error(mulib_ipc_send(
        &transport->framed, frame.data, frame.size, deadline
    ));
}

extern "C" On1xSDK_Status on1x_sdk_transport_receive(
    On1xSDK_Transport *transport, std::uint64_t absolute_deadline_ns,
    const On1xSDK_Cancellation *cancellation, On1xSDK_Budget *budget,
    On1xSDK_OwnedBytes *out
) {
    if (out) {
        *out = {};
    }
    if (!transport || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    auto status = on1x_sdk_cancellation_poll(cancellation);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (budget) {
        status = on1x_sdk_budget_consume(
            budget, ON1X_SDK_BUDGET_IPC, 1
        );
        if (status != ON1X_SDK_OK) {
            return status;
        }
    }
    std::int64_t deadline = 0;
    status = deadline_ms(absolute_deadline_ns, deadline);
    if (status != ON1X_SDK_OK) {
        return status;
    }
    const auto *allocator = on1x_sdk_context_allocator(transport->context);
    void *memory = nullptr;
    status = on1x_sdk_allocate(
        allocator, transport->max_control, alignof(std::max_align_t), &memory
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    std::size_t size = 0;
    const auto error = mulib_ipc_receive(
        &transport->framed, memory, transport->max_control, &size, deadline
    );
    if (error) {
        (void)on1x_sdk_release(
            allocator, memory, transport->max_control,
            alignof(std::max_align_t)
        );
        return map_error(error);
    }
    if (budget &&
        (status = on1x_sdk_budget_consume(
             budget, ON1X_SDK_BUDGET_BYTES, size)) != ON1X_SDK_OK) {
        (void)on1x_sdk_release(
            allocator, memory, transport->max_control,
            alignof(std::max_align_t)
        );
        return status;
    }
    out->data = static_cast<std::uint8_t *>(memory);
    out->size = size;
    out->capacity = transport->max_control;
    out->allocator = allocator;
    out->owner_context = on1x_sdk_context_owner(transport->context);
    out->retained_context = transport->context;
    on1x_sdk_context_retain(out->retained_context);
    return ON1X_SDK_OK;
}
