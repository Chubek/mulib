#define MULIB_IMPL_DYNLOAD
#include "mulib-dynload.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  char name[128] = "untouched", error[256];
  mulib_dynload_t d = MULIB_DYNLOAD_INIT;
  void *(*allocate)(size_t) = NULL;
  void (*release)(void *) = NULL;
  assert(mulib_dynload_native_name("awesome",name,2) == MULIB_DYNLOAD_ERANGE && !strcmp(name,"untouched"));
  assert(mulib_dynload_native_name("../bad",name,sizeof name) == MULIB_DYNLOAD_EINVAL);
  assert(!mulib_dynload_native_name("awesome",name,sizeof name));
#if defined(_WIN32)
  assert(!strcmp(name,"awesome.dll"));
  assert(!mulib_dynload_open(&d,"msvcrt.dll",error,sizeof error));
#elif defined(__unix__) || defined(__APPLE__)
# if defined(__APPLE__)
  assert(!strcmp(name,"libawesome.dylib"));
# else
  assert(!strcmp(name,"libawesome.so"));
# endif
  assert(mulib_dynload_open_ex(&d,NULL,MULIB_DYNLOAD_NOW|MULIB_DYNLOAD_LAZY,error,sizeof error) == MULIB_DYNLOAD_EINVAL);
  assert(!mulib_dynload_open_ex(&d,NULL,MULIB_DYNLOAD_LAZY|MULIB_DYNLOAD_LOCAL,error,sizeof error));
#else
  assert(mulib_dynload_open(&d,NULL,error,sizeof error) == MULIB_DYNLOAD_ENOTSUP); return 0;
#endif
  assert(!mulib_dynload_function(&d,"malloc",&allocate,sizeof allocate,error,sizeof error));
  assert(!mulib_dynload_function(&d,"free",&release,sizeof release,error,sizeof error));
  {
    void *p = allocate(32); assert(p); memset(p,42,32); release(p);
    assert(mulib_dynload_function(&d,"mulib_missing_123456",&allocate,sizeof allocate,error,sizeof error) == MULIB_DYNLOAD_EIO);
    assert(allocate && error[0]);
    assert(mulib_dynload_function(&d,"malloc",&allocate,1,error,sizeof error) == MULIB_DYNLOAD_ENOTSUP);
  }
  assert(!mulib_dynload_close(&d,error,sizeof error));
  return 0;
}
