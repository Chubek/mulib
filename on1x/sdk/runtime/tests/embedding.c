#include "On1xRT.h"

extern const On1xRT_ModuleDescriptor *ON1X_RT_MODULE_ENTRY_SYMBOL(void);

int main(void) {
  On1xRT_Engine *host = NULL;
  On1xRT_Error *error = NULL;
  On1xRT_Module module = {0};
  On1xRT_Instance instance = {0};
  On1xRT_StringView choose = {"choose", 6};
  const int64_t args[] = {INT64_MIN, INT64_MAX};
  int64_t result = -1;
  if (on1x_rt_abi_version() != ON1X_RT_ABI_VERSION ||
      on1x_rt_engine_create(NULL, &host, NULL) ||
      on1x_rt_engine_register_static_module(host, ON1X_RT_MODULE_ENTRY_SYMBOL(),
                                            &module, NULL) ||
      on1x_rt_instance_create(host, module, &instance, NULL))
    return 1;
  if (on1x_rt_instance_call_i64(host, instance, choose, args, 33, &result,
                                NULL) != ON1X_RT_LIMIT ||
      on1x_rt_instance_call_i64(host, instance, choose, NULL, 2, &result,
                                NULL) != ON1X_RT_INVALID_ARGUMENT ||
      on1x_rt_instance_call_i64(host, instance, choose, args, 1, &result,
                                NULL) != ON1X_RT_SIGNATURE_MISMATCH ||
      result != -1)
    return 2;
  if (on1x_rt_instance_call_i64(host, instance, (On1xRT_StringView){"fail", 4},
                                NULL, 0, &result,
                                &error) != ON1X_RT_CALLBACK_FAILED ||
      result != -1 || on1x_rt_error_code(error) != 4321 ||
      on1x_rt_error_release(&error))
    return 3;
  if (on1x_rt_instance_call_i64(host, instance, choose, args, 2, &result,
                                NULL) ||
      result != INT64_MAX)
    return 4;
  return on1x_rt_instance_release(host, &instance, NULL) ||
         on1x_rt_module_unload(host, &module, NULL) ||
         on1x_rt_engine_destroy(&host, NULL);
}
