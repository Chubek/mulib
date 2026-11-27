#define _GNU_SOURCE
#include "On1xKernelRT.h"

#include <cephyr/Cephyr.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "on1x_kernel_rt_embedded.inc"

typedef int64_t (*Binary)(int64_t, int64_t);
typedef int64_t (*Ternary)(int64_t, int64_t, int64_t);

struct On1xKernelRT_Module {
  cephyr_module *image;
  Ternary clamp;
  Binary add;
  Binary sub;
};

_Static_assert(sizeof(clamp_source) <= 64u * 1024u, "clamp TU exceeds source budget");
_Static_assert(sizeof(baseline_config) <= 4096u, "Cephyr config exceeds budget");
_Static_assert(sizeof(void *) == sizeof(Binary) && sizeof(void *) == sizeof(Ternary),
               "expected SysV AMD64 function pointer representation");

uint32_t on1x_kernel_rt_abi_version(void) { return ON1X_KERNEL_RT_ABI_VERSION; }

const char *on1x_kernel_rt_status_message(On1xKernelRT_Status status) {
  switch (status) {
  case ON1X_KERNEL_RT_OK: return "standard FFI: success";
  case ON1X_KERNEL_RT_INVALID_ARGUMENT: return "standard FFI: invalid argument";
  case ON1X_KERNEL_RT_UNSUPPORTED: return "standard FFI: unsupported module, operation, or Cephyr ABI";
  case ON1X_KERNEL_RT_OUT_OF_MEMORY: return "standard FFI: out of memory";
  case ON1X_KERNEL_RT_CONFIG_ERROR: return "standard FFI: could not pin the Cephyr configuration in memory";
  case ON1X_KERNEL_RT_COMPILE_ERROR: return "standard FFI: Cephyr could not compile the pinned kernel";
  case ON1X_KERNEL_RT_IMAGE_ERROR: return "standard FFI: Cephyr did not produce a bounded AMD64 shared image";
  case ON1X_KERNEL_RT_LOAD_ERROR: return "standard FFI: Cephyr could not load the kernel image";
  case ON1X_KERNEL_RT_SYMBOL_ERROR: return "standard FFI: pinned kernel export was not found";
  case ON1X_KERNEL_RT_SIGNATURE_MISMATCH: return "standard FFI: wrong argument count";
  case ON1X_KERNEL_RT_INVALID_INTERVAL: return "standard FFI clamp: expected low <= high";
  default: return "standard FFI: unknown status";
  }
}

/* Supplying --config explicitly bypasses cwd and CEPHYR_CONFIG selection.
 * The known config has no path references and the package locates its own
 * resources and native assembler/linker relative to libcephyr. */
static int config_fd(void) {
  const int fd = memfd_create("on1x-stdffi-config", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) return -1;
  for (size_t offset = 0; offset < sizeof(baseline_config);) {
    const ssize_t n = write(fd, baseline_config + offset, sizeof(baseline_config) - offset);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) goto fail;
    offset += (size_t)n;
  }
  if (fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) == 0)
    return fd;
fail:
  (void)close(fd);
  return -1;
}

static On1xKernelRT_Status load_clamp(On1xKernelRT_Module *module) {
  const int fd = config_fd();
  if (fd < 0) return ON1X_KERNEL_RT_CONFIG_ERROR;
  char path[64];
  const int length = snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
  if (length < 0 || (size_t)length >= sizeof(path) || access(path, R_OK) != 0) {
    (void)close(fd);
    return ON1X_KERNEL_RT_CONFIG_ERROR;
  }
  const char *const argv[] = {
    "--config", path, "--target=x86_64-linux-gnu", "--stdlib=cephyr",
    "--no-factir", "--no-sandslab", "-std=c11", "-fPIC", "-shared", "-nostdlib",
    "-Wl,--no-undefined", "-x", "c", "-", "-o", "-"
  };
  const cephyr_request request = {
    .struct_size = sizeof(request), .argc = sizeof(argv) / sizeof(argv[0]), .argv = argv,
    .stdin_data = (const char *)clamp_source, .stdin_size = sizeof(clamp_source)
  };
  cephyr_result *artifact = NULL;
  const int compiled = cephyr_compile(&request, &artifact);
  (void)close(fd);
  On1xKernelRT_Status status = ON1X_KERNEL_RT_COMPILE_ERROR;
  if (compiled != CEPHYR_OK) goto done;

  size_t size = 0;
  const char *image = cephyr_result_output(artifact, &size);
  status = ON1X_KERNEL_RT_IMAGE_ERROR;
  if (!image || size < 64u || size > 1024u * 1024u ||
      memcmp(image, "\177ELF\2\1\1", 7u) || memcmp(image + 16, "\3\0\76\0", 4u))
    goto done;
  status = ON1X_KERNEL_RT_LOAD_ERROR;
  if (cephyr_module_load(artifact, &module->image, NULL) != CEPHYR_OK) goto done;

  /* Each name and its C prototype are fixed by the embedded MuLIB TU.
   * POSIX dlsym addresses can be converted to the compatible function type.
   * Use memcpy to express this without a non-ISO-C pointer cast. */
  void *clamp = NULL, *add = NULL, *sub = NULL;
  status = ON1X_KERNEL_RT_SYMBOL_ERROR;
  if (cephyr_module_symbol(module->image, "on1x_stdffi_v1_clamp", &clamp) != CEPHYR_OK || !clamp ||
      cephyr_module_symbol(module->image, "on1x_stdffi_v1_add", &add) != CEPHYR_OK || !add ||
      cephyr_module_symbol(module->image, "on1x_stdffi_v1_sub", &sub) != CEPHYR_OK || !sub)
    goto done;
  memcpy(&module->clamp, &clamp, sizeof(clamp));
  memcpy(&module->add, &add, sizeof(add));
  memcpy(&module->sub, &sub, sizeof(sub));
  status = ON1X_KERNEL_RT_OK;

done:
  cephyr_result_destroy(artifact);
  return status;
}

