#ifndef ON1X_KERNEL_PACKAGE_H
#define ON1X_KERNEL_PACKAGE_H

#include <stddef.h>
#include <stdint.h>
#include "on1x_kernel_import.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_KERNEL_PACKAGE_FORMAT 1u
#define ON1X_KERNEL_IMPORT_VERSION 1u
#define ON1X_KERNEL_PACKAGE_SUFFIX ".on1xkp"
#define ON1X_KERNEL_PACKAGE_NAME_MAX 63u
#define ON1X_KERNEL_PACKAGE_EXPORT_MAX 16u
#define ON1X_KERNEL_PACKAGE_DEP_MAX 16u
#define ON1X_KERNEL_PACKAGE_CONFLICT_MAX 16u
#define ON1X_KERNEL_PACKAGE_RECIPE_MAX (64u * 1024u)
#define ON1X_KERNEL_PACKAGE_IMAGE_MAX (4u * 1024u * 1024u)
#define ON1X_KERNEL_PACKAGE_SIZE_MAX                                       \
  (8192u + ON1X_KERNEL_PACKAGE_RECIPE_MAX + ON1X_KERNEL_PACKAGE_IMAGE_MAX)
#define ON1X_KERNEL_SHA256_HEX_LEN 64u

enum on1x_kernel_package_kind {
  ON1X_KERNEL_PACKAGE_RECIPE = 1,
  ON1X_KERNEL_PACKAGE_NATIVE_AMD64_LINUX_V1 = 2
};

struct on1x_kernel_package_export {
  char name[ON1X_KERNEL_PACKAGE_NAME_MAX + 1u];
  unsigned arity;
  char interface_sha256[ON1X_KERNEL_SHA256_HEX_LEN + 1u];
  char behavior_sha256[ON1X_KERNEL_SHA256_HEX_LEN + 1u];
};

struct on1x_kernel_package_dep {
  char module[ON1X_KERNEL_PACKAGE_NAME_MAX + 1u];
  uint32_t version;
};

struct on1x_kernel_package_conflict {
  char module[ON1X_KERNEL_PACKAGE_NAME_MAX + 1u];
};

struct on1x_kernel_package {
  char module[ON1X_KERNEL_PACKAGE_NAME_MAX + 1u];
  uint32_t version;
  enum on1x_kernel_package_kind kind;
  size_t export_count, dep_count, conflict_count;
  struct on1x_kernel_package_export exports[ON1X_KERNEL_PACKAGE_EXPORT_MAX];
  struct on1x_kernel_package_dep deps[ON1X_KERNEL_PACKAGE_DEP_MAX];
  struct on1x_kernel_package_conflict conflicts[ON1X_KERNEL_PACKAGE_CONFLICT_MAX];
  const unsigned char *recipe, *image;
  size_t recipe_size, image_size;
  /* Encoder derives this field from image bytes; parser returns it.
   * Recipe-only manifests have no image digest (empty string). */
  char image_sha256[ON1X_KERNEL_SHA256_HEX_LEN + 1u];
};

/* Return 1 on success, 0 on failure. The input and *out must not overlap.
 * Parse leaves *out unchanged on failure and borrows recipe/image from data
 * on success. Structural validation and image integrity do not authorize a
 * payload or check recipe semantics. */
int on1x_kernel_package_parse(const void *data, size_t size,
                             struct on1x_kernel_package *out);
/* Pass data=NULL and capacity=0 to measure. All pointers must be disjoint
 * from the output byte buffer. On failure no output is written. */
int on1x_kernel_package_encode(const struct on1x_kernel_package *package,
                              void *data, size_t capacity, size_t *written);
/* Strict ASCII identifiers, and v1 signed i64 interface hash. */
int on1x_kernel_package_module_valid(const char *module);
int on1x_kernel_import_valid(const struct on1x_kernel_import *record);
int on1x_kernel_hash_interface(const char *module, const char *export_name,
                              unsigned arity,
                              char out[ON1X_KERNEL_SHA256_HEX_LEN + 1u]);

#ifdef __cplusplus
}
#endif
#endif
