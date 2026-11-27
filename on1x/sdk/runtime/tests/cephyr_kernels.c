#include <On1xKernelRT.h>

#include <inttypes.h>
#include <stdio.h>

#define CHECK(expr) do { \
  if (!(expr)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } \
} while (0)

static int verify(const On1xKernelRT_Module *module, On1xKernelRT_Operation operation,
                  int64_t a, int64_t b, int64_t c, size_t count, int64_t expected) {
  int64_t result = 0;
  const int64_t args[] = {a, b, c};
  const On1xKernelRT_Status status = on1x_kernel_rt_call_i64(module, operation, args, count, &result);
  if (status == ON1X_KERNEL_RT_OK && result == expected) return 1;
  fprintf(stderr, "operation %" PRIu32 ": %s; result %" PRId64 ", expected %" PRId64 "\n",
          operation, on1x_kernel_rt_status_message(status), result, expected);
  return 0;
}

int main(void) {
  On1xKernelRT_Module *module = NULL, *second = NULL;
  int64_t result = 42;
  int64_t args[] = {42, 5, -3};
  CHECK(on1x_kernel_rt_abi_version() == ON1X_KERNEL_RT_ABI_VERSION);
  CHECK(on1x_kernel_rt_open(0, &module) == ON1X_KERNEL_RT_UNSUPPORTED && !module);
  CHECK(on1x_kernel_rt_open(ON1X_KERNEL_RT_MULIB_CLAMP, NULL) == ON1X_KERNEL_RT_INVALID_ARGUMENT);
  CHECK(on1x_kernel_rt_call_i64(NULL, ON1X_KERNEL_RT_CLAMP_I64, args, 3, &result)
        == ON1X_KERNEL_RT_INVALID_ARGUMENT && result == 42);
  const On1xKernelRT_Status opened = on1x_kernel_rt_open(ON1X_KERNEL_RT_MULIB_CLAMP, &module);
  if (opened != ON1X_KERNEL_RT_OK) fprintf(stderr, "%s\n", on1x_kernel_rt_status_message(opened));
  CHECK(opened == ON1X_KERNEL_RT_OK && module);
  CHECK(on1x_kernel_rt_open(ON1X_KERNEL_RT_MULIB_CLAMP, &module) == ON1X_KERNEL_RT_INVALID_ARGUMENT);
  CHECK(on1x_kernel_rt_open(ON1X_KERNEL_RT_MULIB_CLAMP, &second) == ON1X_KERNEL_RT_OK);
  CHECK(module != second);
  CHECK(verify(module, ON1X_KERNEL_RT_CLAMP_I64, -9, -8, 3, 3, -8));
  CHECK(verify(module, ON1X_KERNEL_RT_CLAMP_I64, 9, -8, 3, 3, 3));
  CHECK(verify(module, ON1X_KERNEL_RT_CLAMP_I64, 0, -8, 3, 3, 0));
  CHECK(verify(module, ON1X_KERNEL_RT_CLAMP_I64, 0, 1, 1, 3, 1));
  CHECK(verify(module, ON1X_KERNEL_RT_ADD_I64, INT64_MAX, 1, 0, 2, INT64_MAX));
  CHECK(verify(module, ON1X_KERNEL_RT_ADD_I64, INT64_MIN, -1, 0, 2, INT64_MIN));
  CHECK(verify(module, ON1X_KERNEL_RT_ADD_I64, 19, -2, 0, 2, 17));
  CHECK(verify(module, ON1X_KERNEL_RT_SUB_I64, INT64_MIN, 1, 0, 2, INT64_MIN));
  CHECK(verify(module, ON1X_KERNEL_RT_SUB_I64, INT64_MAX, INT64_MIN, 0, 2, INT64_MAX));
  CHECK(verify(module, ON1X_KERNEL_RT_SUB_I64, 19, -2, 0, 2, 21));
  CHECK(on1x_kernel_rt_call_i64(module, ON1X_KERNEL_RT_CLAMP_I64, args, 3, &result)
        == ON1X_KERNEL_RT_INVALID_INTERVAL && result == 42);
  CHECK(on1x_kernel_rt_call_i64(module, ON1X_KERNEL_RT_CLAMP_I64, args, SIZE_MAX, &result)
        == ON1X_KERNEL_RT_SIGNATURE_MISMATCH && result == 42);
  CHECK(on1x_kernel_rt_call_i64(module, 0, args, 3, &result)
        == ON1X_KERNEL_RT_UNSUPPORTED && result == 42);
  CHECK(on1x_kernel_rt_call_i64(module, ON1X_KERNEL_RT_ADD_I64, NULL, 2, &result)
        == ON1X_KERNEL_RT_INVALID_ARGUMENT && result == 42);
  on1x_kernel_rt_close(&module);
  CHECK(!module);
  on1x_kernel_rt_close(&module);
  on1x_kernel_rt_close(NULL);
  CHECK(verify(second, ON1X_KERNEL_RT_SUB_I64, 70, 8, 0, 2, 62));
  on1x_kernel_rt_close(&second);
  CHECK(!second);
  CHECK(on1x_kernel_rt_builtin_i64(ON1X_KERNEL_RT_CLAMP_I64, args, 3, &result)
        == ON1X_KERNEL_RT_INVALID_INTERVAL && result == 42);
  CHECK(on1x_kernel_rt_builtin_i64(ON1X_KERNEL_RT_ADD_I64, args, 2, &result)
        == ON1X_KERNEL_RT_OK && result == 47);
  CHECK(on1x_kernel_rt_builtin_i64(ON1X_KERNEL_RT_SUB_I64, args, 2, &result)
        == ON1X_KERNEL_RT_OK && result == 37);
  return 0;
}
