#define MULIB_IMPL_ENTROPY
#include "mulib-entropy.h"
#include <stdio.h>
int main(void)
{
  unsigned char bytes[32];
  if(!mulib_entropy(bytes,sizeof bytes))return 1;
  puts("Generated 32 bytes of OS cryptographic entropy");
  mulib_secret_wipe(bytes,sizeof bytes);return 0;
}
