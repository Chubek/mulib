/* Inspectable foreign-contract annotations, never a binding or admission. */
#ifndef ON1X_SDK_CONTRACT_H
#define ON1X_SDK_CONTRACT_H

#include "On1xSDK-Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_CONTRACT_TEXT_BYTES 1024u

enum {
    ON1X_SDK_CONTRACT_EFFECT_UNKNOWN = 1u << 0,
    ON1X_SDK_CONTRACT_EFFECT_ALLOCATE = 1u << 1,
    ON1X_SDK_CONTRACT_EFFECT_TRAP = 1u << 2,
    ON1X_SDK_CONTRACT_EFFECT_BLOCK = 1u << 3,
    ON1X_SDK_CONTRACT_EFFECT_IO = 1u << 4,
    ON1X_SDK_CONTRACT_EFFECT_MUTATE = 1u << 5,
    ON1X_SDK_CONTRACT_EFFECT_CALLBACK = 1u << 6,
    ON1X_SDK_CONTRACT_EFFECT_NONDETERMINISTIC = 1u << 7
};

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;
    const char *signature;       /* Required; opaque type description, e.g. "i64 -> i64". */
    const char *ownership;       /* Optional, e.g. "consumes argument 0". */
    const char *borrowing;       /* Optional, e.g. "read-only for call duration". */
    uint32_t effects;            /* Known bits or UNKNOWN by itself; 0 claims no listed effects. */
    const char *target;          /* Optional; target label as in On1xSDK-Toolchain.h. */
    const char *abi_description; /* Required when target is present; opaque layout/convention. */
    const char *failure;         /* Optional, e.g. "checked status". */
    const char *evidence_sha256;  /* Optional, exactly 64 lowercase hexadecimal digits. */
} On1xSDK_Contract;

/*
 * Copies the descriptor under the registry's ABI/lifetime rules. Text fields
 * are nonempty printable ASCII (at most ON1X_SDK_MAX_CONTRACT_TEXT_BYTES);
 * absent optional fields are NULL. target and abi_description must be paired.
 * Registration checks metadata shape, not types, ABI, evidence or review.
 * There is no callable symbol, safety flag, or safe/raw admission operation.
 */
On1xSDK_Status on1x_sdk_contract_register(On1xSDK_Registry *registry,
                                         const On1xSDK_Contract *contract);
const On1xSDK_Contract *on1x_sdk_contract_find(const On1xSDK_Registry *registry,
                                             const char *name);
size_t on1x_sdk_contract_count(const On1xSDK_Registry *registry);
const On1xSDK_Contract *on1x_sdk_contract_at(const On1xSDK_Registry *registry, size_t index);

#ifdef __cplusplus
}
#endif

#endif
