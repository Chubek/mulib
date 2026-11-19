#include "fixture_module.h"

#include <stdalign.h>
#include <stdint.h>
#include <string.h>

#define VIEW(text) {text, sizeof(text) - 1u}

typedef struct FixtureModuleState {
  uint32_t marker;
} FixtureModuleState;

typedef struct FixtureInstanceState {
  int64_t bias;
} FixtureInstanceState;

static On1xRT_TestFixtureCounters fixture_counters;
static On1xRT_Engine *fixture_engine;

void on1x_rt_test_fixture_reset(void) {
  memset(&fixture_counters, 0, sizeof(fixture_counters));
  fixture_engine = NULL;
}

On1xRT_TestFixtureCounters on1x_rt_test_fixture_counters(void) {
  return fixture_counters;
}

void on1x_rt_test_fixture_set_engine(On1xRT_Engine *engine) {
  fixture_engine = engine;
}

static On1xRT_Status fixture_initialize(On1xRT_ModuleContext *context,
                                        void *provider_context,
                                        void **module_state,
                                        On1xRT_Error **error) {
  FixtureModuleState *state;
  (void)provider_context;
  (void)error;
  state = (FixtureModuleState *)on1x_rt_module_allocate(
      context, sizeof(*state), alignof(FixtureModuleState));
  if (!state)
    return ON1X_RT_OUT_OF_MEMORY;
  state->marker = UINT32_C(0x6f6e3178);
  *module_state = state;
  ++fixture_counters.initialize_calls;
  return ON1X_RT_OK;
}

static On1xRT_Status fixture_shutdown(On1xRT_ModuleContext *context,
                                      void *provider_context,
                                      void *module_state,
                                      On1xRT_Error **error) {
  FixtureModuleState *state = (FixtureModuleState *)module_state;
  (void)provider_context;
  (void)error;
  if (!state || state->marker != UINT32_C(0x6f6e3178))
    return ON1X_RT_INVALID_STATE;
  state->marker = 0u;
  on1x_rt_module_release_memory(context, state, sizeof(*state),
                                alignof(FixtureModuleState));
  ++fixture_counters.shutdown_calls;
  return ON1X_RT_OK;
}

static On1xRT_Status fixture_instance_create(On1xRT_ModuleContext *context,
                                             void *provider_context,
                                             void *module_state,
                                             void **instance_state,
                                             On1xRT_Error **error) {
  FixtureInstanceState *state;
  (void)provider_context;
  (void)error;
  if (!module_state)
    return ON1X_RT_INVALID_STATE;
  state = (FixtureInstanceState *)on1x_rt_module_allocate(
      context, sizeof(*state), alignof(FixtureInstanceState));
  if (!state)
    return ON1X_RT_OUT_OF_MEMORY;
  state->bias = 1;
  *instance_state = state;
  ++fixture_counters.instance_create_calls;
  return ON1X_RT_OK;
}

static On1xRT_Status fixture_instance_destroy(On1xRT_ModuleContext *context,
                                              void *provider_context,
                                              void *module_state,
                                              void *instance_state,
                                              On1xRT_Error **error) {
  FixtureInstanceState *state = (FixtureInstanceState *)instance_state;
  (void)provider_context;
  (void)error;
  if (!module_state || !state || state->bias != 1)
    return ON1X_RT_INVALID_STATE;
  state->bias = 0;
  on1x_rt_module_release_memory(context, state, sizeof(*state),
                                alignof(FixtureInstanceState));
  ++fixture_counters.instance_destroy_calls;
  return ON1X_RT_OK;
}

static On1xRT_Status fixture_add(On1xRT_CallContext *call,
                                 void *function_context,
                                 const On1xRT_Value *arguments,
                                 size_t argument_count, On1xRT_Value *result,
                                 On1xRT_Error **error) {
  const FixtureInstanceState *state =
      (const FixtureInstanceState *)on1x_rt_call_instance_state(call);
  (void)function_context;
  (void)error;
  if (!state || argument_count != 2u)
    return ON1X_RT_INVALID_STATE;
  *result = on1x_rt_value_i64(arguments[0].as.i64 + arguments[1].as.i64 +
                              state->bias);
  ++fixture_counters.function_calls;
  return ON1X_RT_OK;
}

static On1xRT_Status fixture_mirror(On1xRT_CallContext *call,
                                    void *function_context,
                                    const On1xRT_Value *arguments,
                                    size_t argument_count, On1xRT_Value *result,
                                    On1xRT_Error **error) {
  On1xRT_BytesView view = {0};
  On1xRT_Buffer buffer = {0};
  On1xRT_Status status;
  (void)function_context;
  if (argument_count != 1u)
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_call_buffer_view(call, arguments[0].as.buffer, &view, error);
  if (status != ON1X_RT_OK)
    return status;
  status =
      on1x_rt_call_buffer_create(call, view.data, view.size, &buffer, error);
  if (status != ON1X_RT_OK)
    return status;
  *result = on1x_rt_value_buffer(buffer);
  ++fixture_counters.function_calls;
  return ON1X_RT_OK;
}

