#define MULIB_IMPL_AUXHASH
#include "mulib-auxhash.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
int main(void)
{
  uint64_t table[256]; mulib_auxhash_buzhash_table(table,42);
  uint64_t hash=mulib_auxhash_buzhash("abcd",4,table);
  hash=mulib_auxhash_buzhash_roll(hash,'a','e',4,table);
  assert(hash==mulib_auxhash_buzhash("bcde",4,table));
  printf("%08" PRIx32 "\n",mulib_auxhash_murmur2("hello",5,0)); return 0;
}
