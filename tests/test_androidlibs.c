#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_EZJNI
#define MULIB_IMPL_EZNDK
#define MULIB_IMPL_ANDROIDLIBS
#include "mulib-androidlibs.h"
#include <assert.h>
#include <string.h>
int main (void)
{
  mulib_ezjni_signature_t sig;
  mulib_androidlibs_info_t info;
  mulib_androidlibs_native_t lib = {0};
  mulib_ezndk_window_t window = {0};
  const char *name; int api;
  assert (mulib_ezjni_signature ("(I[JLjava/lang/String;D)Z", &sig) == MULIB_EZJNI_OK);
  assert (sig.count == 4 && sig.parameters[1] == 'L' && sig.result == 'Z');
  assert (mulib_ezjni_signature ("(V)I", &sig) == MULIB_EZJNI_INVALID);
  assert (mulib_ezjni_signature ("()[V", &sig) == MULIB_EZJNI_INVALID);
  assert (mulib_ezjni_signature ("(Ljava.lang.Object;)V", &sig) == MULIB_EZJNI_INVALID);
  assert (mulib_androidlibs_info (MULIB_ANDROIDLIBS_VULKAN, &info));
  assert (info.min_api == 24 && !strcmp (info.soname, "libvulkan.so"));
  assert (mulib_androidlibs_open (MULIB_ANDROIDLIBS_AAUDIO, 25, &lib, NULL, 0) == MULIB_ANDROIDLIBS_API_LEVEL);
  assert (mulib_androidlibs_service_info (MULIB_ANDROIDLIBS_CAMERA_SERVICE, &name, &api) && api == 21 && !strcmp (name, "camera"));
  assert (mulib_androidlibs_close (&lib) == MULIB_ANDROIDLIBS_OK);
  assert (mulib_ezndk_window_close (&window) == MULIB_EZNDK_OK);
#if !defined(MULIB_EZJNI_NATIVE)
  assert (!mulib_ezjni_available ());
  assert (mulib_ezjni_exception (&window) == MULIB_EZJNI_UNSUPPORTED);
#endif
  return 0;
}
