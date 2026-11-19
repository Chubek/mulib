#include "trusted_bridge.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define ON1X_SDK_NATIVE_SLOT_MAX 24u

enum NativeKind {
    NATIVE_KIND_NONE = 0,
    NATIVE_KIND_CONTEXT = 1,
    NATIVE_KIND_SNAPSHOT = 2,
    NATIVE_KIND_CANDIDATE = 3
};

enum NativeLifecycle {
    NATIVE_LIFECYCLE_DISCOVERED = 0,
    NATIVE_LIFECYCLE_ACTIVE = 1,
    NATIVE_LIFECYCLE_SHUTDOWN = 2
};

typedef struct NativeContext {
    uint32_t sdk_abi_version;
    uint32_t bridge_abi_version;
    uint32_t bridge_table_size;
    uint32_t source_schema;
    uint32_t candidate_schema;
    uint32_t candidate_limit;
    uint64_t source_revision;
    uint64_t granted_capabilities;
    uint8_t source[ON1X_SDK_NATIVE_SOURCE_MAX];
    size_t source_size;
    uint32_t work_limit;
    uint32_t work_used;
    uint32_t cancel_after_polls;
    uint32_t cancel_polls;
    uint32_t fail_diagnostic_after;
    uint32_t fail_initialize;
    uint32_t fail_publish;
    uint32_t lifecycle;
    uint32_t metadata_state;
    uint32_t metadata_schema;
    uint32_t extension_id;
    uint32_t version_major;
    uint32_t version_minor;
    uint64_t requested_capabilities;
    uint32_t execution_profile;
    On1xSDKNative_Diagnostic diagnostics[ON1X_SDK_NATIVE_DIAGNOSTIC_MAX];
    size_t diagnostic_count;
    uint8_t published[ON1X_SDK_NATIVE_CANDIDATE_MAX];
    size_t published_size;
    uint32_t published_schema;
    uint64_t published_revision;
} NativeContext;

typedef struct NativeSnapshot {
    uint32_t schema;
    uint64_t revision;
    uint64_t length;
} NativeSnapshot;

typedef struct NativeCandidate {
    uint32_t schema;
    uint64_t source_revision;
    uint32_t limit;
    uint8_t bytes[ON1X_SDK_NATIVE_CANDIDATE_MAX];
    uint32_t length;
} NativeCandidate;

typedef union NativePayload {
    NativeContext context;
    NativeSnapshot snapshot;
    NativeCandidate candidate;
} NativePayload;

typedef struct NativeSlot {
    uint32_t generation;
    uint32_t kind;
    uint64_t owner;
    uint32_t live;
    uint32_t retired;
    NativePayload payload;
} NativeSlot;

static NativeSlot native_slots[ON1X_SDK_NATIVE_SLOT_MAX + 1u];

static uint64_t native_handle(uint32_t slot, uint32_t generation)
{
    return ((uint64_t)generation << 32u) | (uint64_t)slot;
}

static uint32_t native_handle_slot(uint64_t handle)
{
    return (uint32_t)(handle & UINT64_C(0xffffffff));
}

static uint32_t native_handle_generation(uint64_t handle)
{
    return (uint32_t)(handle >> 32u);
}

static On1xSDKNative_Status native_allocate(
    uint32_t kind,
    uint64_t owner,
    NativeSlot **out_slot,
    uint64_t *out_handle
)
{
    uint32_t index;
    if (out_slot == NULL || out_handle == NULL)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    *out_slot = NULL;
    *out_handle = 0u;
    for (index = 1u; index <= ON1X_SDK_NATIVE_SLOT_MAX; ++index) {
        NativeSlot *slot = &native_slots[index];
        if (slot->live || slot->retired)
            continue;
        if (slot->generation == 0u)
            slot->generation = 1u;
        memset(&slot->payload, 0, sizeof(slot->payload));
        slot->kind = kind;
        slot->owner = owner;
        slot->live = 1u;
        *out_slot = slot;
        *out_handle = native_handle(index, slot->generation);
        return ON1X_SDK_NATIVE_OK;
    }
    return ON1X_SDK_NATIVE_LIMIT;
}

