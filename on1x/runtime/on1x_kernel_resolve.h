#ifndef ON1X_KERNEL_RESOLVE_H
#define ON1X_KERNEL_RESOLVE_H

#include <stddef.h>

struct on1x_kernel_import;
struct on1x_kernel_package;
struct on1x_kernel_resolution;

#define ON1X_KERNEL_RESOLVE_MAX_ROOTS 16u
#define ON1X_KERNEL_RESOLVE_MAX_PACKAGES 64u
#define ON1X_KERNEL_RESOLVE_MAX_IMPORTS 256u
#define ON1X_KERNEL_RESOLVE_MAX_NAME 63u
#define ON1X_KERNEL_RESOLVE_CHAIN_SIZE \
    ((ON1X_KERNEL_RESOLVE_MAX_PACKAGES + 1u) * (ON1X_KERNEL_RESOLVE_MAX_NAME + 4u) + 1u)

struct on1x_kernel_resolve_diag {
    char module[ON1X_KERNEL_RESOLVE_MAX_NAME + 1u];
    char export_name[ON1X_KERNEL_RESOLVE_MAX_NAME + 1u];
    char reason[256];
    char chain[ON1X_KERNEL_RESOLVE_CHAIN_SIZE];
};

/* Resolve only reachable packages and match application imports to their declared
 * metadata. The loader must still independently admit/check the recipe and payload
 * of each selected package before use. No dispatch state is changed here.
 *
 * root_list is the explicit, colon-separated host directory list (at most 4096
 * bytes). Relative paths resolve against the caller's working directory; no
 * implicit roots are used. Empty components and unavailable directories fail.
 * For each canonical module the only candidate is <module>.on1xkp in each root.
 * Files must be regular and not symlinks; even byte-identical duplicates fail.
 * At most 32 MiB of package bytes are retained while preparing the snapshot.
 *
 * Returns zero on success (empty imports require no roots and return no snapshot).
 * Nonzero returns *out == NULL and optionally populates diag. Initialize *out
 * separately if it already owns a snapshot; this operation never frees an old
 * snapshot passed through that location.
 */
int on1x_kernel_resolve(const char *root_list,
                        const struct on1x_kernel_import *required, size_t count,
                        struct on1x_kernel_resolution **out,
                        struct on1x_kernel_resolve_diag *diag);

/* Views are borrowed until free. at() enumerates in dependency-first order. */
size_t on1x_kernel_resolution_count(const struct on1x_kernel_resolution *resolution);
const struct on1x_kernel_package *on1x_kernel_resolution_at(
    const struct on1x_kernel_resolution *resolution, size_t ordered_index);
const struct on1x_kernel_package *on1x_kernel_resolution_find(
    const struct on1x_kernel_resolution *resolution, const char *module);
const void *on1x_kernel_resolution_bytes(
    const struct on1x_kernel_resolution *resolution, size_t ordered_index, size_t *size);

/* Copy the first discovery chain for a selected package, useful for late
 * recipe/payload admission errors. Returns zero if the module is not present. */
int on1x_kernel_resolution_chain(const struct on1x_kernel_resolution *resolution,
                                  const char *module,
                                  char chain[ON1X_KERNEL_RESOLVE_CHAIN_SIZE]);
void on1x_kernel_resolution_free(struct on1x_kernel_resolution *resolution);

#endif
