#define MULIB_IMPL_SHA
#define MULIB_IMPL_BUILDSTATE
#include "mulib-buildstate.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  const char *argv[] = {"cc", "-c", "main.c"};
  const char *inputs[] = {"main.c", "api.h"};
  const char *outputs[] = {"main.o"};
  const mulib_buildstate_env_t env[] = {{"LANG", "C"}};
  const mulib_buildstate_action_t action = {
      argv, 3, NULL, env, 1, inputs, 2, outputs, 1, 30000, 0};
  char fingerprint[65];
  assert(!mulib_buildstate_action_fingerprint(&action, fingerprint));
  puts(fingerprint);
  return 0;
}
