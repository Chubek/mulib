/* Explicit registration of trusted, linked SDK extensions. */

#ifndef ON1X_SDK_REGISTRY_H
#define ON1X_SDK_REGISTRY_H

#include "On1xSDK-Types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef On1xSDK_Status (*On1xSDK_RegisterFn)(
    const On1xSDK_Host *host,
    On1xSDK_Registry *registry
);

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;

    const char *name;
    const char *version;

    On1xSDK_RegisterFn register_all;
} On1xSDK_Extension;

/*
 * Registries are thread-confined, including reads and all callback activity
 * on a registry. All input pointers must designate valid readable storage
 * during their call. A string must be readable through its terminating NUL
 * or the documented scan bound if no NUL is found. The SDK copies descriptor
 * data and its strings, not callback code or what userdata points to.
 * Committed descriptor views are read-only and live until registry
 * destruction; callback code and any referenced userdata must outlive
 * their registration.
 *
 * On failure, create clears *out if out is non-NULL.
 */
On1xSDK_Status on1x_sdk_registry_create(On1xSDK_Registry **out);
/*
 * Do not destroy a registry while an SDK operation is using it, including
 * from within a callback. NULL and requests during register_all are no-ops.
 */
void on1x_sdk_registry_destroy(On1xSDK_Registry *registry);

/*
 * Use abi_version = ON1X_SDK_ABI_VERSION and struct_size = sizeof descriptor.
 * At least the two uint32_t header fields must be readable. Unknown trailing
 * fields for the same ABI are ignored. A short structure or a different ABI
 * returns ABI_MISMATCH. Extension names match [A-Za-z_][A-Za-z0-9_.-]*;
 * version is nonempty printable ASCII. Both must be NUL-terminated within
 * ON1X_SDK_NAME_MAX + 1 bytes or registration returns LIMIT. Malformed or
 * missing fields return INVALID_ARGUMENT. A committed extension name is unique
 * regardless of version. Typed registration names are unique within a kind.
 * Duplicates return DUPLICATE. The registry holds up to 1024 extensions and
 * up to 1024 descriptors per kind; exhaustion returns LIMIT.
 *
 * register_all runs synchronously with an opaque host token borrowed for
 * the call. Typed adds are allowed outside the callback, too. Inside it,
 * queries see committed entries and prior successful adds in insertion order;
 * treat pending descriptor views as borrowed for the callback only, and
 * reacquire them after successful registration. A non-OK return rolls back
 * all of the callback's adds to this registry and is returned unchanged if
 * it is a known status. A C++ exception or unrecognized status returns ERROR
 * (LIMIT for std::bad_alloc).
 * Propagate failed adds from the callback to roll back earlier successful ones.
 * Nested extension registration on this registry returns BUSY. Rollback
 * covers this registry only; it cannot undo callback side effects. Do not
 * bypass the callback return or C++ unwinding (e.g. by using longjmp).
 *
 * Call only trusted, explicitly linked extension code. This API does not
 * discover, dynamically load, or sandbox extensions.
 */
On1xSDK_Status on1x_sdk_registry_register(On1xSDK_Registry *registry,
                                       const On1xSDK_Extension *extension);

#ifdef __cplusplus
}
#endif

#endif
