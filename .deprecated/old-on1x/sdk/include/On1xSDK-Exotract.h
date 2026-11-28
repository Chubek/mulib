/*
 * Declarative Exotract/ExoLib build evidence.  Paths, switch names and profile
 * names are data only and are never resolved to executable code by this API.
 */
#ifndef ON1X_SDK_EXOTRACT_H
#define ON1X_SDK_EXOTRACT_H

#include "On1xSDK-FFI.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_EXOTRACT_SCHEMA_VERSION 2u
#define ON1X_SDK_EXOTRACT_MAX_SWITCHES 64u

enum {
  ON1X_SDK_EXOTRACT_DISCOVERY_COMPLETE = 1u << 0,
  ON1X_SDK_EXOTRACT_SYNTAX_CHECKED = 1u << 1,
  ON1X_SDK_EXOTRACT_DECLARATION_ONLY = 1u << 2,
  ON1X_SDK_EXOTRACT_DISCOVERY_ONLY = 1u << 3
};

typedef struct On1xSDK_ExotractProfile {
  uint32_t abi_version;
  uint32_t struct_size;
  uint32_t schema_version;
  uint32_t flags;
  On1xSDK_FFIText library;
  On1xSDK_FFIText manifest_sha256;
  On1xSDK_FFIText profile_sha256;
  const On1xSDK_FFIText *implementation_switches;
  size_t implementation_switch_count;
  uint32_t implementation_translation_units;
  uint32_t reserved;
} On1xSDK_ExotractProfile;

On1xSDK_Status
on1x_sdk_exotract_profile_check(const On1xSDK_ExotractProfile *profile);
On1xSDK_Status
on1x_sdk_exotract_switch_at(const On1xSDK_ExotractProfile *profile,
                            size_t index,
                            On1xSDK_FFIText *implementation_switch);

#ifdef __cplusplus
}
#endif

#endif