static On1xRT_Status
fixture_embedded_string(On1xRT_CallContext *call, void *function_context,
                        const On1xRT_Value *arguments, size_t argument_count,
                        On1xRT_Value *result, On1xRT_Error **error) {
  static const unsigned char text[] = {'A', 0, 'B'};
  On1xRT_String string = {0};
  On1xRT_Status status;
  (void)function_context;
  (void)arguments;
  if (argument_count)
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_call_string_create(call, text, sizeof(text), &string, error);
  if (status != ON1X_RT_OK)
    return status;
  *result = on1x_rt_value_string(string);
  ++fixture_counters.function_calls;
  return ON1X_RT_OK;
}

static On1xRT_Status
fixture_call_host(On1xRT_CallContext *call, void *function_context,
                  const On1xRT_Value *arguments, size_t argument_count,
                  On1xRT_Value *result, On1xRT_Error **error) {
  static const On1xRT_ValueKind parameters[] = {ON1X_RT_VALUE_I64};
  static const On1xRT_Signature signature = {
      ON1X_RT_ABI_VERSION, sizeof(On1xRT_Signature),
      parameters,          1u,
      ON1X_RT_VALUE_I64,   0u};
  static const On1xRT_StringView name = VIEW("double");
  (void)function_context;
  if (argument_count != 1u)
    return ON1X_RT_INVALID_ARGUMENT;
  ++fixture_counters.function_calls;
  return on1x_rt_call_host(call, name, &signature, arguments, argument_count,
                           result, error);
}

static On1xRT_Status fixture_fail(On1xRT_CallContext *call,
                                  void *function_context,
                                  const On1xRT_Value *arguments,
                                  size_t argument_count, On1xRT_Value *result,
                                  On1xRT_Error **error) {
  static const On1xRT_StringView message = VIEW("fixture failure");
  On1xRT_Status status;
  (void)function_context;
  (void)arguments;
  (void)result;
  if (argument_count)
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_call_error(call, 9001u, message, error);
  if (status != ON1X_RT_OK)
    return status;
  ++fixture_counters.function_calls;
  return ON1X_RT_ERROR;
}

static On1xRT_Status
fixture_reentry_status(On1xRT_CallContext *call, void *function_context,
                       const On1xRT_Value *arguments, size_t argument_count,
                       On1xRT_Value *result, On1xRT_Error **error) {
  On1xRT_Buffer buffer = {0};
  On1xRT_Error *nested_error = NULL;
  On1xRT_Status status;
  (void)call;
  (void)function_context;
  (void)arguments;
  (void)error;
  if (argument_count || !fixture_engine)
    return ON1X_RT_INVALID_STATE;
  status =
      on1x_rt_buffer_create(fixture_engine, NULL, 0u, &buffer, &nested_error);
  if (nested_error)
    (void)on1x_rt_error_release(&nested_error);
  *result = on1x_rt_value_i64((int64_t)status);
  ++fixture_counters.function_calls;
  return ON1X_RT_OK;
}

static const On1xRT_ValueKind add_parameters[] = {ON1X_RT_VALUE_I64,
                                                  ON1X_RT_VALUE_I64};
static const On1xRT_ValueKind buffer_parameters[] = {ON1X_RT_VALUE_BUFFER};
static const On1xRT_ValueKind integer_parameters[] = {ON1X_RT_VALUE_I64};

#define SIGNATURE(parameters_, count_, result_)                                \
  {ON1X_RT_ABI_VERSION,                                                        \
   sizeof(On1xRT_Signature),                                                   \
   parameters_,                                                                \
   count_,                                                                     \
   result_,                                                                    \
   0u}
#define EXPORT(name_, signature_, callback_)                                   \
  {ON1X_RT_ABI_VERSION,                                                        \
   sizeof(On1xRT_ExportDescriptor),                                            \
   VIEW(name_),                                                                \
   signature_,                                                                 \
   callback_,                                                                  \
   NULL,                                                                       \
   0u,                                                                         \
   0u}

static const On1xRT_ExportDescriptor fixture_exports[] = {
    EXPORT("add", SIGNATURE(add_parameters, 2u, ON1X_RT_VALUE_I64),
           fixture_add),
    EXPORT("mirror", SIGNATURE(buffer_parameters, 1u, ON1X_RT_VALUE_BUFFER),
           fixture_mirror),
    EXPORT("embedded", SIGNATURE(NULL, 0u, ON1X_RT_VALUE_STRING),
           fixture_embedded_string),
    EXPORT("host_double", SIGNATURE(integer_parameters, 1u, ON1X_RT_VALUE_I64),
           fixture_call_host),
    EXPORT("fail", SIGNATURE(NULL, 0u, ON1X_RT_VALUE_VOID), fixture_fail),
    EXPORT("reentry_status", SIGNATURE(NULL, 0u, ON1X_RT_VALUE_I64),
           fixture_reentry_status)};

static const On1xRT_ModuleDescriptor fixture_descriptor = {
    ON1X_RT_ABI_VERSION,
    sizeof(On1xRT_ModuleDescriptor),
    ON1X_RT_MODULE_DESCRIPTOR_VERSION,
    ON1X_RT_MODULE_STATIC,
    VIEW("fixture"),
    VIEW("1.0.0"),
    VIEW("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
    NULL,
    0u,
    fixture_exports,
    sizeof(fixture_exports) / sizeof(fixture_exports[0]),
    NULL,
    fixture_initialize,
    fixture_shutdown,
    fixture_instance_create,
    fixture_instance_destroy};

ON1X_RT_EXPORT const On1xRT_ModuleDescriptor *
ON1X_RT_MODULE_ENTRY_SYMBOL(void) {
  return &fixture_descriptor;
}
