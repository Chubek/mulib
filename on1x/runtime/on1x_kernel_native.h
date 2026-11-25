#ifndef ON1X_KERNEL_NATIVE_H
#define ON1X_KERNEL_NATIVE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_KERNEL_NATIVE_IMAGE_LIMIT (4u * 1024u * 1024u)
#define ON1X_KERNEL_NATIVE_POLICY_LIMIT 4096u

struct on1x_kernel_native;

/* Verified canonical stdffi.clamp v1 package bindings, not caller imports.
 * The loader checks and binds all three exports, even if the app uses fewer. */
struct on1x_kernel_native_binding {
  const char *export_name;
  unsigned arity;
  const char *interface_sha256;
  const char *behavior_sha256;
};

/* SysV AMD64, status + result. Zero is success; nonzero is never a result. */
struct on1x_kernel_native_adapter {
  int (*call2)(int64_t, int64_t, int64_t *);
  int (*call3)(int64_t, int64_t, int64_t, int64_t *);
};

/* `policy` must come from explicit trusted host configuration, never from
 * the package or package search roots. An approval is not a sandbox or an
 * equivalence proof: the host vouches for the approved machine code.
 * Returns 0 on success, -1 on failure. Optional reason is a static string.
 * The returned owner retains the sealed image; input buffers are borrowed
 * only for this call. No provider code runs during successful open. */
int on1x_kernel_native_open(
    const void *image, size_t image_size, const void *policy, size_t policy_size,
    const struct on1x_kernel_native_binding *bindings, size_t binding_count,
    struct on1x_kernel_native **out, const char **reason);

/* On success exactly one pointer is set. Pointers are valid until close. */
int on1x_kernel_native_get(const struct on1x_kernel_native *owner,
    const char *export_name, unsigned arity,
    struct on1x_kernel_native_adapter *out);
void on1x_kernel_native_close(struct on1x_kernel_native *owner);

#ifdef __cplusplus
}
#endif
#endif
