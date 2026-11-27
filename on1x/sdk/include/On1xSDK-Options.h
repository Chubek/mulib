#ifndef ON1X_SDK_OPTIONS_H
#define ON1X_SDK_OPTIONS_H

#include "On1xSDK-Compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_Option;
enum {
    ON1X_SDK_OPTION_MAX_SOURCE_BYTES = 1,
    ON1X_SDK_OPTION_MAX_TOTAL_BYTES = 2,
    ON1X_SDK_OPTION_MAX_TOKENS = 3,
    ON1X_SDK_OPTION_MAX_AST_NODES = 4,
    ON1X_SDK_OPTION_MAX_MODULES = 5,
    ON1X_SDK_OPTION_MAX_IMPORT_DEPTH = 6,
    ON1X_SDK_OPTION_MAX_DIAGNOSTICS = 7,
    ON1X_SDK_OPTION_WORK_BUDGET = 8,
    ON1X_SDK_OPTION_DEADLINE_MILLISECONDS = 9,
    ON1X_SDK_OPTION_MAX_OVERLAYS = 10
};

typedef uint32_t On1xSDK_BooleanOption;
enum {
    ON1X_SDK_OPTION_PRELUDE = 1,
    ON1X_SDK_OPTION_EXPAND_MACROS = 2,
    ON1X_SDK_OPTION_STRICT_UTF8 = 3,
    /*
     * Opt in after adding the standard library package as the first search
     * path. The canonical loader uses that path as an explicit stdlib root
     * (with an On1xfile), not as an ordinary library path. Defaults to off.
     * Clearing search paths also clears this selection.
     */
    ON1X_SDK_OPTION_FIRST_SEARCH_PATH_IS_STDLIB = 4,
    ON1X_SDK_OPTION_CAPTURE_MODULE_GRAPH = 5
};

On1xSDK_Status on1x_sdk_options_create(
    On1xSDK_Session *session,
    On1xSDK_Options **out_options);
On1xSDK_Status on1x_sdk_options_clone(
    const On1xSDK_Options *options,
    On1xSDK_Options **out_options);
On1xSDK_Status on1x_sdk_options_retain(On1xSDK_Options *options);
void on1x_sdk_options_release(On1xSDK_Options *options);

On1xSDK_Status on1x_sdk_options_set_limit(
    On1xSDK_Options *options,
    On1xSDK_Option option,
    uint64_t value);
On1xSDK_Status on1x_sdk_options_get_limit(
    const On1xSDK_Options *options,
    On1xSDK_Option option,
    uint64_t *out_value);
On1xSDK_Status on1x_sdk_options_set_boolean(
    On1xSDK_Options *options,
    On1xSDK_BooleanOption option,
    uint32_t value);
On1xSDK_Status on1x_sdk_options_get_boolean(
    const On1xSDK_Options *options,
    On1xSDK_BooleanOption option,
    uint32_t *out_value);

/* Paths and feature names are UTF-8 and copied. */
On1xSDK_Status on1x_sdk_options_add_search_path(
    On1xSDK_Options *options,
    const char *path,
    size_t path_size);
On1xSDK_Status on1x_sdk_options_enable_feature(
    On1xSDK_Options *options,
    const char *name,
    size_t name_size);
On1xSDK_Status on1x_sdk_options_clear_search_paths(On1xSDK_Options *options);
On1xSDK_Status on1x_sdk_options_clear_features(On1xSDK_Options *options);

uint64_t on1x_sdk_options_revision(const On1xSDK_Options *options);

#ifdef __cplusplus
}
#endif

#endif
