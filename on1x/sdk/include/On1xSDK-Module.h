/* A catalog of named module metadata, independent of source import loading. */
#ifndef ON1X_SDK_MODULE_H
#define ON1X_SDK_MODULE_H

#include "On1xSDK-Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_MODULE_IMPORTS 64u
#define ON1X_SDK_MAX_MODULE_SUMMARY_BYTES 1024u

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;
    const char *revision;       /* Nonempty printable ASCII, at most ON1X_SDK_NAME_MAX bytes. */
    const char *summary;        /* Optional nonempty printable ASCII. */
    size_t import_count;
    const char *const *imports; /* NULL iff import_count is zero. */
} On1xSDK_Module;

/*
 * Deep-copies the descriptor and its direct imports in declaration order.
 * Module/import names are dot-separated ASCII identifiers, each matching
 * [A-Za-z_][A-Za-z0-9_]*, at most ON1X_SDK_NAME_MAX bytes total per name.
 * Self-imports and duplicate imports are rejected. Imports are names, not
 * paths; they need not be registered. No resolution, cycle checking, loading
 * or compiler import override occurs here.
 */
On1xSDK_Status on1x_sdk_module_register(On1xSDK_Registry *registry,
                                       const On1xSDK_Module *module);
const On1xSDK_Module *on1x_sdk_module_find(const On1xSDK_Registry *registry,
                                          const char *name);
size_t on1x_sdk_module_count(const On1xSDK_Registry *registry);
const On1xSDK_Module *on1x_sdk_module_at(const On1xSDK_Registry *registry, size_t index);

#ifdef __cplusplus
}
#endif

#endif