On1xKernelRT_Status on1x_kernel_rt_open(On1xKernelRT_ModuleId id,
                                        On1xKernelRT_Module **module) {
  if (!module || *module) return ON1X_KERNEL_RT_INVALID_ARGUMENT;
  if (id != ON1X_KERNEL_RT_MULIB_CLAMP || cephyr_abi_version() != 1 ||
      cephyr_python_version() != 0) return ON1X_KERNEL_RT_UNSUPPORTED;
  On1xKernelRT_Module *candidate = calloc(1, sizeof(*candidate));
  if (!candidate) return ON1X_KERNEL_RT_OUT_OF_MEMORY;
  const On1xKernelRT_Status status = load_clamp(candidate);
  if (status == ON1X_KERNEL_RT_OK) *module = candidate;
  else on1x_kernel_rt_close(&candidate);
  return status;
}

void on1x_kernel_rt_close(On1xKernelRT_Module **module) {
  if (!module || !*module) return;
  On1xKernelRT_Module *closed = *module;
  *module = NULL;
  cephyr_module_destroy(closed->image);
  free(closed);
}

static On1xKernelRT_Status check_call(On1xKernelRT_Operation operation,
                                      const int64_t *args, size_t count,
                                      const int64_t *result) {
  if (!args || !result) return ON1X_KERNEL_RT_INVALID_ARGUMENT;
  switch (operation) {
  case ON1X_KERNEL_RT_CLAMP_I64:
    if (count != 3u) return ON1X_KERNEL_RT_SIGNATURE_MISMATCH;
    if (args[1] > args[2]) return ON1X_KERNEL_RT_INVALID_INTERVAL;
    return ON1X_KERNEL_RT_OK;
  case ON1X_KERNEL_RT_ADD_I64:
  case ON1X_KERNEL_RT_SUB_I64:
    return count == 2u ? ON1X_KERNEL_RT_OK : ON1X_KERNEL_RT_SIGNATURE_MISMATCH;
  default:
    return ON1X_KERNEL_RT_UNSUPPORTED;
  }
}

On1xKernelRT_Status on1x_kernel_rt_call_i64(const On1xKernelRT_Module *module,
                                            On1xKernelRT_Operation operation,
                                            const int64_t *args, size_t count,
                                            int64_t *result) {
  if (!module) return ON1X_KERNEL_RT_INVALID_ARGUMENT;
  const On1xKernelRT_Status status = check_call(operation, args, count, result);
  if (status != ON1X_KERNEL_RT_OK) return status;
  if (operation == ON1X_KERNEL_RT_CLAMP_I64) *result = module->clamp(args[0], args[1], args[2]);
  else if (operation == ON1X_KERNEL_RT_ADD_I64) *result = module->add(args[0], args[1]);
  else *result = module->sub(args[0], args[1]);
  return ON1X_KERNEL_RT_OK;
}

static pthread_once_t builtin_once = PTHREAD_ONCE_INIT;
static On1xKernelRT_Module *builtin_module;
static On1xKernelRT_Status builtin_status;

static void init_builtin(void) {
  builtin_status = on1x_kernel_rt_open(ON1X_KERNEL_RT_MULIB_CLAMP, &builtin_module);
}

On1xKernelRT_Status on1x_kernel_rt_builtin_i64(On1xKernelRT_Operation operation,
                                               const int64_t *args, size_t count,
                                               int64_t *result) {
  const On1xKernelRT_Status status = check_call(operation, args, count, result);
  if (status != ON1X_KERNEL_RT_OK) return status;
  if (pthread_once(&builtin_once, init_builtin) != 0) return ON1X_KERNEL_RT_LOAD_ERROR;
  if (builtin_status != ON1X_KERNEL_RT_OK) return builtin_status;
  return on1x_kernel_rt_call_i64(builtin_module, operation, args, count, result);
}
