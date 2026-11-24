#include <On1xRT.h>

#include <stdio.h>
#include <string.h>

#define ABI ON1X_RT_ABI_VERSION
#define TEXT(s) ((On1xRT_StringView){s, sizeof(s) - 1u})
#define CHECK(e)                                                               \
  do {                                                                         \
    if (!(e)) {                                                                \
      fprintf(stderr, "ffi_embedding.c:%d: %s\n", __LINE__, #e);                \
      return 1;                                                                \
    }                                                                          \
  } while (0)

#ifdef ON1X_RT_FFI_TEST_STATIC
extern const On1xRT_ModuleDescriptor *ON1X_RT_MODULE_ENTRY_SYMBOL(void);
#endif

typedef struct Policy {
  On1xRT_StringView identity;
  On1xRT_StringView path;
  On1xRT_StringView digest;
  unsigned artifact_checks;
  unsigned descriptor_checks;
} Policy;

static On1xRT_StringView text(const char *s) {
  return (On1xRT_StringView){s, strlen(s)};
}

static int equal(On1xRT_StringView a, On1xRT_StringView b) {
  return a.size == b.size && (!a.size || !memcmp(a.data, b.data, a.size));
}

static On1xRT_Status descriptor_allowed(void *context,
                                         const On1xRT_ModuleDescriptor *d) {
  Policy *p = context;
  ++p->descriptor_checks;
  return d->abi_version == ABI && d->flags == ON1X_RT_MODULE_AUTHORIZED_AOT &&
                 equal(d->name, TEXT("ffi_embed")) && equal(d->revision, TEXT("1")) &&
                 equal(d->identity_sha256, p->identity)
             ? ON1X_RT_OK
             : ON1X_RT_DENIED;
}

#ifndef ON1X_RT_FFI_TEST_STATIC
static On1xRT_Status artifact_allowed(void *context, On1xRT_StringView path,
                                       On1xRT_StringView digest) {
  Policy *p = context;
  ++p->artifact_checks;
  return equal(p->path, path) && equal(p->digest, digest) ? ON1X_RT_OK
                                                       : ON1X_RT_DENIED;
}
#endif

typedef struct Observation {
  On1xRT_Engine *engine;
  int64_t expected_sum;
  int64_t expected_rhs;
  unsigned calls;
  int reject;
} Observation;

static On1xRT_Status inspect(On1xRT_CallContext *call, void *context,
                              const On1xRT_Value *arguments, size_t count,
                              On1xRT_Value *result, On1xRT_Error **error) {
  Observation *state = context;
  ++state->calls;
  if (count != 2u || arguments[0].kind != ON1X_RT_VALUE_I64 ||
      arguments[1].kind != ON1X_RT_VALUE_I64 ||
      arguments[0].as.i64 != state->expected_sum ||
      arguments[1].as.i64 != state->expected_rhs ||
      on1x_rt_engine_destroy(&state->engine, NULL) != ON1X_RT_REENTRANT)
    return ON1X_RT_ERROR;
  if (state->reject) {
    On1xRT_Status status = on1x_rt_call_error(call, 4224u, TEXT("host decline"), error);
    return status == ON1X_RT_OK ? ON1X_RT_CALLBACK_FAILED : status;
  }
  *result = arguments[0];
  return ON1X_RT_OK;
}

int main(int argc, char **argv) {
  const On1xRT_StringView name = TEXT("saturating_add");
  const int64_t overshoot[] = {INT64_MAX, 10}, undershoot[] = {INT64_MIN, -10};
  const int64_t average[] = {-123, 321};
  int64_t result = -1;
  On1xRT_Engine *engine = NULL;
  On1xRT_Module module = {0};
  On1xRT_Instance instance = {0};
  On1xRT_HostFunction callback = {0};
  On1xRT_Error *error = NULL;
  Policy policy = {0};
#ifdef ON1X_RT_FFI_TEST_STATIC
  CHECK(argc == 2);
#else
  CHECK(argc == 5);
  CHECK(on1x_rt_feature_status(ON1X_RT_FEATURE_DYNAMIC_MODULE) == ON1X_RT_OK);
#endif
  const On1xRT_StringView identity = text(argv[1]);
  CHECK(identity.size == 64u);
  policy.identity = TEXT("not authorized");
  const On1xRT_EngineConfig config = {
      .abi_version = ABI, .struct_size = sizeof(config), .max_handles = 5u,
      .max_modules = 1u, .max_host_functions = 1u,
      /* One unreturned temporary fills the entire buffer budget. */
      .max_value_bytes = sizeof("temporary") - 1u,
      .authorization_context = &policy, .authorize_aot = descriptor_allowed};
  CHECK(on1x_rt_engine_create(&config, &engine, NULL) == ON1X_RT_OK);
#ifdef ON1X_RT_FFI_TEST_STATIC
  const On1xRT_ModuleDescriptor *descriptor = ON1X_RT_MODULE_ENTRY_SYMBOL();
  CHECK(on1x_rt_module_descriptor_check(descriptor, NULL) == ON1X_RT_OK);
  CHECK(on1x_rt_engine_register_aot_module(engine, descriptor, &module, NULL) ==
        ON1X_RT_DENIED);
  CHECK(!module.id && policy.descriptor_checks == 1u);
  policy.identity = identity;
  CHECK(on1x_rt_engine_register_aot_module(engine, descriptor, &module, NULL) ==
        ON1X_RT_OK);
#else
  const On1xRT_StringView path = text(argv[2]), changed = text(argv[4]);
  policy.digest = text(argv[3]);
  policy.path = changed;
  CHECK(on1x_rt_engine_load_dynamic(engine, path, &module, NULL) == ON1X_RT_UNSUPPORTED);
  CHECK(on1x_rt_engine_load_authorized_dynamic(engine, path, artifact_allowed,
                                              &policy, &module, NULL) == ON1X_RT_DENIED);
  CHECK(on1x_rt_engine_load_authorized_dynamic(engine, changed, artifact_allowed,
                                              &policy, &module, NULL) == ON1X_RT_DENIED);
  CHECK(!module.id && !module.owner && policy.artifact_checks == 2u &&
        policy.descriptor_checks == 0u);
  policy.path = path;
  CHECK(on1x_rt_engine_load_authorized_dynamic(engine, path, artifact_allowed,
                                              &policy, &module, NULL) == ON1X_RT_DENIED);
  CHECK(!module.id && policy.descriptor_checks == 1u);
  policy.identity = identity;
  CHECK(on1x_rt_engine_load_authorized_dynamic(engine, path, artifact_allowed,
                                              &policy, &module, NULL) == ON1X_RT_OK);
  CHECK(policy.artifact_checks == 4u);
#endif
  CHECK(module.id && module.owner && policy.descriptor_checks == 2u);
  CHECK(on1x_rt_instance_create(engine, module, &instance, NULL) == ON1X_RT_OK);
  CHECK(on1x_rt_module_unload(engine, &module, NULL) == ON1X_RT_BUSY);
  CHECK(on1x_rt_engine_destroy(&engine, NULL) == ON1X_RT_BUSY);
  CHECK(on1x_rt_instance_call_i64(engine, instance, name, overshoot, 1u, &result, NULL) ==
        ON1X_RT_SIGNATURE_MISMATCH);
  CHECK(on1x_rt_instance_call_i64(engine, instance, name, overshoot, 2u, &result, NULL) ==
        ON1X_RT_NOT_FOUND);
  CHECK(result == -1);

  Observation state = {engine, INT64_MAX, overshoot[1], 0, 0};
  const On1xRT_ValueKind params[] = {ON1X_RT_VALUE_I64, ON1X_RT_VALUE_I64};
  const On1xRT_HostFunctionDescriptor host_function = {
      .abi_version = ABI, .struct_size = sizeof(host_function),
      .name = TEXT("inspect_result"),
      .signature = {ABI, sizeof(On1xRT_Signature), params, 2u, ON1X_RT_VALUE_I64, 0u},
      .invoke = inspect, .function_context = &state};
  CHECK(on1x_rt_engine_register_host_function(engine, &host_function, &callback, NULL) ==
        ON1X_RT_OK);
  CHECK(on1x_rt_instance_call_i64(engine, instance, name, overshoot, 2u, &result, NULL) ==
        ON1X_RT_OK);
  CHECK(result == INT64_MAX && state.calls == 1u);
  state.reject = 1;
  CHECK(on1x_rt_instance_call_i64(engine, instance, name, overshoot, 2u, &result, &error) ==
        ON1X_RT_CALLBACK_FAILED);
  CHECK(result == INT64_MAX && state.calls == 2u && on1x_rt_error_code(error) == 4224u);
  state.reject = 0;
  state.expected_sum = INT64_MIN;
  state.expected_rhs = undershoot[1];
  CHECK(on1x_rt_instance_call_i64(engine, instance, name, undershoot, 2u, &result, NULL) ==
        ON1X_RT_OK);
  CHECK(result == INT64_MIN && state.calls == 3u);
  state.expected_sum = 198;
  state.expected_rhs = average[1];
  CHECK(on1x_rt_instance_call_i64(engine, instance, name, average, 2u, &result, NULL) ==
        ON1X_RT_OK);
  CHECK(result == 198 && state.calls == 4u);

  CHECK(on1x_rt_host_function_unregister(engine, &callback, NULL) == ON1X_RT_OK);
  CHECK(on1x_rt_instance_call_i64(engine, instance, name, average, 2u, &result, NULL) ==
        ON1X_RT_NOT_FOUND);
  CHECK(state.calls == 4u && result == 198);
  On1xRT_Instance stale_instance = instance;
  CHECK(on1x_rt_instance_release(engine, &instance, NULL) == ON1X_RT_OK);
  CHECK(!instance.id && on1x_rt_instance_release(engine, &stale_instance, NULL) ==
                        ON1X_RT_STALE_HANDLE);
  On1xRT_Module stale_module = module;
  CHECK(on1x_rt_module_unload(engine, &module, NULL) == ON1X_RT_OK);
  CHECK(!module.id && on1x_rt_module_unload(engine, &stale_module, NULL) ==
                      ON1X_RT_STALE_HANDLE);
  CHECK(on1x_rt_engine_destroy(&engine, NULL) == ON1X_RT_OK && !engine);
  CHECK(equal(on1x_rt_error_message(error), TEXT("host decline")));
  CHECK(on1x_rt_error_release(&error) == ON1X_RT_OK && !error);
  return 0;
}
