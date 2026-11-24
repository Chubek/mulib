#include <On1xRT.h>

/*
 * Fixture only. Build this provider with the real checked emitter output and
 * typed FFI native plan, not with a hand-written kernel or wrapper. The C
 * name is private to the accompanying On1x fixture.
 */
extern int64_t on1x_fn_saturating_add(int64_t, int64_t);

#ifndef ON1X_RT_FFI_TEST_IDENTITY
#error "Supply the fixture source identity from the build, not a runtime lock"
#endif

#define ABI ON1X_RT_ABI_VERSION
#define TEXT(s) {s, sizeof(s) - 1u}

static const On1xRT_ValueKind parameters[] = {ON1X_RT_VALUE_I64,
                                             ON1X_RT_VALUE_I64};
static const On1xRT_Signature signature = {ABI, sizeof(signature), parameters,
                                          2, ON1X_RT_VALUE_I64, 0};

static On1xRT_Status saturating_add(On1xRT_CallContext *call, void *context,
                                   const On1xRT_Value *args, size_t count,
                                   On1xRT_Value *result, On1xRT_Error **error) {
  static const On1xRT_StringView host_name = TEXT("inspect_result");
  On1xRT_Buffer transient = {0};
  (void)context;
  if (count != 2)
    return ON1X_RT_SIGNATURE_MISMATCH;
  /* The buffer is deliberately not returned: success and error must release
   * it at the outer call boundary, including when the host callback fails. */
  const On1xRT_Status status = on1x_rt_call_buffer_create(
      call, "temporary", sizeof("temporary") - 1u, &transient, error);
  if (status != ON1X_RT_OK)
    return status;
  On1xRT_Value replacement[] = {
      on1x_rt_value_i64(on1x_fn_saturating_add(args[0].as.i64, args[1].as.i64)),
      args[1]};
  return on1x_rt_call_host(call, host_name, &signature, replacement, 2, result,
                           error);
}

ON1X_RT_EXPORT const On1xRT_ModuleDescriptor *ON1X_RT_MODULE_ENTRY_SYMBOL(void) {
  static const On1xRT_ExportDescriptor exports[] = {
      {ABI, sizeof(*exports), TEXT("saturating_add"),
       {ABI, sizeof(On1xRT_Signature), parameters, 2, ON1X_RT_VALUE_I64, 0},
       saturating_add, NULL, 0, 0}};
  static const On1xRT_ModuleDescriptor descriptor = {
      .abi_version = ABI,
      .struct_size = sizeof(descriptor),
      .descriptor_version = 1,
      .flags = ON1X_RT_MODULE_AUTHORIZED_AOT,
      .name = TEXT("ffi_embed"),
      .revision = TEXT("1"),
      .identity_sha256 = TEXT(ON1X_RT_FFI_TEST_IDENTITY),
      .exports = exports,
      .export_count = 1};
  return &descriptor;
}
