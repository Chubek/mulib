#include "On1xRT-Allocator.h"
#include "On1xRT-Array.h"
#include "On1xRT-Buffer.h"
#include "On1xRT-Capability.h"
#include "On1xRT-Channel.h"
#include "On1xRT-Closure.h"
#include "On1xRT-Debugger.h"
#include "On1xRT-Engine.h"
#include "On1xRT-Error.h"
#include "On1xRT-Event.h"
#include "On1xRT-Fiber.h"
#include "On1xRT-Function.h"
#include "On1xRT-Handle.h"
#include "On1xRT-HostFunction.h"
#include "On1xRT-Instance.h"
#include "On1xRT-Memory.h"
#include "On1xRT-Module.h"
#include "On1xRT-Profiler.h"
#include "On1xRT-Sandbox.h"
#include "On1xRT-Scheduler.h"
#include "On1xRT-String.h"
#include "On1xRT-Task.h"
#include "On1xRT-Value.h"

#include "fixture_module.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

static_assert(std::is_standard_layout_v<On1xRT_Module>);
static_assert(std::is_standard_layout_v<On1xRT_Value>);
static_assert(sizeof(On1xRT_Module) == 2u * sizeof(std::uint64_t));
static_assert(sizeof(On1xRT_Status) == sizeof(std::uint32_t));

namespace {
bool okay(On1xRT_Status status, On1xRT_Error *&error, const char *operation) {
  if (status == ON1X_RT_OK && error == nullptr)
    return true;
  const auto message = on1x_rt_error_message(error);
  std::fprintf(stderr, "%s: %s: %.*s\n", operation, on1x_rt_status_name(status),
               static_cast<int>(message.size),
               message.data ? message.data : "");
  if (error)
    (void)on1x_rt_error_release(&error);
  return false;
}
} // namespace

int main() {
  static const On1xRT_ValueKind parameters[] = {ON1X_RT_VALUE_I64,
                                                ON1X_RT_VALUE_I64};
  static const On1xRT_Signature signature = {
      ON1X_RT_ABI_VERSION, sizeof(On1xRT_Signature),
      parameters,          2u,
      ON1X_RT_VALUE_I64,   0u};
  On1xRT_Engine *engine = nullptr;
  On1xRT_Error *error = nullptr;
  On1xRT_Module module{};
  On1xRT_Instance instance{};
  On1xRT_Function function{};
  On1xRT_Value result = on1x_rt_value_void();
  const On1xRT_Value arguments[] = {on1x_rt_value_i64(20),
                                    on1x_rt_value_i64(21)};

  on1x_rt_test_fixture_reset();
  if (!okay(on1x_rt_engine_create(nullptr, &engine, &error), error,
            "engine_create"))
    return 1;
  if (!okay(on1x_rt_engine_register_static_module(
                engine, ON1X_RT_MODULE_ENTRY_SYMBOL(), &module, &error),
            error, "register"))
    return 2;
  if (!okay(on1x_rt_instance_create(engine, module, &instance, &error), error,
            "instance_create"))
    return 3;
  if (!okay(on1x_rt_instance_function(engine, instance,
                                      On1xRT_StringView{"add", 3u}, &signature,
                                      &function, &error),
            error, "function_lookup"))
    return 4;
  if (!okay(on1x_rt_function_call(engine, function, arguments, 2u, &result,
                                  &error),
            error, "function_call"))
    return 5;
  if (result.kind != ON1X_RT_VALUE_I64 || result.as.i64 != 42)
    return 6;
  if (!okay(on1x_rt_value_release(engine, &result, &error), error,
            "value_release") ||
      !okay(on1x_rt_function_release(engine, &function, &error), error,
            "function_release") ||
      !okay(on1x_rt_instance_release(engine, &instance, &error), error,
            "instance_release") ||
      !okay(on1x_rt_module_unload(engine, &module, &error), error,
            "module_unload") ||
      !okay(on1x_rt_engine_destroy(&engine, &error), error, "engine_destroy"))
    return 7;
  return engine == nullptr ? 0 : 8;
}
