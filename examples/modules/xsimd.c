#define MULIB_IMPL_XSIMD
#include "mulib-xsimd.h"
#include <assert.h>
int main(void)
{
  float samples[5]={1,2,3,4,5}, dot;
  assert(mulib_xsimd_scale(MULIB_XSIMD_AUTO,samples,samples,5,0.5f));
  assert(mulib_xsimd_dot(MULIB_XSIMD_AUTO,samples,samples,5,&dot));
  assert(dot==13.75f); return 0;
}
