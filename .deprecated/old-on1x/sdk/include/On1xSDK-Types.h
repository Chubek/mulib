/* Common scalar types shared by the ABI-v1 compatibility surface and ABI 2. */

#ifndef ON1X_SDK_TYPES_H
#define ON1X_SDK_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ON1X_SDK_ABI_VERSION remains 1 for source compatibility with the original
 * descriptor headers. New ABI-2 descriptors use ON1X_SDK_ABI_VERSION_V2 (or
 * ON1X_SDK_ABI_CURRENT_VERSION) and live in their own headers/layouts.
 */
#define ON1X_SDK_ABI_VERSION 1u
#define ON1X_SDK_ABI_VERSION_V1 1u
#define ON1X_SDK_ABI_VERSION_V2 2u
#define ON1X_SDK_ABI_CURRENT_VERSION ON1X_SDK_ABI_VERSION_V2
#define ON1X_SDK_ABI_VERSION_CURRENT ON1X_SDK_ABI_VERSION_V2
#define ON1X_SDK_ABI_CURRENT ON1X_SDK_ABI_VERSION_V2
#define ON1X_SDK_ABI_V1 ON1X_SDK_ABI_VERSION_V1
#define ON1X_SDK_ABI_V2 ON1X_SDK_ABI_VERSION_V2
/* Maximum byte length of a registration name, excluding the NUL. */
#define ON1X_SDK_NAME_MAX 128u

typedef struct On1xSDK_Host On1xSDK_Host;
typedef struct On1xSDK_Registry On1xSDK_Registry;
typedef struct On1xSDK_Program On1xSDK_Program;
typedef struct On1xSDK_Context On1xSDK_Context;
typedef struct On1xSDK_Cancellation On1xSDK_Cancellation;
typedef struct On1xSDK_Budget On1xSDK_Budget;
typedef struct On1xSDK_CapabilitySet On1xSDK_CapabilitySet;
typedef struct On1xSDK_RegistrySnapshot On1xSDK_RegistrySnapshot;
typedef struct On1xSDK_Catalogue On1xSDK_Catalogue;
typedef struct On1xSDK_CataloguePlan On1xSDK_CataloguePlan;
typedef struct On1xSDK_CatalogueEntryView On1xSDK_CatalogueEntryView;
typedef struct On1xSDK_PluginManifestOwned On1xSDK_PluginManifestOwned;
typedef struct On1xSDK_PluginAuthorization On1xSDK_PluginAuthorization;
typedef struct On1xSDK_PluginDiscoveryResult On1xSDK_PluginDiscoveryResult;
typedef struct On1xSDK_ProtocolSession On1xSDK_ProtocolSession;
typedef struct On1xSDK_Transport On1xSDK_Transport;
typedef struct On1xSDK_SharedMemory On1xSDK_SharedMemory;

/* Fix the representation in C++ so an unknown numeric callback status can
 * be checked without loading an out-of-range C++ enum value. */
#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_OK = 0,
    ON1X_SDK_ERROR = 1,
    ON1X_SDK_UNSUPPORTED = 2,
    ON1X_SDK_LIMIT = 3,
    ON1X_SDK_INVALID_ARGUMENT = 4,
    ON1X_SDK_ABI_MISMATCH = 5,
    ON1X_SDK_DUPLICATE = 6,
    ON1X_SDK_BUSY = 7,

    /* ABI 2 additions. Values 0..7 above are frozen ABI-v1 values. */
    ON1X_SDK_CANCELLED = 8,
    ON1X_SDK_DEADLINE_EXCEEDED = 9,
    ON1X_SDK_STALE = 10,
    ON1X_SDK_CONFLICT = 11,
    ON1X_SDK_NOT_FOUND = 12,
    ON1X_SDK_INVALID_STATE = 13,
    ON1X_SDK_VERIFICATION_FAILED = 14,
    ON1X_SDK_CAPABILITY_DENIED = 15,
    ON1X_SDK_IO_ERROR = 16,
    ON1X_SDK_PROTOCOL_ERROR = 17,
    ON1X_SDK_CALLBACK_FAILED = 18,
    ON1X_SDK_UNSATISFIABLE = 19
} On1xSDK_Status;

#ifdef __cplusplus
typedef enum : uint32_t {
#else
typedef enum {
#endif
    ON1X_SDK_SOURCE = 1,
    ON1X_SDK_CHECKED_HIR = 2,
    ON1X_SDK_VERIFIED_MIR = 3
} On1xSDK_Phase;

#ifndef __cplusplus
_Static_assert(sizeof(On1xSDK_Status) == sizeof(uint32_t), "SDK requires 32-bit enums");
_Static_assert(sizeof(On1xSDK_Phase) == sizeof(uint32_t), "SDK requires 32-bit enums");
#endif

#ifdef __cplusplus
}
#endif

#endif
