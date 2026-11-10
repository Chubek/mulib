#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_SEARCH
#include "mulib-search.h"
static int match(size_t offset,void *user) {
  size_t *count=user;assert(offset==*count);++*count;return 1;
}
int main(void){
  assert(mulib_search_kmp("hello",5,"ll",2)==2);
  size_t count=0;
  assert(mulib_search_all("aaaa",4,"aa",2,match,&count)==3&&count==3);
  return 0;
}