static void native_release(NativeSlot *slot)
{
    if (slot == NULL || !slot->live)
        return;
    memset(&slot->payload, 0, sizeof(slot->payload));
    slot->kind = NATIVE_KIND_NONE;
    slot->owner = 0u;
    slot->live = 0u;
    if (slot->generation == UINT32_MAX) {
        slot->retired = 1u;
    } else {
        ++slot->generation;
    }
}

static On1xSDKNative_Status native_lookup(
    uint64_t handle,
    uint32_t kind,
    uint64_t owner,
    NativeSlot **out_slot
)
{
    const uint32_t index = native_handle_slot(handle);
    const uint32_t generation = native_handle_generation(handle);
    NativeSlot *slot;
    if (out_slot == NULL)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    *out_slot = NULL;
    if (handle == 0u || generation == 0u || index == 0u ||
        index > ON1X_SDK_NATIVE_SLOT_MAX)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    slot = &native_slots[index];
    if (slot->generation != generation)
        return ON1X_SDK_NATIVE_STALE;
    if (!slot->live)
        return ON1X_SDK_NATIVE_NOT_FOUND;
    if (slot->kind != kind || (owner != 0u && slot->owner != owner))
        return ON1X_SDK_NATIVE_CONFLICT;
    *out_slot = slot;
    return ON1X_SDK_NATIVE_OK;
}

static On1xSDKNative_Status native_context(
    uint64_t handle,
    NativeSlot **out_slot,
    NativeContext **out_context
)
{
    NativeSlot *slot = NULL;
    const On1xSDKNative_Status status =
        native_lookup(handle, NATIVE_KIND_CONTEXT, 0u, &slot);
    if (out_slot != NULL)
        *out_slot = NULL;
    if (out_context != NULL)
        *out_context = NULL;
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (out_slot != NULL)
        *out_slot = slot;
    if (out_context != NULL)
        *out_context = &slot->payload.context;
    return ON1X_SDK_NATIVE_OK;
}

static On1xSDKNative_Status native_owned(
    uint64_t context,
    uint64_t object,
    uint32_t kind,
    NativeContext **out_context,
    NativeSlot **out_object
)
{
    NativeContext *owner_context = NULL;
    NativeSlot *object_slot = NULL;
    On1xSDKNative_Status status =
        native_context(context, NULL, &owner_context);
    if (out_context != NULL)
        *out_context = NULL;
    if (out_object != NULL)
        *out_object = NULL;
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    status = native_lookup(object, kind, context, &object_slot);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (out_context != NULL)
        *out_context = owner_context;
    if (out_object != NULL)
        *out_object = object_slot;
    return ON1X_SDK_NATIVE_OK;
}

static size_t native_live_owned(uint64_t context)
{
    uint32_t index;
    size_t count = 0u;
    for (index = 1u; index <= ON1X_SDK_NATIVE_SLOT_MAX; ++index) {
        const NativeSlot *slot = &native_slots[index];
        if (slot->live && slot->owner == context)
            ++count;
    }
    return count;
}

static int native_utf8(const uint8_t *bytes, size_t size)
{
    size_t i = 0u;
    while (i < size) {
        const uint8_t first = bytes[i++];
        uint32_t value;
        uint32_t minimum;
        unsigned trailing;
        unsigned j;
        if (first == 0u)
            return 0;
        if (first < 0x80u)
            continue;
        if (first >= 0xc2u && first <= 0xdfu) {
            value = (uint32_t)(first & 0x1fu);
            minimum = 0x80u;
            trailing = 1u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            value = (uint32_t)(first & 0x0fu);
            minimum = 0x800u;
            trailing = 2u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            value = (uint32_t)(first & 0x07u);
            minimum = 0x10000u;
            trailing = 3u;
        } else {
            return 0;
        }
        if (size - i < trailing)
            return 0;
        for (j = 0u; j < trailing; ++j) {
            const uint8_t next = bytes[i++];
            if ((next & 0xc0u) != 0x80u)
                return 0;
            value = (value << 6u) | (uint32_t)(next & 0x3fu);
        }
        if (value < minimum || value > 0x10ffffu ||
            (value >= 0xd800u && value <= 0xdfffu))
            return 0;
    }
    return 1;
}

