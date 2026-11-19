/* Bounded On1xfile manifest parsing for the supported project-tool subset. */
#ifndef ON1X_SDK_MANIFEST_H
#define ON1X_SDK_MANIFEST_H

#include "On1xSDK-Project.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MANIFEST_MAX_BYTES (64u * 1024u)
#define ON1X_SDK_MANIFEST_MAX_TARGETS 64u
#define ON1X_SDK_MANIFEST_MAX_DEPENDENCIES 32u

typedef struct On1xSDK_ProjectManifest On1xSDK_ProjectManifest;

enum {
    ON1X_SDK_PROJECT_EXECUTABLE = 1,
    ON1X_SDK_PROJECT_COMPLEX = 2,
    ON1X_SDK_PROJECT_LIBRARY = 3
};

enum {
    ON1X_SDK_DEPENDENCY_LOCAL = 1,
    ON1X_SDK_DEPENDENCY_GIT = 2,
    ON1X_SDK_DEPENDENCY_GIT_REMOTE = 3
};

enum {
    ON1X_SDK_SELECTOR_NONE = 0,
    ON1X_SDK_SELECTOR_COMMIT = 1,
    ON1X_SDK_SELECTOR_BRANCH = 2,
    ON1X_SDK_SELECTOR_TAG = 3
};

typedef struct On1xSDK_ManifestParseOptions {
    uint32_t abi_version;
    uint32_t struct_size;
    size_t max_bytes;        /* Zero selects ON1X_SDK_MANIFEST_MAX_BYTES. */
    size_t max_targets;      /* Zero selects ON1X_SDK_MANIFEST_MAX_TARGETS. */
    size_t max_dependencies; /* Zero selects ON1X_SDK_MANIFEST_MAX_DEPENDENCIES. */
} On1xSDK_ManifestParseOptions;

typedef struct On1xSDK_ManifestInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t kind;
    On1xSDK_ProjectString name;
    On1xSDK_ProjectString directory;
    unsigned char source_revision[ON1X_SDK_PROJECT_SHA256_BYTES];
} On1xSDK_ManifestInfo;

typedef struct On1xSDK_ManifestTarget {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_ProjectString name;
    On1xSDK_ProjectString source;
} On1xSDK_ManifestTarget;

typedef struct On1xSDK_ManifestDependency {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_ProjectString name;
    uint32_t kind;
    On1xSDK_ProjectString source;
    uint32_t selector_kind;
    On1xSDK_ProjectString selector;
} On1xSDK_ManifestDependency;

/*
 * Parses declarative On1xfile version 1 without executing templates, commands,
 * profiles, build programs, or extension code.  The supported reusable subset
 * is kind/name/directory/target/dependency.  Other directives return
 * UNSUPPORTED rather than being silently ignored.
 *
 * On success, one owned reference is returned.  All accessors return borrowed
 * immutable views valid until destroy.  On failure *out_manifest is NULL.  If
 * the optional sink rejects a diagnostic, its status is returned (unknown
 * numeric callback statuses become ERROR).
 */
ON1X_SDK_PROJECT_API On1xSDK_Status on1x_sdk_manifest_parse(
    On1xSDK_ProjectBytes input,
    const On1xSDK_ManifestParseOptions *options,
    const On1xSDK_ProjectAllocator *allocator,
    const On1xSDK_ProjectDiagnosticSink *sink,
    On1xSDK_ProjectManifest **out_manifest);

ON1X_SDK_PROJECT_API const On1xSDK_ManifestInfo *on1x_sdk_manifest_info(
    const On1xSDK_ProjectManifest *manifest);
ON1X_SDK_PROJECT_API size_t on1x_sdk_manifest_target_count(
    const On1xSDK_ProjectManifest *manifest);
ON1X_SDK_PROJECT_API const On1xSDK_ManifestTarget *on1x_sdk_manifest_target_at(
    const On1xSDK_ProjectManifest *manifest, size_t index);
ON1X_SDK_PROJECT_API size_t on1x_sdk_manifest_dependency_count(
    const On1xSDK_ProjectManifest *manifest);
ON1X_SDK_PROJECT_API const On1xSDK_ManifestDependency *
on1x_sdk_manifest_dependency_at(
    const On1xSDK_ProjectManifest *manifest, size_t index);
ON1X_SDK_PROJECT_API void on1x_sdk_manifest_destroy(
    On1xSDK_ProjectManifest *manifest);

#ifdef __cplusplus
}
#endif

#endif
