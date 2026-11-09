#define MULIB_IMPL_EZJNI
#include "mulib-ezjni.h"
#include <assert.h>
#include <stdio.h>
int main (void)
{
  mulib_ezjni_signature_t signature;
  assert (mulib_ezjni_signature ("(ILjava/lang/String;[B)J",&signature) == MULIB_EZJNI_OK);
  assert (signature.count == 3 && signature.result == 'J');
  printf ("JNI descriptor: %zu arguments; native backend %s\n", signature.count, mulib_ezjni_available () ? "enabled" : "disabled");
  /* A real JNI entry point supplies JNIEnv*; native worker threads instead use
   * mulib_ezjni_enter(JavaVM*, capacity, &scope) and paired leave(). */
  return 0;
}
