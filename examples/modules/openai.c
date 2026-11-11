#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_OPENAI
#include "mulib-openai.h"
#include <stdio.h>
#include <stdlib.h>
int main (void)
{
  mulib_http_header_t path = {(char *)"response_id", (char *)"response/example"};
  char *target = NULL;
  const mulib_openai_operation_t *op = mulib_openai_operation ("getResponse");
  if (mulib_openai_target (op, &path, 1, NULL, 0, 0, &target) != 1) return 1;
  printf ("%s %s (%zu registered operations)\n", op->method, target, mulib_openai_operation_count ());
  free (target); return 0;
}
