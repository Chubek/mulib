#include "fixture_module.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#define TEXT(text) ((On1xRT_StringView){text, sizeof(text) - 1u})
#define TEXT_INIT(text) {text, sizeof(text) - 1u}

static int failure(int line, const char *message) {
  fprintf(stderr, "negative.c:%d: %s\n", line, message);
  return line;
}

#define CHECK(condition, message)                                              \
  do {                                                                         \
    if (!(condition))                                                          \
      return failure(__LINE__, message);                                       \
  } while (0)

static void clear_error(On1xRT_Error **error) {
  if (*error)
    (void)on1x_rt_error_release(error);
}

typedef struct TestProvider {
  char tag;
  unsigned fail_initialize;
  unsigned unknown_initialize;
  unsigned fail_shutdown;
  unsigned fail_instance_destroy;
} TestProvider;

static char lifecycle_log[128];
static size_t lifecycle_log_size;

static void log_tag(char tag) {
  if (lifecycle_log_size < sizeof(lifecycle_log))
    lifecycle_log[lifecycle_log_size++] = tag;
}

static On1xRT_Status test_initialize(On1xRT_ModuleContext *context,
                                     void *provider_context,
                                     void **module_state,
                                     On1xRT_Error **error) {
  TestProvider *provider = (TestProvider *)provider_context;
  (void)context;
  (void)error;
  log_tag(provider->tag);
  if (provider->unknown_initialize)
    return (On1xRT_Status)999u;
  if (provider->fail_initialize)
    return ON1X_RT_ERROR;
  *module_state = provider;
  return ON1X_RT_OK;
}

static On1xRT_Status test_shutdown(On1xRT_ModuleContext *context,
                                   void *provider_context, void *module_state,
                                   On1xRT_Error **error) {
  TestProvider *provider = (TestProvider *)provider_context;
  (void)context;
  (void)module_state;
  (void)error;
  if (provider->fail_shutdown) {
    --provider->fail_shutdown;
    return ON1X_RT_ERROR;
  }
  log_tag((char)(provider->tag >= 'A' && provider->tag <= 'Z'
                     ? provider->tag + ('a' - 'A')
                     : provider->tag));
  return ON1X_RT_OK;
}

static On1xRT_Status test_instance_create(On1xRT_ModuleContext *context,
                                          void *provider_context,
                                          void *module_state,
                                          void **instance_state,
                                          On1xRT_Error **error) {
  (void)context;
  (void)module_state;
  (void)error;
  *instance_state = provider_context;
  return ON1X_RT_OK;
}

static On1xRT_Status test_instance_destroy(On1xRT_ModuleContext *context,
                                           void *provider_context,
                                           void *module_state,
                                           void *instance_state,
                                           On1xRT_Error **error) {
  TestProvider *provider = (TestProvider *)provider_context;
  (void)context;
  (void)module_state;
  (void)instance_state;
  (void)error;
  if (provider->fail_instance_destroy) {
    --provider->fail_instance_destroy;
    return ON1X_RT_ERROR;
  }
  return ON1X_RT_OK;
}

static On1xRT_Status noop_callback(On1xRT_CallContext *call,
                                   void *function_context,
                                   const On1xRT_Value *arguments,
                                   size_t argument_count, On1xRT_Value *result,
                                   On1xRT_Error **error) {
  (void)call;
  (void)function_context;
  (void)arguments;
  (void)error;
  if (argument_count)
    return ON1X_RT_INVALID_ARGUMENT;
  *result = on1x_rt_value_void();
  return ON1X_RT_OK;
}

static On1xRT_Status
bad_output_callback(On1xRT_CallContext *call, void *function_context,
                    const On1xRT_Value *arguments, size_t argument_count,
                    On1xRT_Value *result, On1xRT_Error **error) {
  On1xRT_String pending = {0};
  static const char bytes[] = "pending";
  (void)function_context;
  (void)arguments;
  if (argument_count)
    return ON1X_RT_INVALID_ARGUMENT;
  if (on1x_rt_call_string_create(call, bytes, sizeof(bytes) - 1u, &pending,
                                 error) != ON1X_RT_OK)
    return ON1X_RT_OUT_OF_MEMORY;
  *result = on1x_rt_value_i64(1);
  return ON1X_RT_OK;
}

static const On1xRT_Signature void_signature = {ON1X_RT_ABI_VERSION,
                                                sizeof(On1xRT_Signature),
                                                NULL,
                                                0u,
                                                ON1X_RT_VALUE_VOID,
                                                0u};

static const On1xRT_Signature integer_signature = {ON1X_RT_ABI_VERSION,
                                                   sizeof(On1xRT_Signature),
                                                   NULL,
                                                   0u,
                                                   ON1X_RT_VALUE_I64,
                                                   0u};

