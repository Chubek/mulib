#include "On1xRT.h"

#include <stdio.h>

/* The private C11 ABI stays inside this fixture, not in the RT descriptor. */
#define main on1x_fixture_program_main
#include "ownership.inc"
#undef main

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);             \
      return 1;                                                                \
    }                                                                          \
  } while (0)

struct trace {
  unsigned initialized;
  unsigned invoked;
  unsigned shutdowns;
};

static On1xRT_Status initialize(On1xRT_ModuleContext *ctx, void *userdata,
                                void **state, On1xRT_Error **error) {
  (void)ctx;
  (void)error;
  ++((struct trace *)userdata)->initialized;
  *state = NULL;
  return ON1X_RT_OK;
}

static On1xRT_Status shutdown_module(On1xRT_ModuleContext *ctx, void *userdata,
                                    void *state, On1xRT_Error **error) {
  (void)ctx;
  (void)state;
  (void)error;
  ++((struct trace *)userdata)->shutdowns;
  return ON1X_RT_OK;
}

static On1xRT_Status owned_value(On1xRT_CallContext *ctx, void *userdata,
                                const On1xRT_Value *args, size_t count,
                                On1xRT_Value *result, On1xRT_Error **error) {
  (void)ctx;
  (void)error;
  if (count != 2u)
    return ON1X_RT_INVALID_ARGUMENT;
  ++((struct trace *)userdata)->invoked;
  *result = on1x_rt_value_i64(on1x_fn_owned_value(args[0].as.i64, args[1].as.i64));
  return ON1X_RT_OK;
}

int main(void) {
  const On1xRT_ValueKind parameters[] = {ON1X_RT_VALUE_I64, ON1X_RT_VALUE_I64};
  /* No ABI-1 tag for own<...>; do not encode one as a scalar or byte buffer. */
  const On1xRT_ValueKind unknown_kind = (On1xRT_ValueKind)6;
  const On1xRT_ValueKind invalid_parameters[] = {unknown_kind, ON1X_RT_VALUE_I64};
  const On1xRT_Signature scalar = {
      ON1X_RT_ABI_VERSION, sizeof(scalar), parameters, 2, ON1X_RT_VALUE_I64, 0};
  On1xRT_Signature invalid[] = {scalar, scalar, scalar};
  struct trace trace = {0};
  On1xRT_ExportDescriptor exported = {
      ON1X_RT_ABI_VERSION, sizeof(exported), {"owned_value", 11}, scalar,
      owned_value, &trace, 0, 0};
  const On1xRT_ModuleDescriptor descriptor = {
      .abi_version = ON1X_RT_ABI_VERSION,
      .struct_size = sizeof(descriptor),
      .descriptor_version = 1,
      .flags = ON1X_RT_MODULE_STATIC,
      .name = {"ownership", 9},
      .revision = {"1", 1},
      .identity_sha256 =
          {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
           64},
      .exports = &exported,
      .export_count = 1,
      .provider_context = &trace,
      .initialize = initialize,
      .shutdown = shutdown_module};
  On1xRT_Engine *engine = NULL;
  On1xRT_Module module = {0};
  On1xRT_Instance instance = {0};
  On1xRT_Function function = {0};
  On1xRT_Value result = on1x_rt_value_void();
  On1xRT_Value args[] = {on1x_rt_value_i64(INT64_MIN),
                       on1x_rt_value_i64(INT64_MAX)};

  CHECK(on1x_rt_engine_create(NULL, &engine, NULL) == ON1X_RT_OK);
  invalid[0].parameters = invalid_parameters;
  invalid[1].result = unknown_kind;
  invalid[2].flags = 1; /* No ownership/borrow modifiers on signatures. */
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
    exported.signature = invalid[i];
    CHECK(on1x_rt_engine_register_static_module(
              engine, &descriptor, &module, NULL) == ON1X_RT_INVALID_ARGUMENT);
    CHECK(!module.id && !module.owner && !trace.initialized && !trace.shutdowns);
  }
  exported.signature = scalar;
  CHECK(on1x_rt_engine_register_static_module(
            engine, &descriptor, &module, NULL) == ON1X_RT_OK);
  CHECK(trace.initialized == 1);
  CHECK(on1x_rt_instance_create(engine, module, &instance, NULL) == ON1X_RT_OK);
  CHECK(on1x_rt_instance_function(
            engine, instance, exported.name, &scalar, &function, NULL) ==
        ON1X_RT_OK);

  args[0].kind = unknown_kind;
  CHECK(on1x_rt_function_call(engine, function, args, 2, &result, NULL) ==
        ON1X_RT_SIGNATURE_MISMATCH);
  CHECK(result.kind == ON1X_RT_VALUE_VOID && !trace.invoked);
  args[0] = on1x_rt_value_i64(INT64_MIN);
  CHECK(on1x_unique_live_allocations() == 0);
  for (unsigned i = 0; i < 5; ++i) {
    CHECK(on1x_rt_function_call(engine, function, args, 2, &result, NULL) ==
          ON1X_RT_OK);
    CHECK(result.kind == ON1X_RT_VALUE_I64 && result.as.i64 == args[1].as.i64);
    CHECK(on1x_unique_live_allocations() == 0);
    const On1xRT_Value previous = args[0];
    args[0] = args[1];
    args[1] = previous;
  }
  CHECK(trace.invoked == 5);
  CHECK(on1x_rt_function_release(engine, &function, NULL) == ON1X_RT_OK);
  CHECK(on1x_rt_instance_release(engine, &instance, NULL) == ON1X_RT_OK);
  CHECK(on1x_rt_module_unload(engine, &module, NULL) == ON1X_RT_OK);
  CHECK(trace.shutdowns == 1);
  CHECK(on1x_rt_engine_destroy(&engine, NULL) == ON1X_RT_OK);
  return 0;
}
