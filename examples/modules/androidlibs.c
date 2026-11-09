#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_EZJNI
#define MULIB_IMPL_EZNDK
#define MULIB_IMPL_ANDROIDLIBS
#include "mulib-androidlibs.h"
#include <assert.h>
#include <stdio.h>
int main (void)
{
  mulib_androidlibs_info_t info; const char *name; int api;
  assert (mulib_androidlibs_info (MULIB_ANDROIDLIBS_VULKAN,&info));
  assert (mulib_androidlibs_service_info (MULIB_ANDROIDLIBS_CAMERA_SERVICE,&name,&api));
  printf ("%s: API %d+, header <%s>\n", info.soname, info.min_api, info.header);
  printf ("Context service %s: API %d+\n",name,api);
  return 0;
}
