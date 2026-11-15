#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
static int remaining=-1;
static void *limited_malloc(size_t size) {
  if(remaining==0)return NULL;
  if(remaining>0)--remaining;
  return malloc(size);
}
#define BASIS_MALLOC limited_malloc
#include "../stdlib/runtime/basis_native.c"
int main(int argc,char **argv) {
  if(argc!=2)return 2;
  FILE *file=fopen(argv[1],"wb");
  if(!file)return 2;
  if(fputs("binary",file)<0 || fclose(file))return 2;
  /* The input buffer succeeds; allocation of its hex snapshot fails. */
  remaining=1;
  basis_native_read_hex(argv[1]);
  return 0;
}
