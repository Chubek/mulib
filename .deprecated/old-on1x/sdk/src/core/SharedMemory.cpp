#include "On1xSDK-PluginSignature.h"
#include "On1xSDK-SharedMemory.h"

#include "Wire.hpp"
#include "mulib-shm.h"

#include <atomic>
#include <cstring>
#include <new>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#endif

extern "C" int on1x_sdk_mu_sha256(
    const void *data, size_t length, unsigned char out[32]
);

struct On1xSDK_SharedMemory {
    std::atomic<std::uint32_t> references{1};
    On1xSDK_Context *context = nullptr;
    mulib_shm_t segment = MULIB_SHM_INIT;
    On1xSDK_HandleRef handle{};
    std::uint64_t remote_owner = 0;
    On1xSDK_Handle remote_handle = 0;
    std::uint32_t flags = 0;
    std::size_t used_size = 0;
    std::uint8_t digest[32]{};
    bool creator = false;
};

namespace {

On1xSDK_Status map_shm_error(int error) noexcept {
    switch (error) {
    case 0:
        return ON1X_SDK_OK;
    case -1:
        return ON1X_SDK_INVALID_ARGUMENT;
    case -3:
        return ON1X_SDK_UNSUPPORTED;
    default:
        return ON1X_SDK_IO_ERROR;
    }
}

bool descriptor_shape(
    const On1xSDK_SharedMemoryDescriptor &descriptor
) noexcept {
    std::uint8_t digest_bits = 0;
    for (const auto byte : descriptor.digest_sha256) {
        digest_bits |= byte;
    }
    const bool one_state =
        !!(descriptor.flags & ON1X_SDK_SHARED_MEMORY_FROZEN) ^
        !!(descriptor.flags & ON1X_SDK_SHARED_MEMORY_STAGING);
    return descriptor.schema_major ==
               ON1X_SDK_SHARED_MEMORY_SCHEMA_MAJOR &&
           descriptor.schema_minor <=
               ON1X_SDK_SHARED_MEMORY_SCHEMA_MINOR &&
           (descriptor.flags == ON1X_SDK_SHARED_MEMORY_FROZEN ||
            descriptor.flags == ON1X_SDK_SHARED_MEMORY_STAGING) &&
           one_state &&
           descriptor.owner_context && descriptor.handle &&
           descriptor.kind == ON1X_SDK_HANDLE_KIND_SHARED_MEMORY &&
           (descriptor.state == ON1X_SDK_HANDLE_STATE_CREATED ||
            descriptor.state == ON1X_SDK_HANDLE_STATE_FROZEN) &&
           descriptor.platform_id >= 0 && !descriptor.reserved &&
           descriptor.capacity && descriptor.capacity <=
               ON1X_SDK_SHARED_MEMORY_MAX_BYTES &&
           descriptor.size <= descriptor.capacity &&
           ((descriptor.flags == ON1X_SDK_SHARED_MEMORY_FROZEN &&
             descriptor.state == ON1X_SDK_HANDLE_STATE_FROZEN) ||
            (descriptor.flags == ON1X_SDK_SHARED_MEMORY_STAGING &&
             descriptor.state == ON1X_SDK_HANDLE_STATE_CREATED &&
             descriptor.size == 0 && digest_bits == 0));
}

bool same_descriptor(
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

void close_memory(On1xSDK_SharedMemory *memory) noexcept {
    if (!memory) {
        return;
    }
    if (memory->creator && memory->segment.id >= 0) {
        (void)mulib_shm_remove(memory->segment.id);
    }
    (void)mulib_shm_close(&memory->segment);
    if (memory->context && memory->handle.handle) {
        (void)on1x_sdk_handle_release(memory->context, &memory->handle);
    }
    if (memory->context) {
        on1x_sdk_context_release(memory->context);
    }
    delete memory;
}

} // namespace

extern "C" On1xSDK_Status on1x_sdk_sha256(
    On1xSDK_ByteView bytes, std::uint8_t digest[32]
) {
    if (!digest || (!bytes.data && bytes.size)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    return on1x_sdk_mu_sha256(bytes.data, bytes.size, digest)
        ? ON1X_SDK_OK : ON1X_SDK_ERROR;
}

extern "C" int on1x_sdk_digest_equal(
    const std::uint8_t left[32], const std::uint8_t right[32]
) {
    if (!left || !right) {
        return 0;
    }
    std::uint8_t different = 0;
    for (unsigned i = 0; i < 32; ++i) {
        different |= static_cast<std::uint8_t>(left[i] ^ right[i]);
    }
    return different == 0;
}

extern "C" On1xSDK_Status on1x_sdk_shared_memory_create_staging(
    On1xSDK_Context *context, std::size_t capacity,
    On1xSDK_SharedMemory **out
) {
    if (out) {
        *out = nullptr;
    }
    if (!context || !out || !capacity) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (capacity > ON1X_SDK_SHARED_MEMORY_MAX_BYTES) {
        return ON1X_SDK_LIMIT;
    }
    try {
        auto *memory = new On1xSDK_SharedMemory;
        const auto error = mulib_shm_create(&memory->segment, capacity);
        if (error) {
            delete memory;
            return map_shm_error(error);
        }
        memory->context = context;
        on1x_sdk_context_retain(context);
        memory->flags = ON1X_SDK_SHARED_MEMORY_STAGING;
        memory->creator = true;
        const On1xSDK_HandleDescriptor descriptor{
            ON1X_SDK_ABI_VERSION_V2,
            sizeof(On1xSDK_HandleDescriptor),
            ON1X_SDK_HANDLE_KIND_SHARED_MEMORY,
            ON1X_SDK_HANDLE_STATE_CREATED,
            memory,
            nullptr,
            nullptr
        };
        const auto status =
            on1x_sdk_handle_insert(context, &descriptor, &memory->handle);
        if (status != ON1X_SDK_OK) {
            close_memory(memory);
            return status;
        }
        *out = memory;
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" On1xSDK_Status on1x_sdk_shared_memory_create_frozen(
    On1xSDK_Context *context, On1xSDK_ByteView bytes,
    On1xSDK_SharedMemory **out
) {
    if (out) {
        *out = nullptr;
    }
    if (!context || !out || (!bytes.data && bytes.size)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    On1xSDK_SharedMemory *memory = nullptr;
    auto status =
        on1x_sdk_shared_memory_create_staging(
            context, bytes.size ? bytes.size : 1, &memory
        );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    if (bytes.size) {
        std::memcpy(memory->segment.data, bytes.data, bytes.size);
    }
    status = on1x_sdk_shared_memory_freeze(memory, bytes.size);
    if (status != ON1X_SDK_OK) {
        on1x_sdk_shared_memory_release(memory);
        return status;
    }
    *out = memory;
    return ON1X_SDK_OK;
}

extern "C" void on1x_sdk_shared_memory_retain(
    On1xSDK_SharedMemory *memory
) {
    if (memory) {
        memory->references.fetch_add(1, std::memory_order_relaxed);
    }
}

extern "C" void on1x_sdk_shared_memory_release(
    On1xSDK_SharedMemory *memory
) {
    if (memory &&
        memory->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        close_memory(memory);
    }
}

extern "C" On1xSDK_Status on1x_sdk_shared_memory_mutable_bytes(
    On1xSDK_SharedMemory *memory, std::uint8_t **data, std::size_t *capacity
) {
    if (data) {
        *data = nullptr;
    }
    if (capacity) {
        *capacity = 0;
    }
    if (!memory || !data || !capacity) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!(memory->flags & ON1X_SDK_SHARED_MEMORY_STAGING)) {
        return ON1X_SDK_INVALID_STATE;
    }
    *data = static_cast<std::uint8_t *>(memory->segment.data);
    *capacity = memory->segment.size;
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_shared_memory_freeze(
    On1xSDK_SharedMemory *memory, std::size_t used_size
) {
    if (!memory || !(memory->flags & ON1X_SDK_SHARED_MEMORY_STAGING)) {
        return memory ? ON1X_SDK_INVALID_STATE : ON1X_SDK_INVALID_ARGUMENT;
    }
    if (used_size > memory->segment.size) {
        return ON1X_SDK_LIMIT;
    }
    if (memory->context) {
        void *object = nullptr;
        const auto status = on1x_sdk_handle_resolve(
            memory->context, &memory->handle,
            ON1X_SDK_HANDLE_KIND_SHARED_MEMORY,
            ON1X_SDK_HANDLE_STATE_CREATED, &object
        );
        if (status != ON1X_SDK_OK) {
            return status;
        }
        if (object != memory) {
            return ON1X_SDK_VERIFICATION_FAILED;
        }
    }
    auto status = on1x_sdk_sha256(
        {static_cast<const std::uint8_t *>(memory->segment.data), used_size},
        memory->digest
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
#if defined(__unix__) || defined(__APPLE__)
    if (memory->context) {
        status = on1x_sdk_handle_transition(
            memory->context, &memory->handle,
            ON1X_SDK_HANDLE_STATE_CREATED,
            ON1X_SDK_HANDLE_STATE_FROZEN
        );
        if (status != ON1X_SDK_OK) {
            return status;
        }
    }
    if (mprotect(memory->segment.data, memory->segment.size, PROT_READ) != 0) {
        if (memory->context) {
            (void)on1x_sdk_handle_transition(
                memory->context, &memory->handle,
                ON1X_SDK_HANDLE_STATE_FROZEN,
                ON1X_SDK_HANDLE_STATE_CREATED
            );
        }
        return ON1X_SDK_IO_ERROR;
    }
#else
    return ON1X_SDK_UNSUPPORTED;
#endif
    memory->used_size = used_size;
    memory->flags = ON1X_SDK_SHARED_MEMORY_FROZEN;
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_shared_memory_bytes(
    const On1xSDK_SharedMemory *memory, const std::uint8_t **data,
    std::size_t *size
) {
    if (data) {
        *data = nullptr;
    }
    if (size) {
        *size = 0;
    }
    if (!memory || !data || !size) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    if (!(memory->flags & ON1X_SDK_SHARED_MEMORY_FROZEN)) {
        return ON1X_SDK_INVALID_STATE;
    }
    *data = static_cast<const std::uint8_t *>(memory->segment.data);
    *size = memory->used_size;
    return ON1X_SDK_OK;
}

static On1xSDK_Status export_descriptor(
    const On1xSDK_SharedMemory *memory, bool staging,
    On1xSDK_SharedMemoryDescriptor *out
) {
    if (out) {
        *out = {};
    }
    if (!memory || !out) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    const auto required = staging
        ? ON1X_SDK_SHARED_MEMORY_STAGING : ON1X_SDK_SHARED_MEMORY_FROZEN;
    if (!(memory->flags & required)) {
        return ON1X_SDK_INVALID_STATE;
    }
    out->schema_major = ON1X_SDK_SHARED_MEMORY_SCHEMA_MAJOR;
    out->schema_minor = ON1X_SDK_SHARED_MEMORY_SCHEMA_MINOR;
    out->flags = required;
    out->owner_context = memory->context
        ? memory->handle.owner_context : memory->remote_owner;
    out->handle = memory->context
        ? memory->handle.handle : memory->remote_handle;
    out->kind = ON1X_SDK_HANDLE_KIND_SHARED_MEMORY;
    out->state = staging
        ? ON1X_SDK_HANDLE_STATE_CREATED : ON1X_SDK_HANDLE_STATE_FROZEN;
    out->platform_id = memory->segment.id;
    out->capacity = memory->segment.size;
    out->size = staging ? 0 : memory->used_size;
    if (!staging) {
        std::memcpy(out->digest_sha256, memory->digest, 32);
    }
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_shared_memory_export(
    const On1xSDK_SharedMemory *memory,
    On1xSDK_SharedMemoryDescriptor *out
) {
    return export_descriptor(memory, false, out);
}

extern "C" On1xSDK_Status on1x_sdk_shared_memory_export_staging(
    const On1xSDK_SharedMemory *memory,
    On1xSDK_SharedMemoryDescriptor *out
) {
    return export_descriptor(memory, true, out);
}

extern "C" On1xSDK_Status on1x_sdk_shared_memory_import_authorized(
    const On1xSDK_SharedMemoryDescriptor *descriptor,
    std::uint32_t required_flags, On1xSDK_SharedMemory **out
) {
    if (out) {
        *out = nullptr;
    }
    if (!descriptor || !out ||
        (required_flags != ON1X_SDK_SHARED_MEMORY_FROZEN &&
         required_flags != ON1X_SDK_SHARED_MEMORY_STAGING) ||
        !descriptor_shape(*descriptor) ||
        !(descriptor->flags & required_flags)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    try {
        auto *memory = new On1xSDK_SharedMemory;
        const bool frozen =
            required_flags == ON1X_SDK_SHARED_MEMORY_FROZEN;
        const auto error = mulib_shm_attach(
            &memory->segment, descriptor->platform_id, frozen ? 1 : 0
        );
        if (error) {
            delete memory;
            return map_shm_error(error);
        }
        if (memory->segment.size != descriptor->capacity) {
            close_memory(memory);
            return ON1X_SDK_PROTOCOL_ERROR;
        }
        memory->remote_owner = descriptor->owner_context;
        memory->remote_handle = descriptor->handle;
        memory->flags = required_flags |
            (frozen ? ON1X_SDK_SHARED_MEMORY_READ_ONLY_IMPORT : 0);
        memory->used_size = descriptor->size;
        if (frozen) {
            std::uint8_t digest[32]{};
            auto status = on1x_sdk_sha256(
                {static_cast<const std::uint8_t *>(memory->segment.data),
                 memory->used_size},
                digest
            );
            if (status != ON1X_SDK_OK ||
                !on1x_sdk_digest_equal(digest, descriptor->digest_sha256)) {
                close_memory(memory);
                return status == ON1X_SDK_OK
                    ? ON1X_SDK_VERIFICATION_FAILED : status;
            }
            std::memcpy(memory->digest, digest, 32);
        }
        *out = memory;
        return ON1X_SDK_OK;
    } catch (const std::bad_alloc &) {
        return ON1X_SDK_LIMIT;
    } catch (...) {
        return ON1X_SDK_ERROR;
    }
}

extern "C" On1xSDK_Status on1x_sdk_shared_memory_descriptor_encode(
    const On1xSDK_SharedMemoryDescriptor *descriptor, void *output,
    std::size_t output_size
) {
    if (!descriptor || !output ||
        output_size != ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE ||
        !descriptor_shape(*descriptor)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    auto *cursor = static_cast<std::uint8_t *>(output);
    on1x::sdk::wire::put16(cursor, descriptor->schema_major);
    on1x::sdk::wire::put16(cursor, descriptor->schema_minor);
    on1x::sdk::wire::put32(cursor, descriptor->flags);
    on1x::sdk::wire::put64(cursor, descriptor->owner_context);
    on1x::sdk::wire::put64(cursor, descriptor->handle);
    on1x::sdk::wire::put32(cursor, descriptor->kind);
    on1x::sdk::wire::put32(cursor, descriptor->state);
    on1x::sdk::wire::put32(
        cursor, static_cast<std::uint32_t>(descriptor->platform_id)
    );
    on1x::sdk::wire::put32(cursor, 0);
    on1x::sdk::wire::put64(cursor, descriptor->capacity);
    on1x::sdk::wire::put64(cursor, descriptor->size);
    on1x::sdk::wire::put_bytes(cursor, descriptor->digest_sha256, 32);
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_shared_memory_descriptor_decode(
    const void *input, std::size_t input_size,
    On1xSDK_SharedMemoryDescriptor *out
) {
    if (out) {
        *out = {};
    }
    if (!input || !out ||
        input_size != ON1X_SDK_SHARED_MEMORY_DESCRIPTOR_WIRE_SIZE) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    const auto *cursor = static_cast<const std::uint8_t *>(input);
    out->schema_major = on1x::sdk::wire::get16(cursor);
    out->schema_minor = on1x::sdk::wire::get16(cursor);
    out->flags = on1x::sdk::wire::get32(cursor);
    out->owner_context = on1x::sdk::wire::get64(cursor);
    out->handle = on1x::sdk::wire::get64(cursor);
    out->kind = on1x::sdk::wire::get32(cursor);
    out->state = on1x::sdk::wire::get32(cursor);
    out->platform_id = static_cast<std::int32_t>(
        on1x::sdk::wire::get32(cursor)
    );
    out->reserved = on1x::sdk::wire::get32(cursor);
    out->capacity = on1x::sdk::wire::get64(cursor);
    out->size = on1x::sdk::wire::get64(cursor);
    std::memcpy(out->digest_sha256, cursor, 32);
    if (!descriptor_shape(*out)) {
        *out = {};
        return ON1X_SDK_PROTOCOL_ERROR;
    }
    return ON1X_SDK_OK;
}

extern "C" On1xSDK_Status on1x_sdk_shared_memory_validate_local(
    On1xSDK_Context *context,
    const On1xSDK_SharedMemoryDescriptor *descriptor
) {
    if (!context || !descriptor || !descriptor_shape(*descriptor)) {
        return ON1X_SDK_INVALID_ARGUMENT;
    }
    On1xSDK_HandleRef reference{
        descriptor->owner_context, descriptor->handle,
        descriptor->kind, descriptor->state
    };
    void *object = nullptr;
    const auto status = on1x_sdk_handle_resolve(
        context, &reference, ON1X_SDK_HANDLE_KIND_SHARED_MEMORY,
        descriptor->state, &object
    );
    if (status != ON1X_SDK_OK) {
        return status;
    }
    auto *const memory = static_cast<On1xSDK_SharedMemory *>(object);
    On1xSDK_SharedMemoryDescriptor local{};
    const auto export_status = export_descriptor(
        memory,
        descriptor->flags == ON1X_SDK_SHARED_MEMORY_STAGING,
        &local
    );
    if (export_status != ON1X_SDK_OK) {
        return export_status;
    }
    return same_descriptor(local, *descriptor)
        ? ON1X_SDK_OK : ON1X_SDK_VERIFICATION_FAILED;
}