On1xSDKNative_Status on1x_sdk_native_context_create(
    const On1xSDKNative_ContextConfig *config,
    On1xSDKNative_Handle *out_context
)
{
    const size_t required =
        offsetof(On1xSDKNative_ContextConfig, fail_publish) +
        sizeof(config->fail_publish);
    NativeSlot *slot = NULL;
    uint64_t handle = 0u;
    On1xSDKNative_Status status;
    NativeContext *context;
    if (out_context != NULL)
        *out_context = 0u;
    if (config == NULL || out_context == NULL)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    if (config->abi_version != ON1X_SDK_NATIVE_BRIDGE_ABI ||
        config->struct_size < required)
        return ON1X_SDK_NATIVE_ABI_MISMATCH;
    if (config->source_size > ON1X_SDK_NATIVE_SOURCE_MAX ||
        config->candidate_limit > ON1X_SDK_NATIVE_CANDIDATE_MAX)
        return ON1X_SDK_NATIVE_LIMIT;
    if ((config->source_size != 0u && config->source_data == NULL) ||
        config->source_revision == 0u ||
        config->source_schema == 0u ||
        config->candidate_schema == 0u ||
        config->candidate_limit == 0u ||
        config->work_limit == 0u ||
        (config->granted_capabilities &
            ~(ON1X_SDK_NATIVE_CAP_DIAGNOSTIC_EMIT |
              ON1X_SDK_NATIVE_CAP_CANDIDATE_SUBMIT)) != 0u)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    if (!native_utf8(config->source_data, config->source_size))
        return ON1X_SDK_NATIVE_VERIFICATION_FAILED;
    status = native_allocate(
        NATIVE_KIND_CONTEXT, 0u, &slot, &handle);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    context = &slot->payload.context;
    context->sdk_abi_version = config->sdk_abi_version;
    context->bridge_abi_version = config->bridge_abi_version;
    context->bridge_table_size = config->bridge_table_size;
    context->source_schema = config->source_schema;
    context->candidate_schema = config->candidate_schema;
    context->candidate_limit = config->candidate_limit;
    context->source_revision = config->source_revision;
    context->granted_capabilities = config->granted_capabilities;
    if (config->source_size != 0u)
        memcpy(context->source, config->source_data, config->source_size);
    context->source_size = config->source_size;
    context->work_limit = config->work_limit;
    context->cancel_after_polls = config->cancel_after_polls;
    context->fail_diagnostic_after = config->fail_diagnostic_after;
    context->fail_initialize = config->fail_initialize;
    context->fail_publish = config->fail_publish;
    context->lifecycle = NATIVE_LIFECYCLE_DISCOVERED;
    *out_context = handle;
    return ON1X_SDK_NATIVE_OK;
}

On1xSDKNative_Status on1x_sdk_native_context_destroy(
    On1xSDKNative_Handle context
)
{
    NativeSlot *slot = NULL;
    const On1xSDKNative_Status status =
        native_context(context, &slot, NULL);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (native_live_owned(context) != 0u)
        return ON1X_SDK_NATIVE_BUSY;
    native_release(slot);
    return ON1X_SDK_NATIVE_OK;
}

On1xSDKNative_Status on1x_sdk_native_module_validate(
    const On1xSDKNative_Module *module
)
{
    static const char expected[] = "org.on1x.reference-byte-analyzer";
    const size_t required =
        offsetof(On1xSDKNative_Module, invoke) + sizeof(module->invoke);
    if (module == NULL)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    if (module->abi_version != ON1X_SDK_NATIVE_BRIDGE_ABI ||
        module->struct_size < required ||
        module->sdk_abi_version != ON1X_SDK_NATIVE_SDK_ABI ||
        module->bridge_abi_version != ON1X_SDK_NATIVE_BRIDGE_ABI)
        return ON1X_SDK_NATIVE_ABI_MISMATCH;
    if (module->source_schema != ON1X_SDK_NATIVE_SOURCE_SCHEMA ||
        module->candidate_schema != ON1X_SDK_NATIVE_CANDIDATE_SCHEMA ||
        module->requested_capabilities !=
            (ON1X_SDK_NATIVE_CAP_DIAGNOSTIC_EMIT |
             ON1X_SDK_NATIVE_CAP_CANDIDATE_SUBMIT) ||
        module->name == NULL ||
        module->name_length != sizeof(expected) - 1u ||
        memcmp(module->name, expected, sizeof(expected) - 1u) != 0 ||
        module->invoke == NULL)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    return ON1X_SDK_NATIVE_OK;
}

