#include "On1xSDK-Type.h"

#include <stdint.h>
#include <string.h>

_Static_assert(ON1X_SDK_OK == 0, "ABI-v1 status changed");
_Static_assert(ON1X_SDK_ERROR == 1, "ABI-v1 status changed");
_Static_assert(ON1X_SDK_UNSUPPORTED == 2, "ABI-v1 status changed");
_Static_assert(ON1X_SDK_LIMIT == 3, "ABI-v1 status changed");
_Static_assert(ON1X_SDK_INVALID_ARGUMENT == 4, "ABI-v1 status changed");
_Static_assert(ON1X_SDK_ABI_MISMATCH == 5, "ABI-v1 status changed");
_Static_assert(ON1X_SDK_DUPLICATE == 6, "ABI-v1 status changed");
_Static_assert(ON1X_SDK_BUSY == 7, "ABI-v1 status changed");

static int owner_token;
static int other_token;

static On1xSDK_IRIdentity identity(const On1xSDK_Context *owner)
{
    On1xSDK_IRIdentity value;
    value.abi_version = ON1X_SDK_SEMANTICS_IR_ABI_VERSION;
    value.struct_size = sizeof(value);
    value.owner = owner;
    value.generation = 3;
    value.revision = 5;
    value.state = ON1X_SDK_IR_CANDIDATE;
    value.schema_version = ON1X_SDK_SEMANTICS_IR_SCHEMA_VERSION;
    return value;
}

int main(void)
{
    const On1xSDK_Context *owner =
        (const On1xSDK_Context *)(const void *)&owner_token;
    const On1xSDK_Context *other =
        (const On1xSDK_Context *)(const void *)&other_token;
    On1xSDK_IRIdentity id = identity(owner);
    On1xSDK_IRIdentity other_id = identity(other);
    On1xSDK_Type *i64 = 0;
    On1xSDK_Type *boolean = 0;
    On1xSDK_Type *other_i64 = 0;
    On1xSDK_Type *tuple = 0;
    On1xSDK_Type *bad = (On1xSDK_Type *)(uintptr_t)1;
    const On1xSDK_Type *elements[2];
    const On1xSDK_Type *mixed[2];
    On1xSDK_TypeKind kind = ON1X_SDK_TYPE_INVALID;
    On1xSDK_TypeFlags flags = 0;
    size_t required = 0;
    char spelling[32];

    if (on1x_sdk_type_create_atomic(
            &id, ON1X_SDK_TYPE_I64, 0, &i64) != ON1X_SDK_OK ||
        on1x_sdk_type_create_atomic(
            &id, ON1X_SDK_TYPE_BOOL, 0, &boolean) != ON1X_SDK_OK ||
        on1x_sdk_type_create_atomic(
            &other_id, ON1X_SDK_TYPE_I64, 0, &other_i64) != ON1X_SDK_OK)
        return 1;
    elements[0] = i64;
    elements[1] = boolean;
    if (on1x_sdk_type_create_tuple(
            &id, elements, 2, &tuple) != ON1X_SDK_OK)
        return 2;
    if (on1x_sdk_type_kind(tuple, &kind) != ON1X_SDK_OK ||
        kind != ON1X_SDK_TYPE_TUPLE ||
        on1x_sdk_type_flags(tuple, &flags) != ON1X_SDK_OK ||
        (flags & ON1X_SDK_TYPE_FLAG_MOVE_ONLY) != 0)
        return 3;
    if (on1x_sdk_type_spelling(
            tuple, spelling, sizeof(spelling), &required) != ON1X_SDK_OK ||
        required != strlen("(i64, bool)") ||
        memcmp(spelling, "(i64, bool)", required) != 0)
        return 4;

    mixed[0] = i64;
    mixed[1] = other_i64;
    if (on1x_sdk_type_create_tuple(&id, mixed, 2, &bad) !=
            ON1X_SDK_BUSY ||
        bad != 0)
        return 5;

    id.abi_version = 99;
    if (on1x_sdk_ir_identity_validate(
            &id, ON1X_SDK_IR_CANDIDATE) != ON1X_SDK_ABI_MISMATCH)
        return 6;

    on1x_sdk_type_release(tuple);
    on1x_sdk_type_release(other_i64);
    on1x_sdk_type_release(boolean);
    on1x_sdk_type_release(i64);
    return 0;
}
