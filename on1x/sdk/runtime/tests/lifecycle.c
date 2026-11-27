#include "fixture_module.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define VIEW(text) ((On1xRT_StringView){text, sizeof(text) - 1u})

static int failure(int line, const char *message) {
  fprintf(stderr, "lifecycle.c:%d: %s\n", line, message);
  return line;
}

#define CHECK(condition, message)                                              \
  do {                                                                         \
    if (!(condition))                                                          \
      return failure(__LINE__, message);                                       \
  } while (0)

static int release_error(On1xRT_Error **error) {
  if (!*error)
    return 0;
  return on1x_rt_error_release(error) == ON1X_RT_OK ? 0 : 1;
}

static On1xRT_Status host_double(On1xRT_CallContext *call,
                                 void *function_context,
                                 const On1xRT_Value *arguments,
                                 size_t argument_count, On1xRT_Value *result,
                                 On1xRT_Error **error) {
  unsigned *calls = (unsigned *)function_context;
  (void)error;
  if (argument_count != 1u || on1x_rt_call_module_state(call) != NULL ||
      on1x_rt_call_instance_state(call) != NULL)
    return ON1X_RT_INVALID_STATE;
  ++*calls;
  *result = on1x_rt_value_i64(arguments[0].as.i64 * 2);
  return ON1X_RT_OK;
}

static On1xRT_Function lookup(On1xRT_Engine *engine, On1xRT_Instance instance,
                              const char *name, size_t name_size,
                              const On1xRT_Signature *signature) {
  On1xRT_Function function = {0};
  On1xRT_Error *error = NULL;
  if (on1x_rt_instance_function(engine, instance,
                                (On1xRT_StringView){name, name_size}, signature,
                                &function, &error) != ON1X_RT_OK) {
    if (error) {
      On1xRT_StringView message = on1x_rt_error_message(error);
      fprintf(stderr, "lookup failed: %.*s\n", (int)message.size, message.data);
      (void)on1x_rt_error_release(&error);
    }
    return (On1xRT_Function){0};
  }
  return function;
}