On1xSDKNative_Status on1x_sdk_native_context_diagnostic_count(
    On1xSDKNative_Handle context,
    size_t *out_count
)
{
    NativeContext *value = NULL;
    const On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (out_count != NULL)
        *out_count = 0u;
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (out_count == NULL)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    *out_count = value->diagnostic_count;
    return ON1X_SDK_NATIVE_OK;
}

On1xSDKNative_Status on1x_sdk_native_context_diagnostic(
    On1xSDKNative_Handle context,
    size_t index,
    On1xSDKNative_Diagnostic *out_diagnostic
)
{
    NativeContext *value = NULL;
    const On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (out_diagnostic != NULL)
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (out_diagnostic == NULL)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    if (index >= value->diagnostic_count)
        return ON1X_SDK_NATIVE_NOT_FOUND;
    *out_diagnostic = value->diagnostics[index];
    return ON1X_SDK_NATIVE_OK;
}

On1xSDKNative_Status on1x_sdk_native_context_candidate(
    On1xSDKNative_Handle context,
    const uint8_t **out_bytes,
    size_t *out_size
)
{
    NativeContext *value = NULL;
    const On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (out_bytes != NULL)
        *out_bytes = NULL;
    if (out_size != NULL)
        *out_size = 0u;
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (out_bytes == NULL || out_size == NULL)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    if (value->published_size == 0u)
        return ON1X_SDK_NATIVE_NOT_FOUND;
    *out_bytes = value->published;
    *out_size = value->published_size;
    return ON1X_SDK_NATIVE_OK;
}

On1xSDKNative_Status on1x_sdk_native_context_live_objects(
    On1xSDKNative_Handle context,
    size_t *out_count
)
{
    const On1xSDKNative_Status status =
        native_context(context, NULL, NULL);
    if (out_count != NULL)
        *out_count = 0u;
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (out_count == NULL)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    *out_count = native_live_owned(context);
    return ON1X_SDK_NATIVE_OK;
}

On1xSDKNative_Status on1x_sdk_native_context_metadata_state(
    On1xSDKNative_Handle context,
    uint32_t *out_state
)
{
    NativeContext *value = NULL;
    const On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (out_state != NULL)
        *out_state = 0u;
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (out_state == NULL)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    *out_state = value->metadata_state;
    return ON1X_SDK_NATIVE_OK;
}

uint32_t basis_sdk_bridge_negotiate(
    uint64_t context,
    uint32_t sdk_abi,
    uint32_t bridge_abi,
    uint32_t bridge_size
)
{
    NativeContext *value = NULL;
    const On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (sdk_abi != ON1X_SDK_NATIVE_SDK_ABI ||
        bridge_abi != ON1X_SDK_NATIVE_BRIDGE_ABI ||
        value->sdk_abi_version != sdk_abi ||
        value->bridge_abi_version != bridge_abi ||
        bridge_size > value->bridge_table_size)
        return ON1X_SDK_NATIVE_ABI_MISMATCH;
    return ON1X_SDK_NATIVE_OK;
}

