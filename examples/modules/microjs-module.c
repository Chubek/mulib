/* Complete, offline microjs-module example. */
#define MULIB_IMPL_MICROJS_MODULE
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-microjs-module.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_microjs_modules_t modules = { 0 };
  assert (mulib_microjs_module_register (&modules, "main", "export default 1",
                                         16));
  assert (mulib_microjs_module_find (&modules, "main"));
  mulib_microjs_modules_free (&modules);
  return 0;
}