int main(void) {
  static const On1xRT_ValueKind add_parameters[] = {ON1X_RT_VALUE_I64,
                                                    ON1X_RT_VALUE_I64};
  static const On1xRT_ValueKind buffer_parameters[] = {ON1X_RT_VALUE_BUFFER};
  static const On1xRT_ValueKind integer_parameters[] = {ON1X_RT_VALUE_I64};
  static const On1xRT_Signature add_signature = {
      ON1X_RT_ABI_VERSION, sizeof(On1xRT_Signature),
      add_parameters,      2u,
      ON1X_RT_VALUE_I64,   0u};
  static const On1xRT_Signature buffer_signature = {
      ON1X_RT_ABI_VERSION,  sizeof(On1xRT_Signature),
      buffer_parameters,    1u,
      ON1X_RT_VALUE_BUFFER, 0u};
  static const On1xRT_Signature integer_signature = {
      ON1X_RT_ABI_VERSION, sizeof(On1xRT_Signature),
      integer_parameters,  1u,
      ON1X_RT_VALUE_I64,   0u};
  static const On1xRT_Signature string_signature = {ON1X_RT_ABI_VERSION,
                                                    sizeof(On1xRT_Signature),
                                                    NULL,
                                                    0u,
                                                    ON1X_RT_VALUE_STRING,
                                                    0u};
  static const On1xRT_Signature void_signature = {ON1X_RT_ABI_VERSION,
                                                  sizeof(On1xRT_Signature),
                                                  NULL,
                                                  0u,
                                                  ON1X_RT_VALUE_VOID,
                                                  0u};
  static const On1xRT_Signature noarg_integer_signature = {
      ON1X_RT_ABI_VERSION,
      sizeof(On1xRT_Signature),
      NULL,
      0u,
      ON1X_RT_VALUE_I64,
      0u};
  On1xRT_Engine *engine = NULL;
  On1xRT_Engine *other_engine = NULL;
  On1xRT_Error *error = NULL;
  On1xRT_Module module = {0};
  On1xRT_Instance instance = {0};
  On1xRT_HostFunction host = {0};
  On1xRT_Function function = {0};
  On1xRT_Value result = on1x_rt_value_void();
  unsigned host_calls = 0;
  char copied_name[] = "fixture";
  On1xRT_ModuleDescriptor descriptor = *ON1X_RT_MODULE_ENTRY_SYMBOL();
  const On1xRT_HostFunctionDescriptor host_descriptor = {
      ON1X_RT_ABI_VERSION,
      sizeof(On1xRT_HostFunctionDescriptor),
      VIEW("double"),
      integer_signature,
      host_double,
      &host_calls,
      0u,
      0u};

  on1x_rt_test_fixture_reset();
  descriptor.name = (On1xRT_StringView){copied_name, sizeof(copied_name) - 1u};
  CHECK(on1x_rt_module_descriptor_check(&descriptor, &error) == ON1X_RT_OK,
        "valid module descriptor rejected");
  CHECK(error == NULL, "descriptor check produced an error on success");
  CHECK(on1x_rt_engine_create(NULL, &engine, &error) == ON1X_RT_OK,
        "engine creation failed");
  CHECK(error == NULL && engine != NULL, "invalid engine create outputs");
  on1x_rt_test_fixture_set_engine(engine);
  CHECK(on1x_rt_engine_register_host_function(engine, &host_descriptor, &host,
                                              &error) == ON1X_RT_OK,
        "host function registration failed");
  CHECK(on1x_rt_engine_register_static_module(engine, &descriptor, &module,
                                              &error) == ON1X_RT_OK,
        "static module registration failed");
  memset(copied_name, 'X', sizeof(copied_name) - 1u);
  CHECK(on1x_rt_instance_create(engine, module, &instance, &error) ==
            ON1X_RT_OK,
        "instance creation failed");

  {
    const On1xRT_Value arguments[] = {on1x_rt_value_i64(3),
                                      on1x_rt_value_i64(4)};
    function = lookup(engine, instance, "add", 3u, &add_signature);
    CHECK(function.id != 0u, "add lookup failed");
    CHECK(on1x_rt_function_call(engine, function, arguments, 2u, &result,
                                &error) == ON1X_RT_OK,
          "add call failed");
    CHECK(result.kind == ON1X_RT_VALUE_I64 && result.as.i64 == 8,
          "add result mismatch");
    CHECK(on1x_rt_value_release(engine, &result, &error) == ON1X_RT_OK,
          "scalar result release failed");
    CHECK(on1x_rt_function_release(engine, &function, &error) == ON1X_RT_OK,
          "add function release failed");
  }

  {
    static const unsigned char input_bytes[] = {1u, 0u, 2u, 255u};
    On1xRT_Buffer input = {0};
    On1xRT_BytesView view = {0};
    On1xRT_Value argument;
    function = lookup(engine, instance, "mirror", 6u, &buffer_signature);
    CHECK(function.id != 0u, "mirror lookup failed");
    CHECK(on1x_rt_buffer_create(engine, input_bytes, sizeof(input_bytes),
                                &input, &error) == ON1X_RT_OK,
          "input buffer creation failed");
    argument = on1x_rt_value_buffer(input);
    CHECK(on1x_rt_function_call(engine, function, &argument, 1u, &result,
                                &error) == ON1X_RT_OK,
          "mirror call failed");
    CHECK(result.kind == ON1X_RT_VALUE_BUFFER, "mirror returned wrong kind");
    CHECK(on1x_rt_buffer_view(engine, result.as.buffer, &view, &error) ==
              ON1X_RT_OK,
          "mirror view failed");
    CHECK(view.size == sizeof(input_bytes) &&
              !memcmp(view.data, input_bytes, sizeof(input_bytes)),
          "mirror bytes mismatch");
    CHECK(on1x_rt_value_release(engine, &result, &error) == ON1X_RT_OK,
          "mirror result release failed");
    CHECK(on1x_rt_buffer_release(engine, &input, &error) == ON1X_RT_OK,
          "input buffer release failed");
    CHECK(on1x_rt_function_release(engine, &function, &error) == ON1X_RT_OK,
          "mirror function release failed");
  }

  function = lookup(engine, instance, "embedded", 8u, &string_signature);
  CHECK(function.id != 0u, "embedded lookup failed");
  CHECK(on1x_rt_function_call(engine, function, NULL, 0u, &result, &error) ==
            ON1X_RT_OK,
        "embedded string call failed");
  {
    On1xRT_BytesView view = {0};
    static const unsigned char expected[] = {'A', 0, 'B'};
    CHECK(on1x_rt_string_view(engine, result.as.string, &view, &error) ==
              ON1X_RT_OK,
          "embedded string view failed");
    CHECK(view.size == sizeof(expected) &&
              !memcmp(view.data, expected, sizeof(expected)),
          "embedded NUL string mismatch");
  }
  CHECK(on1x_rt_value_release(engine, &result, &error) == ON1X_RT_OK,
        "embedded string release failed");
  CHECK(on1x_rt_function_release(engine, &function, &error) == ON1X_RT_OK,
        "embedded function release failed");

  {
    const On1xRT_Value argument = on1x_rt_value_i64(9);
    function = lookup(engine, instance, "host_double", 11u, &integer_signature);
    CHECK(function.id != 0u, "host_double lookup failed");
    CHECK(on1x_rt_function_call(engine, function, &argument, 1u, &result,
                                &error) == ON1X_RT_OK,
          "host callback call failed");
    CHECK(result.kind == ON1X_RT_VALUE_I64 && result.as.i64 == 18 &&
              host_calls == 1u,
          "host callback result mismatch");
    CHECK(on1x_rt_function_release(engine, &function, &error) == ON1X_RT_OK,
          "host_double release failed");
  }

  function =
      lookup(engine, instance, "reentry_status", 14u, &noarg_integer_signature);
  CHECK(function.id != 0u, "reentry_status lookup failed");
  CHECK(on1x_rt_function_call(engine, function, NULL, 0u, &result, &error) ==
            ON1X_RT_OK,
        "reentry status call failed");
  CHECK(result.as.i64 == (int64_t)ON1X_RT_REENTRANT,
        "public runtime API was not rejected during callback");
  CHECK(on1x_rt_function_release(engine, &function, &error) == ON1X_RT_OK,
        "reentry function release failed");

  function = lookup(engine, instance, "fail", 4u, &void_signature);
  CHECK(function.id != 0u, "fail lookup failed");
  CHECK(on1x_rt_function_call(engine, function, NULL, 0u, &result, &error) ==
            ON1X_RT_ERROR,
        "failing callback status was not preserved");
  CHECK(error != NULL && on1x_rt_error_code(error) == 9001u,
        "callback error detail was not preserved");
  CHECK(release_error(&error) == 0, "callback error release failed");
  CHECK(on1x_rt_function_release(engine, &function, &error) == ON1X_RT_OK,
        "fail function release failed");

  function = lookup(engine, instance, "add", 3u, &add_signature);
  CHECK(function.id != 0u, "second add lookup failed");
  {
    On1xRT_Function stale = function;
    On1xRT_Function wrong = {instance.id, instance.owner};
    On1xRT_Value arguments[] = {on1x_rt_value_i64(1), on1x_rt_value_i64(2)};
    CHECK(on1x_rt_instance_release(engine, &instance, &error) == ON1X_RT_BUSY,
          "instance released while function was live");
    CHECK(error && on1x_rt_error_code(error) == ON1X_RT_BUSY,
          "busy instance error missing");
    CHECK(release_error(&error) == 0, "busy error release failed");
    CHECK(on1x_rt_function_call(engine, wrong, arguments, 2u, &result,
                                &error) == ON1X_RT_WRONG_KIND,
          "wrong-kind handle was not rejected");
    CHECK(release_error(&error) == 0, "wrong-kind error release failed");
    CHECK(on1x_rt_function_release(engine, &function, &error) == ON1X_RT_OK,
          "second add release failed");
    CHECK(on1x_rt_function_call(engine, stale, arguments, 2u, &result,
                                &error) == ON1X_RT_STALE_HANDLE,
          "stale generation was not rejected");
    CHECK(release_error(&error) == 0, "stale error release failed");
  }

  CHECK(on1x_rt_module_unload(engine, &module, &error) == ON1X_RT_BUSY,
        "module unloaded while instance was live");
  CHECK(release_error(&error) == 0, "module busy error release failed");
  CHECK(on1x_rt_engine_create(NULL, &other_engine, &error) == ON1X_RT_OK,
        "second engine creation failed");
  {
    On1xRT_Function foreign = {instance.id, instance.owner};
    CHECK(on1x_rt_function_call(other_engine, foreign, NULL, 0u, &result,
                                &error) == ON1X_RT_WRONG_OWNER,
          "cross-engine owner mismatch was not rejected");
    CHECK(release_error(&error) == 0, "wrong-owner error release failed");
  }
  CHECK(on1x_rt_engine_destroy(&other_engine, &error) == ON1X_RT_OK,
        "second engine destruction failed");
  CHECK(on1x_rt_engine_destroy(&engine, &error) == ON1X_RT_BUSY,
        "engine destroyed with live objects");
  CHECK(release_error(&error) == 0, "engine busy error release failed");

  CHECK(on1x_rt_instance_release(engine, &instance, &error) == ON1X_RT_OK,
        "instance release failed");
  CHECK(on1x_rt_module_unload(engine, &module, &error) == ON1X_RT_OK,
        "module unload failed");
  CHECK(on1x_rt_host_function_unregister(engine, &host, &error) == ON1X_RT_OK,
        "host function unregister failed");
  CHECK(on1x_rt_engine_destroy(&engine, &error) == ON1X_RT_OK,
        "engine destruction failed");
  CHECK(engine == NULL, "engine pointer not cleared");
  {
    const On1xRT_TestFixtureCounters counters = on1x_rt_test_fixture_counters();
    CHECK(counters.initialize_calls == 1u && counters.shutdown_calls == 1u &&
              counters.instance_create_calls == 1u &&
              counters.instance_destroy_calls == 1u &&
              counters.function_calls == 6u,
          "fixture lifecycle counts mismatch");
  }
  return 0;
}
