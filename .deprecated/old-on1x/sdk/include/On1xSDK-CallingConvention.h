/* Calling-convention identities used by validated target descriptions. */
#ifndef ON1X_SDK_CALLING_CONVENTION_H
#define ON1X_SDK_CALLING_CONVENTION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_CallingConvention;
enum {
    ON1X_SDK_CALLING_CONVENTION_C = 1u,
    ON1X_SDK_CALLING_CONVENTION_SYSV_AMD64 = 2u,
    ON1X_SDK_CALLING_CONVENTION_AAPCS64 = 3u,
    ON1X_SDK_CALLING_CONVENTION_RISCV_LP64 = 4u,
    ON1X_SDK_CALLING_CONVENTION_WASM = 5u
};

#ifdef __cplusplus
}
#endif

#endif