uint32_t basis_sdk_bridge_metadata_register(
    uint64_t context,
    uint32_t metadata_schema,
    uint32_t extension_id,
    uint32_t version_major,
    uint32_t version_minor,
    uint64_t requested_capabilities,
    uint32_t execution_profile
)
{
    NativeContext *value = NULL;
    On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (value->lifecycle != NATIVE_LIFECYCLE_DISCOVERED)
        return ON1X_SDK_NATIVE_INVALID_STATE;
    if (value->metadata_state != 0u)
        return ON1X_SDK_NATIVE_DUPLICATE;
    if (metadata_schema != ON1X_SDK_NATIVE_METADATA_SCHEMA ||
        extension_id != ON1X_SDK_NATIVE_EXTENSION_ID ||
        version_major != ON1X_SDK_NATIVE_EXTENSION_VERSION_MAJOR ||
        version_minor != ON1X_SDK_NATIVE_EXTENSION_VERSION_MINOR ||
        requested_capabilities !=
            (ON1X_SDK_NATIVE_CAP_DIAGNOSTIC_EMIT |
             ON1X_SDK_NATIVE_CAP_CANDIDATE_SUBMIT) ||
        execution_profile != ON1X_SDK_NATIVE_PROFILE_TRUSTED_STATIC)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    value->metadata_schema = metadata_schema;
    value->extension_id = extension_id;
    value->version_major = version_major;
    value->version_minor = version_minor;
    value->requested_capabilities = requested_capabilities;
    value->execution_profile = execution_profile;
    value->metadata_state = 1u;
    return ON1X_SDK_NATIVE_OK;
}

uint32_t basis_sdk_bridge_metadata_rollback(uint64_t context)
{
    NativeContext *value = NULL;
    const On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (value->lifecycle != NATIVE_LIFECYCLE_DISCOVERED ||
        value->metadata_state != 1u)
        return ON1X_SDK_NATIVE_INVALID_STATE;
    value->metadata_state = 0u;
    value->metadata_schema = 0u;
    value->extension_id = 0u;
    value->version_major = 0u;
    value->version_minor = 0u;
    value->requested_capabilities = 0u;
    value->execution_profile = 0u;
    return ON1X_SDK_NATIVE_OK;
}

uint32_t basis_sdk_bridge_lifecycle(uint64_t context, uint32_t event)
{
    NativeContext *value = NULL;
    const On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (event == 1u) {
        if (value->lifecycle != NATIVE_LIFECYCLE_DISCOVERED ||
            value->metadata_state != 1u)
            return ON1X_SDK_NATIVE_INVALID_STATE;
        if (value->fail_initialize)
            return ON1X_SDK_NATIVE_ERROR;
        value->metadata_state = 2u;
        value->lifecycle = NATIVE_LIFECYCLE_ACTIVE;
        return ON1X_SDK_NATIVE_OK;
    }
    if (event == 2u) {
        if (value->lifecycle != NATIVE_LIFECYCLE_ACTIVE ||
            value->metadata_state != 2u)
            return ON1X_SDK_NATIVE_INVALID_STATE;
        if (native_live_owned(context) != 0u)
            return ON1X_SDK_NATIVE_BUSY;
        value->metadata_state = 0u;
        value->lifecycle = NATIVE_LIFECYCLE_SHUTDOWN;
        return ON1X_SDK_NATIVE_OK;
    }
    return ON1X_SDK_NATIVE_UNSUPPORTED;
}

uint32_t basis_sdk_bridge_require_active(uint64_t context)
{
    NativeContext *value = NULL;
    const On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    return value->lifecycle == NATIVE_LIFECYCLE_ACTIVE &&
        value->metadata_state == 2u ?
        ON1X_SDK_NATIVE_OK : ON1X_SDK_NATIVE_INVALID_STATE;
}

uint32_t basis_sdk_bridge_capability_check(
    uint64_t context,
    uint32_t capability
)
{
    NativeContext *value = NULL;
    uint64_t bit;
    On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (value->lifecycle != NATIVE_LIFECYCLE_ACTIVE)
        return ON1X_SDK_NATIVE_INVALID_STATE;
    if (capability == 1u)
        bit = ON1X_SDK_NATIVE_CAP_DIAGNOSTIC_EMIT;
    else if (capability == 2u)
        bit = ON1X_SDK_NATIVE_CAP_CANDIDATE_SUBMIT;
    else
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    return (value->granted_capabilities & bit) != 0u ?
        ON1X_SDK_NATIVE_OK : ON1X_SDK_NATIVE_CAPABILITY_DENIED;
}