static const On1xRT_ExportDescriptor noop_export = {
    ON1X_RT_ABI_VERSION,
    sizeof(On1xRT_ExportDescriptor),
    TEXT_INIT("noop"),
    {ON1X_RT_ABI_VERSION, sizeof(On1xRT_Signature), NULL, 0u,
     ON1X_RT_VALUE_VOID, 0u},
    noop_callback,
    NULL,
    0u,
    0u};

static const On1xRT_ExportDescriptor bad_export = {
    ON1X_RT_ABI_VERSION,
    sizeof(On1xRT_ExportDescriptor),
    TEXT_INIT("bad"),
    {ON1X_RT_ABI_VERSION, sizeof(On1xRT_Signature), NULL, 0u,
     ON1X_RT_VALUE_VOID, 0u},
    bad_output_callback,
    NULL,
    0u,
    0u};

static On1xRT_ModuleDescriptor
descriptor(const char *name, uint32_t flags,
           const On1xRT_StringView *dependencies, size_t dependency_count,
           const On1xRT_ExportDescriptor *exported, TestProvider *provider,
           int instances) {
  On1xRT_ModuleDescriptor value;
  memset(&value, 0, sizeof(value));
  value.abi_version = ON1X_RT_ABI_VERSION;
  value.struct_size = sizeof(value);
  value.descriptor_version = ON1X_RT_MODULE_DESCRIPTOR_VERSION;
  value.flags = flags;
  value.name = (On1xRT_StringView){name, strlen(name)};
  value.revision = TEXT("1");
  value.identity_sha256 =
      TEXT("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  value.dependencies = dependencies;
  value.dependency_count = dependency_count;
  value.exports = exported ? exported : &noop_export;
  value.export_count = 1u;
  value.provider_context = provider;
  if (provider) {
    value.initialize = test_initialize;
    value.shutdown = test_shutdown;
  }
  if (instances) {
    value.instance_create = test_instance_create;
    value.instance_destroy = test_instance_destroy;
  }
  return value;
}

static On1xRT_Status authorize_aot(void *context,
                                   const On1xRT_ModuleDescriptor *module) {
  unsigned *calls = (unsigned *)context;
  ++*calls;
  return module && module->flags == ON1X_RT_MODULE_AUTHORIZED_AOT
             ? ON1X_RT_OK
             : ON1X_RT_DENIED;
}

static int test_descriptors(void) {
  On1xRT_Error *error = NULL;
  On1xRT_ModuleDescriptor value =
      descriptor("valid", ON1X_RT_MODULE_STATIC, NULL, 0u, NULL, NULL, 0);
  CHECK(on1x_rt_module_descriptor_check(&value, &error) == ON1X_RT_OK,
        "valid descriptor rejected");
  value.abi_version = 99u;
  CHECK(on1x_rt_module_descriptor_check(&value, &error) == ON1X_RT_ABI_MISMATCH,
        "module ABI mismatch not distinguished");
  clear_error(&error);
  value.abi_version = ON1X_RT_ABI_VERSION;
  value.flags = ON1X_RT_MODULE_STATIC | ON1X_RT_MODULE_AUTHORIZED_AOT;
  CHECK(on1x_rt_module_descriptor_check(&value, &error) ==
            ON1X_RT_INVALID_ARGUMENT,
        "ambiguous trust profile accepted");
  clear_error(&error);
  value.flags = ON1X_RT_MODULE_STATIC;
  value.dependency_count = ON1X_RT_MAX_DEPENDENCIES + 1u;
  CHECK(on1x_rt_module_descriptor_check(&value, &error) ==
            ON1X_RT_INVALID_ARGUMENT,
        "oversized dependency list accepted");
  clear_error(&error);
  value.dependency_count = 0u;
  value.initialize = test_initialize;
  value.shutdown = NULL;
  CHECK(on1x_rt_module_descriptor_check(&value, &error) ==
            ON1X_RT_INVALID_ARGUMENT,
        "unpaired module lifecycle accepted");
  clear_error(&error);
  {
    const unsigned char invalid_utf8[] = {0xc0u, 0x80u};
    value.initialize = NULL;
    value.name =
        (On1xRT_StringView){(const char *)invalid_utf8, sizeof(invalid_utf8)};
    CHECK(on1x_rt_module_descriptor_check(&value, &error) ==
              ON1X_RT_INVALID_ARGUMENT,
          "malformed UTF-8 module name accepted");
    clear_error(&error);
  }
  value = descriptor("version", ON1X_RT_MODULE_STATIC, NULL, 0u, NULL, NULL, 0);
  value.descriptor_version = ON1X_RT_MODULE_DESCRIPTOR_VERSION + 1u;
  CHECK(on1x_rt_module_descriptor_check(&value, &error) == ON1X_RT_ABI_MISMATCH,
        "module descriptor version mismatch not distinguished");
  clear_error(&error);
  {
    On1xRT_ExportDescriptor incompatible_export = noop_export;
    incompatible_export.signature.abi_version = ON1X_RT_ABI_VERSION + 1u;
    value = descriptor("signature_version", ON1X_RT_MODULE_STATIC, NULL, 0u,
                       &incompatible_export, NULL, 0);
    CHECK(on1x_rt_module_descriptor_check(&value, &error) ==
              ON1X_RT_ABI_MISMATCH,
          "nested signature ABI mismatch not distinguished");
    clear_error(&error);
  }
  {
    const uint32_t prefix_size =
        (uint32_t)(offsetof(On1xRT_ModuleDescriptor, export_count) +
                   sizeof(value.export_count));
    unsigned char *prefix = (unsigned char *)malloc((size_t)prefix_size);
    On1xRT_Engine *engine = NULL;
    On1xRT_Module module = {0};
    value = descriptor("short", ON1X_RT_MODULE_STATIC, NULL, 0u, NULL, NULL, 0);
    value.struct_size = prefix_size;
    CHECK(prefix != NULL, "short descriptor allocation failed");
    memcpy(prefix, &value, (size_t)prefix_size);
    CHECK(on1x_rt_engine_create(NULL, &engine, &error) == ON1X_RT_OK,
          "short descriptor engine creation failed");
    CHECK(on1x_rt_engine_register_static_module(
              engine, (const On1xRT_ModuleDescriptor *)(const void *)prefix,
              &module, &error) == ON1X_RT_OK,
          "valid short module descriptor prefix rejected");
    memset(prefix, 0, (size_t)prefix_size);
    free(prefix);
    CHECK(on1x_rt_module_unload(engine, &module, &error) == ON1X_RT_OK,
          "short descriptor module unload failed");
    CHECK(on1x_rt_engine_destroy(&engine, &error) == ON1X_RT_OK,
          "short descriptor engine destruction failed");
  }
  return 0;
}

static int test_dependencies(void) {
  On1xRT_Engine *engine = NULL;
  On1xRT_Error *error = NULL;
  On1xRT_Module modules[2] = {{0}, {0}};
  TestProvider provider_a = {'A', 0u, 0u, 0u, 0u};
  TestProvider provider_b = {'B', 0u, 0u, 0u, 0u};
  const On1xRT_StringView dependency_b[] = {TEXT("dep_b")};
  On1xRT_ModuleDescriptor module_a = descriptor(
      "dep_a", ON1X_RT_MODULE_STATIC, dependency_b, 1u, NULL, &provider_a, 0);
  On1xRT_ModuleDescriptor module_b = descriptor("dep_b", ON1X_RT_MODULE_STATIC,
                                                NULL, 0u, NULL, &provider_b, 0);
  const On1xRT_ModuleDescriptor *batch[] = {&module_a, &module_b};
  lifecycle_log_size = 0u;
  CHECK(on1x_rt_engine_create(NULL, &engine, &error) == ON1X_RT_OK,
        "dependency engine creation failed");
  CHECK(on1x_rt_engine_register_static_modules(engine, batch, 2u, modules, 2u,
                                               &error) == ON1X_RT_OK,
        "dependency batch registration failed");
  CHECK(lifecycle_log_size == 2u && lifecycle_log[0] == 'B' &&
            lifecycle_log[1] == 'A',
        "dependencies were not initialized first");
  CHECK(on1x_rt_module_unload(engine, &modules[1], &error) == ON1X_RT_BUSY,
        "dependency unloaded while dependent was live");
  clear_error(&error);
  CHECK(on1x_rt_module_unload(engine, &modules[0], &error) == ON1X_RT_OK,
        "dependent unload failed");
  CHECK(on1x_rt_module_unload(engine, &modules[1], &error) == ON1X_RT_OK,
        "dependency unload failed");
  CHECK(on1x_rt_engine_destroy(&engine, &error) == ON1X_RT_OK,
        "dependency engine destruction failed");
  return 0;
}

static int test_cycles_and_rollback(void) {
  On1xRT_Engine *engine = NULL;
  On1xRT_Error *error = NULL;
  On1xRT_Module modules[2] = {{0}, {0}};
  TestProvider provider_c = {'C', 0u, 0u, 0u, 0u};
  TestProvider provider_d = {'D', 0u, 0u, 0u, 0u};
  const On1xRT_StringView dependency_c[] = {TEXT("cycle_c")};
  const On1xRT_StringView dependency_d[] = {TEXT("cycle_d")};
  On1xRT_ModuleDescriptor module_c = descriptor(
      "cycle_c", ON1X_RT_MODULE_STATIC, dependency_d, 1u, NULL, &provider_c, 0);
  On1xRT_ModuleDescriptor module_d = descriptor(
      "cycle_d", ON1X_RT_MODULE_STATIC, dependency_c, 1u, NULL, &provider_d, 0);
  const On1xRT_ModuleDescriptor *cycle[] = {&module_c, &module_d};
  CHECK(on1x_rt_engine_create(NULL, &engine, &error) == ON1X_RT_OK,
        "cycle engine creation failed");
  lifecycle_log_size = 0u;
  CHECK(on1x_rt_engine_register_static_modules(
            engine, cycle, 2u, modules, 2u, &error) == ON1X_RT_INVALID_ARGUMENT,
        "cyclic dependency transaction accepted");
  CHECK(!modules[0].id && !modules[1].id && lifecycle_log_size == 0u,
        "cyclic transaction published or executed modules");
  clear_error(&error);
  {
    TestProvider provider_e = {'E', 1u, 0u, 0u, 0u};
    TestProvider provider_f = {'F', 0u, 0u, 0u, 0u};
    const On1xRT_StringView dependency_f[] = {TEXT("roll_f")};
    On1xRT_ModuleDescriptor module_e =
        descriptor("roll_e", ON1X_RT_MODULE_STATIC, dependency_f, 1u, NULL,
                   &provider_e, 0);
    On1xRT_ModuleDescriptor module_f = descriptor(
        "roll_f", ON1X_RT_MODULE_STATIC, NULL, 0u, NULL, &provider_f, 0);
    const On1xRT_ModuleDescriptor *rollback[] = {&module_e, &module_f};
    lifecycle_log_size = 0u;
    CHECK(on1x_rt_engine_register_static_modules(engine, rollback, 2u, modules,
                                                 2u, &error) == ON1X_RT_ERROR,
          "failing initialization status not preserved");
    CHECK(!modules[0].id && !modules[1].id && lifecycle_log_size == 3u &&
              lifecycle_log[0] == 'F' && lifecycle_log[1] == 'E' &&
              lifecycle_log[2] == 'f',
          "registration rollback did not compensate in reverse");
    clear_error(&error);
  }
  {
    TestProvider provider_u = {'U', 0u, 1u, 0u, 0u};
    On1xRT_ModuleDescriptor unknown = descriptor(
        "unknown", ON1X_RT_MODULE_STATIC, NULL, 0u, NULL, &provider_u, 0);
    CHECK(on1x_rt_engine_register_static_module(engine, &unknown, &modules[0],
                                                &error) == ON1X_RT_ERROR,
          "unknown callback status was not normalized");
    CHECK(!modules[0].id, "unknown callback status published module");
    clear_error(&error);
  }
  CHECK(on1x_rt_engine_destroy(&engine, &error) == ON1X_RT_OK,
        "rollback engine destruction failed");
  return 0;
}

static int test_recoverable_lifecycle(void) {
  On1xRT_Engine *engine = NULL;
  On1xRT_Error *error = NULL;
  On1xRT_Module module = {0};
  On1xRT_Instance instance = {0};
  TestProvider provider = {'G', 0u, 0u, 1u, 1u};
  On1xRT_ModuleDescriptor value = descriptor("recover", ON1X_RT_MODULE_STATIC,
                                             NULL, 0u, NULL, &provider, 1);
  CHECK(on1x_rt_engine_create(NULL, &engine, &error) == ON1X_RT_OK,
        "recoverable engine creation failed");
  CHECK(on1x_rt_engine_register_static_module(engine, &value, &module,
                                              &error) == ON1X_RT_OK,
        "recoverable module registration failed");
  CHECK(on1x_rt_instance_create(engine, module, &instance, &error) ==
            ON1X_RT_OK,
        "recoverable instance creation failed");
  CHECK(on1x_rt_instance_release(engine, &instance, &error) == ON1X_RT_ERROR,
        "instance destroy failure was not surfaced");
  CHECK(instance.id != 0u, "failed instance destroy invalidated handle");
  clear_error(&error);
  CHECK(on1x_rt_instance_release(engine, &instance, &error) == ON1X_RT_OK,
        "instance destroy retry failed");
  CHECK(on1x_rt_module_unload(engine, &module, &error) == ON1X_RT_ERROR,
        "module shutdown failure was not surfaced");
  CHECK(module.id != 0u, "failed shutdown invalidated module handle");
  clear_error(&error);
  CHECK(on1x_rt_module_unload(engine, &module, &error) == ON1X_RT_OK,
        "module shutdown retry failed");
  CHECK(on1x_rt_engine_destroy(&engine, &error) == ON1X_RT_OK,
        "recoverable engine destruction failed");
  return 0;
}

static int test_aot_authorization(void) {
  On1xRT_Engine *engine = NULL;
  On1xRT_Error *error = NULL;
  On1xRT_Module module = {0};
  unsigned authorization_calls = 0u;
  On1xRT_ModuleDescriptor value =
      descriptor("aot", ON1X_RT_MODULE_AUTHORIZED_AOT, NULL, 0u, NULL, NULL, 0);
  CHECK(on1x_rt_engine_create(NULL, &engine, &error) == ON1X_RT_OK,
        "unauthorized AOT engine creation failed");
  CHECK(on1x_rt_engine_register_aot_module(engine, &value, &module, &error) ==
            ON1X_RT_DENIED,
        "AOT module accepted without authorization callback");
  CHECK(!module.id, "denied AOT module was published");
  clear_error(&error);
  CHECK(on1x_rt_engine_destroy(&engine, &error) == ON1X_RT_OK,
        "unauthorized AOT engine destruction failed");
  {
    On1xRT_EngineConfig config;
    memset(&config, 0, sizeof(config));
    config.abi_version = ON1X_RT_ABI_VERSION;
    config.struct_size = sizeof(config);
    config.authorization_context = &authorization_calls;
    config.authorize_aot = authorize_aot;
    CHECK(on1x_rt_engine_create(&config, &engine, &error) == ON1X_RT_OK,
          "authorized AOT engine creation failed");
  }
  {
    const On1xRT_StringView missing[] = {TEXT("missing")};
    On1xRT_ModuleDescriptor unresolved =
        descriptor("unresolved_aot", ON1X_RT_MODULE_AUTHORIZED_AOT, missing, 1u,
                   NULL, NULL, 0);
    CHECK(on1x_rt_engine_register_aot_module(engine, &unresolved, &module,
                                             &error) == ON1X_RT_NOT_FOUND,
          "AOT module with unresolved dependency was not rejected");
    CHECK(authorization_calls == 0u && !module.id,
          "authorization ran before dependency resolution");
    clear_error(&error);
  }
  CHECK(on1x_rt_engine_register_aot_module(engine, &value, &module, &error) ==
            ON1X_RT_OK,
        "authorized AOT module rejected");
  CHECK(authorization_calls == 1u, "AOT authorization not invoked once");
  CHECK(on1x_rt_module_unload(engine, &module, &error) == ON1X_RT_OK,
        "authorized AOT unload failed");
  CHECK(on1x_rt_engine_destroy(&engine, &error) == ON1X_RT_OK,
        "authorized AOT engine destruction failed");
  return 0;
}

typedef struct ThreadAttempt {
  On1xRT_Engine *engine;
  On1xRT_Status status;
  int had_error;
} ThreadAttempt;

static int wrong_thread(void *opaque) {
  ThreadAttempt *attempt = (ThreadAttempt *)opaque;
  On1xRT_Buffer buffer = {0};
  On1xRT_Error *error = NULL;
  attempt->status =
      on1x_rt_buffer_create(attempt->engine, NULL, 0u, &buffer, &error);
  attempt->had_error = error != NULL;
  if (error)
    (void)on1x_rt_error_release(&error);
  return 0;
}

static int test_limits_threads_and_values(void) {
  On1xRT_Engine *engine = NULL;
  On1xRT_Error *error = NULL;
  On1xRT_Module module = {0};
  On1xRT_Instance instance = {0};
  On1xRT_Function function = {0};
  On1xRT_Buffer buffer = {0};
  On1xRT_Buffer extra = {0};
  On1xRT_ModuleDescriptor value =
      descriptor("limited", ON1X_RT_MODULE_STATIC, NULL, 0u, NULL, NULL, 0);
  On1xRT_EngineConfig config;
  thrd_t thread;
  ThreadAttempt attempt;
  memset(&config, 0, sizeof(config));
  config.abi_version = ON1X_RT_ABI_VERSION;
  config.struct_size = sizeof(config);
  config.max_handles = 2u;
  config.max_modules = 1u;
  config.max_host_functions = 1u;
  config.max_value_bytes = 4u;
  CHECK(on1x_rt_engine_create(&config, &engine, &error) == ON1X_RT_OK,
        "limited engine creation failed");
  {
    const On1xRT_ModuleDescriptor *descriptors[] = {&value};
    On1xRT_Module output = {UINT64_MAX, UINT64_MAX};
    CHECK(on1x_rt_engine_register_static_modules(engine, descriptors, 65537u,
                                                 &output, 65537u,
                                                 &error) == ON1X_RT_LIMIT,
          "oversized registration batch was traversed");
    CHECK(output.id == UINT64_MAX && output.owner == UINT64_MAX,
          "over-limit registration touched an unbounded output array");
    clear_error(&error);
  }
  attempt = (ThreadAttempt){engine, ON1X_RT_OK, 0};
  CHECK(thrd_create(&thread, wrong_thread, &attempt) == thrd_success,
        "thread creation failed");
  CHECK(thrd_join(thread, NULL) == thrd_success, "thread join failed");
  CHECK(attempt.status == ON1X_RT_WRONG_THREAD && !attempt.had_error,
        "wrong-thread access did not fail closed");
  CHECK(on1x_rt_buffer_create(engine, "1234", 4u, &buffer, &error) ==
            ON1X_RT_OK,
        "bounded buffer creation failed");
  CHECK(on1x_rt_buffer_create(engine, "x", 1u, &extra, &error) == ON1X_RT_LIMIT,
        "aggregate byte limit not enforced");
  clear_error(&error);
  CHECK(on1x_rt_buffer_write(engine, buffer, 4u, "x", 1u, &error) ==
            ON1X_RT_INVALID_ARGUMENT,
        "out-of-bounds buffer write accepted");
  clear_error(&error);
  CHECK(on1x_rt_buffer_release(engine, &buffer, &error) == ON1X_RT_OK,
        "bounded buffer release failed");
  {
    const unsigned char malformed[] = {0xc0u, 0x80u};
    On1xRT_String string = {0};
    CHECK(on1x_rt_string_create(engine, malformed, sizeof(malformed), &string,
                                &error) == ON1X_RT_INVALID_ARGUMENT,
          "malformed UTF-8 runtime string accepted");
    clear_error(&error);
  }
  CHECK(on1x_rt_engine_register_static_module(engine, &value, &module,
                                              &error) == ON1X_RT_OK,
        "limited module registration failed");
  CHECK(on1x_rt_instance_create(engine, module, &instance, &error) ==
            ON1X_RT_OK,
        "limited instance creation failed");
  CHECK(on1x_rt_instance_function(engine, instance, TEXT("noop"),
                                  &void_signature, &function,
                                  &error) == ON1X_RT_LIMIT,
        "handle limit not enforced at function lookup");
  clear_error(&error);
  CHECK(on1x_rt_instance_release(engine, &instance, &error) == ON1X_RT_OK,
        "limited instance release failed");
  CHECK(on1x_rt_module_unload(engine, &module, &error) == ON1X_RT_OK,
        "limited module unload failed");
  CHECK(on1x_rt_engine_destroy(&engine, &error) == ON1X_RT_OK,
        "limited engine destruction failed");
  {
    On1xRT_EngineConfig short_config;
    memset(&short_config, 0, sizeof(short_config));
    short_config.abi_version = ON1X_RT_ABI_VERSION;
    short_config.struct_size =
        (uint32_t)(offsetof(On1xRT_EngineConfig, struct_size) +
                   sizeof(short_config.struct_size));
    CHECK(on1x_rt_engine_create(&short_config, &engine, &error) == ON1X_RT_OK,
          "valid short configuration prefix rejected");
    CHECK(on1x_rt_engine_destroy(&engine, &error) == ON1X_RT_OK,
          "short-prefix engine destruction failed");
  }
  return 0;
}

static On1xRT_Status recursive_host(On1xRT_CallContext *call,
                                    void *function_context,
                                    const On1xRT_Value *arguments,
                                    size_t argument_count, On1xRT_Value *result,
                                    On1xRT_Error **error) {
  On1xRT_Error *nested_error = NULL;
  On1xRT_Value nested = on1x_rt_value_void();
  On1xRT_Status status;
  (void)function_context;
  (void)arguments;
  (void)error;
  if (argument_count)
    return ON1X_RT_INVALID_ARGUMENT;
  status = on1x_rt_call_host(call, TEXT("recursive"), &integer_signature, NULL,
                             0u, &nested, &nested_error);
  if (nested_error)
    (void)on1x_rt_error_release(&nested_error);
  *result = on1x_rt_value_i64((int64_t)status);
  return ON1X_RT_OK;
}

static On1xRT_Status bridge_host(On1xRT_CallContext *call,
                                 void *function_context,
                                 const On1xRT_Value *arguments,
                                 size_t argument_count, On1xRT_Value *result,
                                 On1xRT_Error **error) {
  (void)function_context;
  return on1x_rt_call_host(call, TEXT("recursive"), &integer_signature,
                           arguments, argument_count, result, error);
}

static On1xRT_Status
incompatible_bridge_host(On1xRT_CallContext *call, void *function_context,
                         const On1xRT_Value *arguments, size_t argument_count,
                         On1xRT_Value *result, On1xRT_Error **error) {
  On1xRT_Signature incompatible = integer_signature;
  On1xRT_Error *nested_error = NULL;
  On1xRT_Value nested = on1x_rt_value_void();
  On1xRT_Status status;
  (void)function_context;
  (void)arguments;
  (void)error;
  if (argument_count)
    return ON1X_RT_INVALID_ARGUMENT;
  incompatible.abi_version = ON1X_RT_ABI_VERSION + 1u;
  status = on1x_rt_call_host(call, TEXT("recursive"), &incompatible, NULL, 0u,
                             &nested, &nested_error);
  if (nested_error)
    (void)on1x_rt_error_release(&nested_error);
  *result = on1x_rt_value_i64((int64_t)status);
  return ON1X_RT_OK;
}

static int test_callbacks_and_unsupported(void) {
  On1xRT_Engine *engine = NULL;
  On1xRT_Error *error = NULL;
  On1xRT_Module module = {0};
  On1xRT_Instance instance = {0};
  On1xRT_Function function = {0};
  On1xRT_HostFunction host = {0};
  On1xRT_Value result = on1x_rt_value_void();
  const On1xRT_ExportDescriptor bridge_export = {
      ON1X_RT_ABI_VERSION,
      sizeof(On1xRT_ExportDescriptor),
      TEXT("bridge"),
      integer_signature,
      bridge_host,
      NULL,
      0u,
      0u};
  const On1xRT_HostFunctionDescriptor host_descriptor = {
      ON1X_RT_ABI_VERSION,
      sizeof(On1xRT_HostFunctionDescriptor),
      TEXT("recursive"),
      integer_signature,
      recursive_host,
      NULL,
      0u,
      0u};
  On1xRT_ModuleDescriptor value = descriptor("callbacks", ON1X_RT_MODULE_STATIC,
                                             NULL, 0u, &bridge_export, NULL, 0);
  CHECK(on1x_rt_engine_create(NULL, &engine, &error) == ON1X_RT_OK,
        "callback engine creation failed");
  {
    On1xRT_HostFunctionDescriptor incompatible = host_descriptor;
    incompatible.signature.abi_version = ON1X_RT_ABI_VERSION + 1u;
    CHECK(on1x_rt_engine_register_host_function(engine, &incompatible, &host,
                                                &error) == ON1X_RT_ABI_MISMATCH,
          "nested host signature ABI mismatch not distinguished");
    CHECK(!host.id, "invalid host signature published a handle");
    clear_error(&error);
  }
  CHECK(on1x_rt_engine_register_host_function(engine, &host_descriptor, &host,
                                              &error) == ON1X_RT_OK,
        "recursive host registration failed");
  CHECK(on1x_rt_engine_register_static_module(engine, &value, &module,
                                              &error) == ON1X_RT_OK,
        "callback module registration failed");
  CHECK(on1x_rt_instance_create(engine, module, &instance, &error) ==
            ON1X_RT_OK,
        "callback instance creation failed");
  {
    On1xRT_Signature incompatible = integer_signature;
    incompatible.abi_version = ON1X_RT_ABI_VERSION + 1u;
    CHECK(on1x_rt_instance_function(engine, instance, TEXT("bridge"),
                                    &incompatible, &function,
                                    &error) == ON1X_RT_ABI_MISMATCH,
          "expected signature ABI mismatch not distinguished");
    CHECK(!function.id, "invalid expected signature published a handle");
    clear_error(&error);
  }
  CHECK(on1x_rt_instance_function(engine, instance, TEXT("bridge"),
                                  &integer_signature, &function,
                                  &error) == ON1X_RT_OK,
        "bridge lookup failed");
  CHECK(on1x_rt_function_call(engine, function, NULL, 0u, &result, &error) ==
            ON1X_RT_OK,
        "bridge call failed");
  CHECK(result.kind == ON1X_RT_VALUE_I64 &&
            result.as.i64 == (int64_t)ON1X_RT_REENTRANT,
        "recursive host callback was not rejected");
  CHECK(on1x_rt_function_release(engine, &function, &error) == ON1X_RT_OK,
        "bridge release failed");
  CHECK(on1x_rt_instance_release(engine, &instance, &error) == ON1X_RT_OK,
        "callback instance release failed");
  CHECK(on1x_rt_module_unload(engine, &module, &error) == ON1X_RT_OK,
        "callback module unload failed");
  {
    const On1xRT_ExportDescriptor incompatible_bridge_export = {
        ON1X_RT_ABI_VERSION,
        sizeof(On1xRT_ExportDescriptor),
        TEXT("incompatible_bridge"),
        integer_signature,
        incompatible_bridge_host,
        NULL,
        0u,
        0u};
    On1xRT_ModuleDescriptor incompatible_bridge =
        descriptor("incompatible_bridge", ON1X_RT_MODULE_STATIC, NULL, 0u,
                   &incompatible_bridge_export, NULL, 0);
    CHECK(on1x_rt_engine_register_static_module(engine, &incompatible_bridge,
                                                &module, &error) == ON1X_RT_OK,
          "incompatible-signature bridge registration failed");
    CHECK(on1x_rt_instance_create(engine, module, &instance, &error) ==
              ON1X_RT_OK,
          "incompatible-signature bridge instance creation failed");
    CHECK(on1x_rt_instance_function(
              engine, instance, TEXT("incompatible_bridge"), &integer_signature,
              &function, &error) == ON1X_RT_OK,
          "incompatible-signature bridge lookup failed");
    CHECK(on1x_rt_function_call(engine, function, NULL, 0u, &result, &error) ==
              ON1X_RT_OK,
          "incompatible-signature bridge call failed");
    CHECK(result.kind == ON1X_RT_VALUE_I64 &&
              result.as.i64 == (int64_t)ON1X_RT_ABI_MISMATCH,
          "callback host signature ABI mismatch not distinguished");
    CHECK(on1x_rt_function_release(engine, &function, &error) == ON1X_RT_OK,
          "incompatible-signature bridge release failed");
    CHECK(on1x_rt_instance_release(engine, &instance, &error) == ON1X_RT_OK,
          "incompatible-signature bridge instance release failed");
    CHECK(on1x_rt_module_unload(engine, &module, &error) == ON1X_RT_OK,
          "incompatible-signature bridge unload failed");
  }
  CHECK(on1x_rt_host_function_unregister(engine, &host, &error) == ON1X_RT_OK,
        "recursive host unregister failed");
  {
    On1xRT_ModuleDescriptor bad = descriptor(
        "bad_output", ON1X_RT_MODULE_STATIC, NULL, 0u, &bad_export, NULL, 0);
    CHECK(on1x_rt_engine_register_static_module(engine, &bad, &module,
                                                &error) == ON1X_RT_OK,
          "bad-output fixture registration failed");
    CHECK(on1x_rt_instance_create(engine, module, &instance, &error) ==
              ON1X_RT_OK,
          "bad-output instance creation failed");
    CHECK(on1x_rt_instance_function(engine, instance, TEXT("bad"),
                                    &void_signature, &function,
                                    &error) == ON1X_RT_OK,
          "bad-output lookup failed");
    CHECK(on1x_rt_function_call(engine, function, NULL, 0u, &result, &error) ==
              ON1X_RT_SIGNATURE_MISMATCH,
          "malformed callback result accepted");
    clear_error(&error);
    CHECK(on1x_rt_function_release(engine, &function, &error) == ON1X_RT_OK,
          "bad-output function release failed");
    CHECK(on1x_rt_instance_release(engine, &instance, &error) == ON1X_RT_OK,
          "bad-output instance release failed");
    CHECK(on1x_rt_module_unload(engine, &module, &error) == ON1X_RT_OK,
          "bad-output module unload failed");
  }
  for (int feature = ON1X_RT_FEATURE_WASM;
       feature <= ON1X_RT_FEATURE_PROFILER; ++feature)
    CHECK(on1x_rt_feature_status((On1xRT_Feature)feature) ==
              ON1X_RT_UNSUPPORTED,
          "unsupported feature reported as available");
  CHECK(on1x_rt_feature_status((On1xRT_Feature)999) == ON1X_RT_INVALID_ARGUMENT,
        "unknown feature was not rejected");
  CHECK(on1x_rt_engine_load_dynamic(engine, TEXT("/not/loaded"), &module,
                                     &error) == ON1X_RT_UNSUPPORTED,
        "path-only dynamic loading did not reject explicitly");
  clear_error(&error);
  {
    const On1xRT_Status feature =
        on1x_rt_feature_status(ON1X_RT_FEATURE_DYNAMIC_MODULE);
    CHECK(feature == ON1X_RT_OK || feature == ON1X_RT_UNSUPPORTED,
          "invalid status for optional dynamic modules");
    CHECK(on1x_rt_engine_load_authorized_dynamic(
              engine, TEXT("/not/loaded"), NULL, NULL, &module, &error) ==
              (feature == ON1X_RT_OK ? ON1X_RT_DENIED : ON1X_RT_UNSUPPORTED),
          "dynamic loading without host policy did not fail closed");
    CHECK(!module.id && !module.owner, "rejected load published a handle");
  }
  clear_error(&error);
  {
    static const uint8_t wasm[] = {0u, 'a', 's', 'm'};
    const On1xRT_BytesView bytes = {wasm, sizeof(wasm)};
    CHECK(on1x_rt_engine_load_wasm(engine, bytes, &module, &error) ==
              ON1X_RT_UNSUPPORTED,
          "WASM did not reject explicitly");
    clear_error(&error);
  }
  CHECK(on1x_rt_engine_enable_concurrency(engine, &error) ==
            ON1X_RT_UNSUPPORTED,
        "concurrency did not reject explicitly");
  clear_error(&error);
  CHECK(on1x_rt_engine_attach_debugger(engine, &error) == ON1X_RT_UNSUPPORTED,
        "debugger did not reject explicitly");
  clear_error(&error);
  CHECK(on1x_rt_engine_destroy(&engine, &error) == ON1X_RT_OK,
        "callback engine destruction failed");
  return 0;
}

int main(void) {
  int status;
  if ((status = test_descriptors()))
    return status;
  if ((status = test_dependencies()))
    return status;
  if ((status = test_cycles_and_rollback()))
    return status;
  if ((status = test_recoverable_lifecycle()))
    return status;
  if ((status = test_aot_authorization()))
    return status;
  if ((status = test_limits_threads_and_values()))
    return status;
  if ((status = test_callbacks_and_unsupported()))
    return status;
  return 0;
}
