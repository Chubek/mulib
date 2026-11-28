#include "CoreInternal.hpp"

#include <cstring>
#include <limits>
#include <new>

namespace {

bool on_handle_thread(const On1xSDK_Context *context) noexcept {
    return context &&
        context->handle_thread == std::this_thread::get_id();
}

struct Located {
    On1xSDK_Context::HandleSlot *slot = nullptr;
    On1xSDK_Status status = ON1X_SDK_OK;
};

Located locate(
    On1xSDK_Context *context, const On1xSDK_HandleRef *reference
) noexcept {
    if (!context || !reference || !reference->handle ||
        reference->owner_context != context->owner) {
        return {nullptr, ON1X_SDK_INVALID_ARGUMENT};
    }
    const auto slot_number = on1x_sdk_handle_slot(reference->handle);
    const auto generation = on1x_sdk_handle_generation(reference->handle);
    if (!slot_number || !generation ||
        slot_number > context->handles.size()) {
        return {nullptr, ON1X_SDK_STALE};
    }
    auto &slot = context->handles[slot_number - 1];
    if (slot.retired || !slot.references || slot.generation != generation) {
        return {nullptr, ON1X_SDK_STALE};
    }
    if (reference->kind != slot.kind) {
        return {nullptr, ON1X_SDK_INVALID_ARGUMENT};
    }
    if (!on1x::sdk::v2::valid_state(reference->state)) {
        return {nullptr, ON1X_SDK_INVALID_ARGUMENT};
    }
    return {&slot, ON1X_SDK_OK};
}

} // namespace

extern "C" std::uint32_t on1x_sdk_handle_slot(On1xSDK_Handle handle) {
    return static_cast<std::uint32_t>(handle);
}

extern "C" std::uint32_t on1x_sdk_handle_generation(On1xSDK_Handle handle) {
    return static_cast<std::uint32_t>(handle >> 32);
}

extern "C" On1xSDK_Status on1x_sdk_handle_insert(
    On1xSDK_Context *context, const On1xSDK_HandleDescriptor *descriptor,
    On1xSDK_HandleRef *out
) {
    if (out) {
        *out = {};
    }
    if (!context || !descriptor || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!on_handle_thread(context)) {
        return ON1X_SDK_BUSY;
    }
    const auto status = on1x_sdk_abi_header_validate(
        descriptor, sizeof(On1xSDK_HandleDescriptor),
        ON1X_SDK_ABI_VERSION_V2, nullptr
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (descriptor->kind == ON1X_SDK_HANDLE_KIND_NONE ||
        !on1x::sdk::v2::valid_state(descriptor->initial_state) ||
        !descriptor->host_object) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    try {
        std::lock_guard lock(context->handle_mutex);
        std::size_t index = context->handles.size();
        for (std::size_t i = 0; i < context->handles.size(); ++i) {
            const auto &slot = context->handles[i];
            if (!slot.retired && !slot.references) {
                index = i;
                break;
            }
        }
        if (index == context->handles.size()) {
            if (context->handles.size() >= context->max_handles ||
                context->handles.size() >= UINT32_MAX) {
                return ON1X_SDK_LIMIT;
            }
            context->handles.emplace_back();
        }
        auto &slot = context->handles[index];
        if (!slot.generation) {
            slot.generation = 1;
        }
        slot.references = 1;
        slot.kind = descriptor->kind;
        slot.state = descriptor->initial_state;
        slot.object = descriptor->host_object;
        slot.drop = descriptor->drop;
        slot.drop_userdata = descriptor->drop_userdata;
        const auto slot_number = static_cast<std::uint32_t>(index + 1);
        out->owner_context = context->owner;
        out->handle =
            (static_cast<std::uint64_t>(slot.generation) << 32) | slot_number;
        out->kind = slot.kind;
        out->state = slot.state;
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" On1xSDK_Status on1x_sdk_handle_retain(
    On1xSDK_Context *context, const On1xSDK_HandleRef *reference
) {
    if (!context || !reference) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!on_handle_thread(context)) {
        return ON1X_SDK_BUSY;
    }
    std::lock_guard lock(context->handle_mutex);
    auto found = locate(context, reference);
    if (found.status != ON1X_SDK_OK) {
        return found.status;
    }
    if (found.slot->references == UINT32_MAX) {
        return ON1X_SDK_LIMIT;
    }
    ++found.slot->references;
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_handle_release(
    On1xSDK_Context *context, const On1xSDK_HandleRef *reference
) {
    if (!context || !reference) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!on_handle_thread(context)) {
        return ON1X_SDK_BUSY;
    }
    On1xSDK_HandleDropFn drop = nullptr;
    void *drop_userdata = nullptr;
    void *object = nullptr;
    {
        std::lock_guard lock(context->handle_mutex);
        auto found = locate(context, reference);
        if (found.status != ON1X_SDK_OK) {
            return found.status;
        }
        auto &slot = *found.slot;
        if (--slot.references) {
            return ON1X_SDK_OK;
        }
        drop = slot.drop;
        drop_userdata = slot.drop_userdata;
        object = slot.object;
        slot.object = nullptr;
        slot.drop = nullptr;
        slot.drop_userdata = nullptr;
        slot.kind = ON1X_SDK_HANDLE_KIND_NONE;
        slot.state = ON1X_SDK_HANDLE_STATE_CLOSED;
        if (slot.generation == UINT32_MAX) {
            slot.retired = true;
        } else {
            ++slot.generation;
        }
    }
    if (drop) {
        try {
            drop(drop_userdata, object);
        } catch (...) {
            return ON1X_SDK_CALLBACK_FAILED;
        }
    }
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_handle_resolve(
    On1xSDK_Context *context, const On1xSDK_HandleRef *reference,
    std::uint32_t expected_kind, std::uint32_t allowed_states,
    void **borrowed_host_object
) {
    if (borrowed_host_object) {
        *borrowed_host_object = nullptr;
    }
    if (!context || !reference || !borrowed_host_object ||
        expected_kind == ON1X_SDK_HANDLE_KIND_NONE || !allowed_states) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!on_handle_thread(context)) {
        return ON1X_SDK_BUSY;
    }
    std::lock_guard lock(context->handle_mutex);
    auto found = locate(context, reference);
    if (found.status != ON1X_SDK_OK) {
        return found.status;
    }
    if (found.slot->kind != expected_kind) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!(found.slot->state & allowed_states)) {
        return ON1X_SDK_INVALID_STATE;
    }
    *borrowed_host_object = found.slot->object;
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_handle_transition(
    On1xSDK_Context *context, On1xSDK_HandleRef *reference,
    std::uint32_t expected_states, std::uint32_t new_state
) {
    if (!context || !reference || !expected_states ||
        !on1x::sdk::v2::valid_state(new_state)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!on_handle_thread(context)) {
        return ON1X_SDK_BUSY;
    }
    std::lock_guard lock(context->handle_mutex);
    auto found = locate(context, reference);
    if (found.status != ON1X_SDK_OK) {
        return found.status;
    }
    if (!(found.slot->state & expected_states)) {
        return ON1X_SDK_INVALID_STATE;
    }
    found.slot->state = new_state;
    reference->state = new_state;
    return ON1X_SDK_OK;
}