uint32_t basis_sdk_bridge_cancel_check(uint64_t context)
{
    NativeContext *value = NULL;
    On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (value->lifecycle != NATIVE_LIFECYCLE_ACTIVE)
        return ON1X_SDK_NATIVE_INVALID_STATE;
    if (value->cancel_after_polls != UINT32_MAX &&
        value->cancel_polls >= value->cancel_after_polls)
        return ON1X_SDK_NATIVE_CANCELLED;
    if (value->cancel_polls != UINT32_MAX)
        ++value->cancel_polls;
    return ON1X_SDK_NATIVE_OK;
}

uint32_t basis_sdk_bridge_work_consume(uint64_t context, uint32_t units)
{
    NativeContext *value = NULL;
    On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (value->lifecycle != NATIVE_LIFECYCLE_ACTIVE)
        return ON1X_SDK_NATIVE_INVALID_STATE;
    if (units == 0u)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    if (units > value->work_limit - value->work_used)
        return ON1X_SDK_NATIVE_LIMIT;
    value->work_used += units;
    return ON1X_SDK_NATIVE_OK;
}

uint64_t basis_sdk_bridge_snapshot_open(
    uint64_t context,
    uint32_t source_schema,
    uint64_t maximum_bytes
)
{
    NativeContext *value = NULL;
    NativeSlot *slot = NULL;
    uint64_t handle = 0u;
    On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (value->lifecycle != NATIVE_LIFECYCLE_ACTIVE)
        return ON1X_SDK_NATIVE_INVALID_STATE;
    if (source_schema != value->source_schema)
        return ON1X_SDK_NATIVE_ABI_MISMATCH;
    if (maximum_bytes == 0u || maximum_bytes > ON1X_SDK_NATIVE_SOURCE_MAX)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    if (value->source_size > maximum_bytes)
        return ON1X_SDK_NATIVE_LIMIT;
    status = native_allocate(
        NATIVE_KIND_SNAPSHOT, context, &slot, &handle);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    slot->payload.snapshot.schema = source_schema;
    slot->payload.snapshot.revision = value->source_revision;
    slot->payload.snapshot.length = value->source_size;
    return handle;
}

uint32_t basis_sdk_bridge_snapshot_check(
    uint64_t context,
    uint64_t snapshot
)
{
    return native_owned(
        context, snapshot, NATIVE_KIND_SNAPSHOT, NULL, NULL);
}

uint32_t basis_sdk_bridge_snapshot_close(
    uint64_t context,
    uint64_t snapshot
)
{
    NativeSlot *slot = NULL;
    const On1xSDKNative_Status status = native_owned(
        context, snapshot, NATIVE_KIND_SNAPSHOT, NULL, &slot);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    native_release(slot);
    return ON1X_SDK_NATIVE_OK;
}

uint64_t basis_sdk_bridge_snapshot_revision(
    uint64_t context,
    uint64_t snapshot
)
{
    NativeSlot *slot = NULL;
    if (native_owned(context, snapshot, NATIVE_KIND_SNAPSHOT,
                     NULL, &slot) != ON1X_SDK_NATIVE_OK)
        return 0u;
    return slot->payload.snapshot.revision;
}

uint64_t basis_sdk_bridge_snapshot_length(
    uint64_t context,
    uint64_t snapshot
)
{
    NativeSlot *slot = NULL;
    if (native_owned(context, snapshot, NATIVE_KIND_SNAPSHOT,
                     NULL, &slot) != ON1X_SDK_NATIVE_OK)
        return 0u;
    return slot->payload.snapshot.length;
}

uint32_t basis_sdk_bridge_snapshot_schema(
    uint64_t context,
    uint64_t snapshot
)
{
    NativeSlot *slot = NULL;
    if (native_owned(context, snapshot, NATIVE_KIND_SNAPSHOT,
                     NULL, &slot) != ON1X_SDK_NATIVE_OK)
        return 0u;
    return slot->payload.snapshot.schema;
}

