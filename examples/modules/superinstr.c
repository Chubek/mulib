#define MULIB_IMPL_MIR
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_SUPERINSTR
#include "mulib-superinstr.h"
#include <assert.h>
#include <string.h>
int main (void)
{
  static const char text[] = "\tmovq %rax, -8(%rbp)\n\tmovq %rcx, -16(%rbp)\n";
  mulib_superinstr_result_t result = {0};
  assert (mulib_superinstr_pack (text,strlen(text),"example",NULL,&result) == MULIB_MIR_OK);
  assert (!strcmp (result.text,text));
  mulib_superinstr_free (&result); return 0;
}
