#include "On1xRT.hpp"
#include <type_traits>

extern "C" const On1xRT_ModuleDescriptor *ON1X_RT_MODULE_ENTRY_SYMBOL();
static_assert(!std::is_copy_constructible_v<on1x::rt::host>);
int main() {
  bool permit_shutdown = false;
  auto descriptor = *ON1X_RT_MODULE_ENTRY_SYMBOL();
  descriptor.provider_context = &permit_shutdown;
  descriptor.initialize = [](On1xRT_ModuleContext *, void *, void **,
                             On1xRT_Error **) { return ON1X_RT_OK; };
  descriptor.shutdown = [](On1xRT_ModuleContext *, void *ctx, void *,
                           On1xRT_Error **) {
    return *static_cast<bool *>(ctx) ? ON1X_RT_OK : ON1X_RT_CALLBACK_FAILED;
  };
  on1x::rt::host first;
  first.register_static(descriptor);
  first.instantiate();
  auto host = std::move(first);
  first.close();
  try {
    host.register_static(*ON1X_RT_MODULE_ENTRY_SYMBOL());
    return 3;
  } catch (const on1x::rt::error &e) {
    if (e.status != ON1X_RT_INVALID_STATE)
      return 4;
  }
  try {
    host.call_i64("fail");
    return 1;
  } catch (const on1x::rt::error &e) {
    if (e.status != ON1X_RT_CALLBACK_FAILED || e.code != 4321 ||
        std::string_view(e.what()) != "provider failed")
      return 2;
  }
  if (host.call_i64("choose", {-1, -42}) != -1)
    return 5;
  try {
    host.close();
    return 6;
  } catch (const on1x::rt::error &e) {
    if (e.status != ON1X_RT_CALLBACK_FAILED)
      return 7;
  }
  permit_shutdown = true;
  host.close();
}
