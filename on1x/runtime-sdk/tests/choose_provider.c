#include "On1xRT.h"

/* Fixture-specific adapter compiled against this exact emitter output. */
#define main on1x_fixture_program_main
#include "choose.inc"
#undef main

static On1xRT_Status choose(On1xRT_CallContext *call, void *context,
                            const On1xRT_Value *args, size_t count,
                            On1xRT_Value *result, On1xRT_Error **error) {
  (void)call;
  (void)context;
  (void)error;
  if (count != 2u)
    return ON1X_RT_INVALID_ARGUMENT;
  *result = on1x_rt_value_i64(on1x_fn_choose(args[0].as.i64, args[1].as.i64));
  return ON1X_RT_OK;
}

static On1xRT_Status fail(On1xRT_CallContext *call, void *context,
                          const On1xRT_Value *args, size_t count,
                          On1xRT_Value *result, On1xRT_Error **error) {
  (void)context;
  (void)args;
  (void)count;
  (void)result;
  on1x_rt_call_error(call, 4321, (On1xRT_StringView){"provider failed", 15},
                     error);
  return ON1X_RT_CALLBACK_FAILED;
}

ON1X_RT_EXPORT const On1xRT_ModuleDescriptor *
ON1X_RT_MODULE_ENTRY_SYMBOL(void) {
  static const On1xRT_ValueKind args[] = {ON1X_RT_VALUE_I64, ON1X_RT_VALUE_I64};
  static const On1xRT_ExportDescriptor exports[] = {
      {1,
       sizeof(*exports),
       {"choose", 6},
       {1, sizeof(On1xRT_Signature), args, 2, ON1X_RT_VALUE_I64, 0},
       choose,
       NULL,
       0,
       0},
      {1,
       sizeof(*exports),
       {"fail", 4},
       {1, sizeof(On1xRT_Signature), NULL, 0, ON1X_RT_VALUE_I64, 0},
       fail,
       NULL,
       0,
       0}};
  static const On1xRT_ModuleDescriptor descriptor = {
      .abi_version = 1,
      .struct_size = sizeof(descriptor),
      .descriptor_version = 1,
      .flags = ON1X_RT_MODULE_STATIC,
      .name = {"choose", 6},
      .revision = {"1", 1},
      .identity_sha256 =
          {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
           64},
      .exports = exports,
      .export_count = 2};
  return &descriptor;
}