uint32_t basis_sdk_bridge_snapshot_read(
    uint64_t context,
    uint64_t snapshot,
    uint64_t revision,
    uint64_t index
)
{
    NativeContext *value = NULL;
    NativeSlot *slot = NULL;
    On1xSDKNative_Status status = native_owned(
        context, snapshot, NATIVE_KIND_SNAPSHOT, &value, &slot);
    if (status != ON1X_SDK_NATIVE_OK)
        return 256u + status;
    if (revision != slot->payload.snapshot.revision ||
        revision != value->source_revision)
        return 256u + ON1X_SDK_NATIVE_STALE;
    if (index >= slot->payload.snapshot.length)
        return 256u + ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    return value->source[(size_t)index];
}

uint32_t basis_sdk_bridge_diagnostic_emit(
    uint64_t context,
    uint64_t snapshot,
    uint64_t revision,
    uint32_t machine_code,
    uint32_t severity,
    uint32_t message,
    uint32_t message_length,
    uint64_t span_start,
    uint64_t span_end,
    uint32_t related_count,
    uint64_t related_start,
    uint64_t related_end
)
{
    static const char code_text[] = "ON1X-SDK-NATIVE-EXCLAMATION";
    static const char message_text[] = "discouraged exclamation byte";
    NativeContext *value = NULL;
    NativeSlot *slot = NULL;
    On1xSDKNative_Diagnostic *diagnostic;
    On1xSDKNative_Status status = native_owned(
        context, snapshot, NATIVE_KIND_SNAPSHOT, &value, &slot);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (value->lifecycle != NATIVE_LIFECYCLE_ACTIVE)
        return ON1X_SDK_NATIVE_INVALID_STATE;
    if ((value->granted_capabilities &
         ON1X_SDK_NATIVE_CAP_DIAGNOSTIC_EMIT) == 0u)
        return ON1X_SDK_NATIVE_CAPABILITY_DENIED;
    if (revision != slot->payload.snapshot.revision ||
        revision != value->source_revision)
        return ON1X_SDK_NATIVE_STALE;
    if (machine_code != ON1X_SDK_NATIVE_DIAGNOSTIC_CODE ||
        severity != ON1X_SDK_NATIVE_DIAGNOSTIC_SEVERITY_WARNING ||
        message != ON1X_SDK_NATIVE_DIAGNOSTIC_MESSAGE ||
        message_length != sizeof(message_text) - 1u)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    if (span_start > span_end || span_end > value->source_size)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    if (related_count > 1u)
        return ON1X_SDK_NATIVE_LIMIT;
    if (related_count == 1u &&
        (related_start > related_end || related_end > value->source_size))
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    if (value->diagnostic_count >= ON1X_SDK_NATIVE_DIAGNOSTIC_MAX)
        return ON1X_SDK_NATIVE_LIMIT;
    if (value->fail_diagnostic_after != UINT32_MAX &&
        value->diagnostic_count >= value->fail_diagnostic_after)
        return ON1X_SDK_NATIVE_IO;
    diagnostic = &value->diagnostics[value->diagnostic_count];
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->machine_code_id = machine_code;
    diagnostic->severity = severity;
    diagnostic->message_id = message;
    diagnostic->machine_code_length = (uint32_t)(sizeof(code_text) - 1u);
    diagnostic->message_length = message_length;
    memcpy(diagnostic->machine_code, code_text, sizeof(code_text) - 1u);
    memcpy(diagnostic->message, message_text, sizeof(message_text) - 1u);
    diagnostic->primary.revision = revision;
    diagnostic->primary.start = span_start;
    diagnostic->primary.end = span_end;
    diagnostic->related_count = related_count;
    if (related_count == 1u) {
        diagnostic->related[0].revision = revision;
        diagnostic->related[0].start = related_start;
        diagnostic->related[0].end = related_end;
    }
    ++value->diagnostic_count;
    return ON1X_SDK_NATIVE_OK;
}

