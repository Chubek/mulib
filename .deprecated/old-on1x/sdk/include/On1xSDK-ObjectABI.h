/* Object-format identities used by validated target descriptions. */
#ifndef ON1X_SDK_OBJECT_ABI_H
#define ON1X_SDK_OBJECT_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_ObjectFormat;
enum {
    ON1X_SDK_OBJECT_FORMAT_NONE = 1u,
    ON1X_SDK_OBJECT_FORMAT_ELF = 2u,
    ON1X_SDK_OBJECT_FORMAT_MACH_O = 3u,
    ON1X_SDK_OBJECT_FORMAT_COFF = 4u,
    ON1X_SDK_OBJECT_FORMAT_WASM = 5u
};

#ifdef __cplusplus
}
#endif

#endif
