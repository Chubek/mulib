/* On1x SDK C ABI, version 1. */

#ifndef ON1X_SDK_TYPES_H
#define ON1X_SDK_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_ABI_VERSION 1u
/* Maximum byte length of a registration name, excluding the NUL. */
#define ON1X_SDK_NAME_MAX 128u

typedef struct On1xSDK_Host On1xSDK_Host;
typedef struct On1xSDK_Registry On1xSDK_Registry;
typedef struct On1xSDK_Program On1xSDK_Program;
typedef struct On1xSDK_Context On1xSDK_Context;

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
    ON1X_SDK_BUSY = 7
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
