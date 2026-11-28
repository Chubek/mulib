/* Offline-only package and lock resolution over parsed project manifests. */
#ifndef ON1X_SDK_PACKAGE_RESOLVER_H
#define ON1X_SDK_PACKAGE_RESOLVER_H

#include "On1xSDK-BuildGraph.h"
#include "On1xSDK-Manifest.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_PackageCandidate {
    uint32_t abi_version;
    uint32_t struct_size;
    const On1xSDK_ProjectManifest *manifest; /* Borrowed for the call. */
    On1xSDK_ProjectString location;          /* Canonical root-relative path. */
    On1xSDK_ProjectString revision;          /* Git commit for Git candidates; empty for local. */
    On1xSDK_ProjectString content_sha256;    /* Exact candidate snapshot digest. */
} On1xSDK_PackageCandidate;

typedef struct On1xSDK_PackageLockEntry {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_ProjectString name;
    uint32_t kind;
    On1xSDK_ProjectString source;
    uint32_t selector_kind;
    On1xSDK_ProjectString selector;
    On1xSDK_ProjectString commit;
} On1xSDK_PackageLockEntry;

typedef struct On1xSDK_PackageResolveOptions {
    uint32_t abi_version;
    uint32_t struct_size;
    int offline; /* Must be nonzero.  This API performs no network access. */
    int require_lock;
    size_t max_packages; /* Zero selects 32. */
} On1xSDK_PackageResolveOptions;

/*
 * Resolves the root and supplied local/cache candidates without opening paths
 * or running code.  Local dependency source paths must exactly match candidate
 * locations.  Branch/tag and require_lock modes require matching, current lock
 * entries.  Git-remote cache misses return UNSUPPORTED; no network fallback
 * exists.  The resulting graph is copied into allocator's domain.
 */
ON1X_SDK_PROJECT_API On1xSDK_Status on1x_sdk_package_resolve_offline(
    const On1xSDK_ProjectManifest *root,
    On1xSDK_ProjectString root_content_sha256,
    const On1xSDK_PackageCandidate *candidates, size_t candidate_count,
    const On1xSDK_PackageLockEntry *locks, size_t lock_count,
    const On1xSDK_PackageResolveOptions *options,
    const On1xSDK_ProjectAllocator *allocator,
    const On1xSDK_ProjectDiagnosticSink *sink,
    On1xSDK_BuildGraph **out_graph);

#ifdef __cplusplus
}
#endif

#endif