uint64_t basis_sdk_bridge_candidate_open(
    uint64_t context,
    uint32_t candidate_schema,
    uint64_t source_revision,
    uint32_t maximum_bytes
)
{
    NativeContext *value = NULL;
    NativeSlot *slot = NULL;
    uint64_t handle = 0u;
    On1xSDKNative_Status status =
        native_context(context, NULL, &value);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    if (value->lifecycle != NATIVE_LIFECYCLE_ACTIVE)
        return ON1X_SDK_NATIVE_INVALID_STATE;
    if ((value->granted_capabilities &
         ON1X_SDK_NATIVE_CAP_CANDIDATE_SUBMIT) == 0u)
        return ON1X_SDK_NATIVE_CAPABILITY_DENIED;
    if (candidate_schema != value->candidate_schema)
        return ON1X_SDK_NATIVE_ABI_MISMATCH;
    if (source_revision != value->source_revision)
        return ON1X_SDK_NATIVE_STALE;
    if (maximum_bytes == 0u ||
        maximum_bytes > ON1X_SDK_NATIVE_CANDIDATE_MAX)
        return ON1X_SDK_NATIVE_INVALID_ARGUMENT;
    status = native_allocate(
        NATIVE_KIND_CANDIDATE, context, &slot, &handle);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    slot->payload.candidate.schema = candidate_schema;
    slot->payload.candidate.source_revision = source_revision;
    slot->payload.candidate.limit =
        maximum_bytes < value->candidate_limit ?
        maximum_bytes : value->candidate_limit;
    return handle;
}

uint32_t basis_sdk_bridge_candidate_append(
    uint64_t context,
    uint64_t candidate,
    uint8_t byte
)
{
    NativeSlot *slot = NULL;
    NativeCandidate *value;
    const On1xSDKNative_Status status = native_owned(
        context, candidate, NATIVE_KIND_CANDIDATE, NULL, &slot);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    value = &slot->payload.candidate;
    if (value->length >= value->limit)
        return ON1X_SDK_NATIVE_LIMIT;
    value->bytes[value->length++] = byte;
    return ON1X_SDK_NATIVE_OK;
}

uint32_t basis_sdk_bridge_candidate_commit(
    uint64_t context,
    uint64_t candidate,
    uint32_t candidate_schema,
    uint64_t source_revision
)
{
    NativeContext *owner = NULL;
    NativeSlot *slot = NULL;
    NativeCandidate *value;
    On1xSDKNative_Status status = native_owned(
        context, candidate, NATIVE_KIND_CANDIDATE, &owner, &slot);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    value = &slot->payload.candidate;
    if (candidate_schema != value->schema ||
        candidate_schema != owner->candidate_schema)
        return ON1X_SDK_NATIVE_ABI_MISMATCH;
    if (source_revision != value->source_revision ||
        source_revision != owner->source_revision)
        return ON1X_SDK_NATIVE_STALE;
    if (owner->published_size != 0u)
        return ON1X_SDK_NATIVE_CONFLICT;
    if (value->length != 6u ||
        value->bytes[0] != (uint8_t)'O' ||
        value->bytes[1] != (uint8_t)'N' ||
        value->bytes[2] != (uint8_t)'1' ||
        value->bytes[3] != (uint8_t)'X' ||
        value->bytes[4] != (uint8_t)ON1X_SDK_NATIVE_CANDIDATE_SCHEMA ||
        value->bytes[5] != (uint8_t)owner->diagnostic_count)
        return ON1X_SDK_NATIVE_VERIFICATION_FAILED;
    if (owner->fail_publish)
        return ON1X_SDK_NATIVE_IO;
    memcpy(owner->published, value->bytes, value->length);
    owner->published_size = value->length;
    owner->published_schema = candidate_schema;
    owner->published_revision = source_revision;
    native_release(slot);
    return ON1X_SDK_NATIVE_OK;
}

uint32_t basis_sdk_bridge_candidate_abort(
    uint64_t context,
    uint64_t candidate
)
{
    NativeSlot *slot = NULL;
    const On1xSDKNative_Status status = native_owned(
        context, candidate, NATIVE_KIND_CANDIDATE, NULL, &slot);
    if (status != ON1X_SDK_NATIVE_OK)
        return status;
    native_release(slot);
    return ON1X_SDK_NATIVE_OK;
}
