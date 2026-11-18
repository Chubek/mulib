#define MULIB_IMPL_DEPFILE
#include "mulib-depfile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int print_rule(void *user, const mulib_depfile_rule_t *rule) {
  size_t i;
  (void)user;
  for (i = 0; i < rule->target_count; ++i)
    printf("%s%s", i ? " " : "", rule->targets[i]);
  printf(":");
  for (i = 0; i < rule->dependency_count; ++i)
    printf(" %s", rule->dependencies[i]);
  putchar('\n');
  return 0;
}

int main(void) {
  const char text[] = "obj/main.o: src/main.c include/api.h\n";
  mulib_depfile_error_t error;
  assert(!mulib_depfile_parse(text, strlen(text), NULL, print_rule, NULL,
                              &error));
  return 0;
}
