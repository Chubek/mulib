/* Trusted native fixture; separately linked, using the host's On1xRT. */
#include "On1xRT.h"

#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>

#define TEXT_INIT(s) {s, sizeof(s) - 1u}

static void mark(char event) {
  const char *path = getenv("ON1X_RT_TEST_DYNAMIC_TRACE");
  FILE *log = path ? fopen(path, "ab") : NULL;
  if (log) {
    (void)fputc(event, log);
    (void)fclose(log);
  }
}

__attribute__((constructor)) static void startup(void) { mark('L'); }
__attribute__((destructor)) static void teardown(void) { mark('U'); }

#if TEST_DYNAMIC_KIND == 3
ON1X_RT_EXPORT char unrelated_symbol(void) { return 'X'; }
#elif TEST_DYNAMIC_KIND == 4
ON1X_RT_EXPORT int ON1X_RT_MODULE_ENTRY_SYMBOL = 0;
#elif TEST_DYNAMIC_KIND == 8
/* The sought entry is defined by a dependency, not this shared object. */
extern int dynamic_dependency_anchor(void);
ON1X_RT_EXPORT int dependent_symbol(void) { return dynamic_dependency_anchor(); }
#else

#if TEST_DYNAMIC_KIND == 1
ON1X_RT_EXPORT int dynamic_dependency_anchor(void) { return 1; }
#endif

typedef struct State {
  int fail_shutdown;
} State;

static On1xRT_Status initialize(On1xRT_ModuleContext *context, void *provider,
                                void **state, On1xRT_Error **error) {
  mark('I');
  (void)provider;
  (void)error;
#if TEST_DYNAMIC_KIND == 5
  (void)state;
  (void)on1x_rt_module_error(
      context, 4242u, (On1xRT_StringView)TEXT_INIT("init failure"), error);
  return ON1X_RT_CALLBACK_FAILED;
#else
  State *live = on1x_rt_module_allocate(context, sizeof(*live), alignof(State));
  if (!live)
    return ON1X_RT_OUT_OF_MEMORY;
  *live = (State){0};
  *state = live;
  return ON1X_RT_OK;
#endif
}

static On1xRT_Status shutdown_module(On1xRT_ModuleContext *context,
                                     void *provider, void *state,
                                     On1xRT_Error **error) {
  State *live = state;
  (void)provider;
  if (live->fail_shutdown) {
    live->fail_shutdown = 0;
    mark('F');
    (void)on1x_rt_module_error(
        context, 4243u, (On1xRT_StringView)TEXT_INIT("retry shutdown"), error);
    return ON1X_RT_CALLBACK_FAILED;
  }
  mark('S');
  on1x_rt_module_release_memory(context, live, sizeof(*live), alignof(State));
  return ON1X_RT_OK;
}

static On1xRT_Status create_instance(On1xRT_ModuleContext *context,
                                     void *provider, void *module_state,
                                     void **instance, On1xRT_Error **error) {
  (void)context;
  (void)provider;
  (void)error;
  mark('C');
  *instance = module_state;
  return ON1X_RT_OK;
}

static On1xRT_Status destroy_instance(On1xRT_ModuleContext *context,
                                      void *provider, void *module_state,
                                      void *instance, On1xRT_Error **error) {
  (void)context;
  (void)provider;
  (void)error;
  mark('D');
  return instance == module_state ? ON1X_RT_OK : ON1X_RT_CALLBACK_FAILED;
}

static On1xRT_Status invoke(On1xRT_CallContext *call, void *function,
                            const On1xRT_Value *arguments, size_t count,
                            On1xRT_Value *result, On1xRT_Error **error) {
  State *live = on1x_rt_call_module_state(call);
  (void)function;
  (void)error;
  if (!live || on1x_rt_call_instance_state(call) != live || count != 1u)
    return ON1X_RT_CALLBACK_FAILED;
  mark('V');
  live->fail_shutdown = (int)arguments[0].as.boolean;
  *result = on1x_rt_value_i64(100 + TEST_DYNAMIC_KIND);
  return ON1X_RT_OK;
}

static const On1xRT_ValueKind parameters[] = {ON1X_RT_VALUE_BOOL};
static const On1xRT_ExportDescriptor exports[] = {
    {ON1X_RT_ABI_VERSION, sizeof(On1xRT_ExportDescriptor), TEXT_INIT("value"),
     {ON1X_RT_ABI_VERSION, sizeof(On1xRT_Signature), parameters, 1u,
      ON1X_RT_VALUE_I64, 0u}, invoke, NULL, 0u, 0u}};

#if TEST_DYNAMIC_KIND == 6
#define MODULE_NAME "resident"
#elif TEST_DYNAMIC_KIND == 7
static const On1xRT_StringView dependencies[] = {TEXT_INIT("dynamic_fixture")};
#define MODULE_NAME "depends_on_fixture"
#else
#define MODULE_NAME "dynamic_fixture"
#endif

static const On1xRT_ModuleDescriptor descriptor = {
    .abi_version = TEST_DYNAMIC_KIND == 2 ? 99u : ON1X_RT_ABI_VERSION,
    .struct_size = sizeof(On1xRT_ModuleDescriptor),
    .descriptor_version = ON1X_RT_MODULE_DESCRIPTOR_VERSION,
    .flags = ON1X_RT_MODULE_AUTHORIZED_AOT,
    .name = TEXT_INIT(MODULE_NAME),
    .revision = TEXT_INIT("test-v1"),
    .identity_sha256 = TEXT_INIT(
        "aeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeae"),
#if TEST_DYNAMIC_KIND == 7
    .dependencies = dependencies,
    .dependency_count = 1u,
#endif
    .exports = exports,
    .export_count = sizeof(exports) / sizeof(exports[0]),
    .initialize = initialize,
    .shutdown = shutdown_module,
    .instance_create = create_instance,
    .instance_destroy = destroy_instance};

ON1X_RT_EXPORT const On1xRT_ModuleDescriptor *ON1X_RT_MODULE_ENTRY_SYMBOL(void) {
  mark('E');
  return &descriptor;
}
#endif
