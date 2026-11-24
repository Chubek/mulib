#ifndef ON1X_KERNEL_RT_H
#define ON1X_KERNEL_RT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional native C11 kernel runtime. This is independent of On1xRT engine ABI 1. */
#define ON1X_KERNEL_RT_ABI_VERSION 1u
#define ON1X_KERNEL_RT_MAX_I64_ARGS 3u

typedef struct On1xKernelRT_Module On1xKernelRT_Module;
typedef uint32_t On1xKernelRT_Status;
enum {
  ON1X_KERNEL_RT_OK = 0,
  ON1X_KERNEL_RT_INVALID_ARGUMENT = 1,
  ON1X_KERNEL_RT_UNSUPPORTED = 2,
  ON1X_KERNEL_RT_OUT_OF_MEMORY = 3,
  ON1X_KERNEL_RT_CONFIG_ERROR = 4,
  ON1X_KERNEL_RT_COMPILE_ERROR = 5,
  ON1X_KERNEL_RT_IMAGE_ERROR = 6,
  ON1X_KERNEL_RT_LOAD_ERROR = 7,
  ON1X_KERNEL_RT_SYMBOL_ERROR = 8,
  ON1X_KERNEL_RT_SIGNATURE_MISMATCH = 9,
  ON1X_KERNEL_RT_INVALID_INTERVAL = 10
};

typedef uint32_t On1xKernelRT_ModuleId;
typedef uint32_t On1xKernelRT_Operation;
enum { ON1X_KERNEL_RT_MULIB_CLAMP = 1 };
enum {
  ON1X_KERNEL_RT_CLAMP_I64 = 1,
  ON1X_KERNEL_RT_ADD_I64 = 2,
  ON1X_KERNEL_RT_SUB_I64 = 3
};

uint32_t on1x_kernel_rt_abi_version(void);
const char *on1x_kernel_rt_status_message(On1xKernelRT_Status status);

/* Only build-pinned modules are accepted. No guest C source, loader path or
 * symbol string is accepted. Open requires a NULL-initialized slot and leaves
 * it unchanged on failure. Close consumes and clears the slot; NULL is safe.
 * The caller must quiesce all calls and not use aliases after close. */
On1xKernelRT_Status on1x_kernel_rt_open(On1xKernelRT_ModuleId id,
                                        On1xKernelRT_Module **module);
void on1x_kernel_rt_close(On1xKernelRT_Module **module);

/* Exact by-value i64 signatures; no native address escapes. Calls may overlap
 * for the current immutable clamp module. Writes result only on success. */
On1xKernelRT_Status on1x_kernel_rt_call_i64(const On1xKernelRT_Module *module,
                                            On1xKernelRT_Operation operation,
                                            const int64_t *args, size_t count,
                                            int64_t *result);

/* Generated-program convenience. Lazily initializes each qualifying builtin
 * once, and keeps it mapped until process exit. Initialization errors are stable. */
On1xKernelRT_Status on1x_kernel_rt_builtin_i64(On1xKernelRT_Operation operation,
                                               const int64_t *args, size_t count,
                                               int64_t *result);

#ifdef __cplusplus
}
#endif
#endif
