#include <assert.h>
#include <string.h>
#include <math.h>
#define MULIB_IMPL_SORT
#include "mulib-sort.h"
static int mulib_example_compare(const void*a,const void*b){int x=*(const int*)a,y=*(const int*)b;return (x>y)-(x<y);}
int main(void){
  int a[]={3,1,2}; mulib_sort_array(a,3,sizeof(int),mulib_example_compare); assert(a[0]==1);
  int b[]={3,2,1,2},key=2;
  assert(mulib_sort_stable(b,4,sizeof(int),mulib_example_compare));
  assert(mulib_sort_lower_bound(&key,b,4,sizeof(int),mulib_example_compare)==1);
  return 0;
}
