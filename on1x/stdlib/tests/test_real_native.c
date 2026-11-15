#include "../runtime/basis_native.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
  assert(basis_native_real_compare(basis_native_real_add("0.25","0.75"),"1")==0);
  assert(basis_native_real_compare(basis_native_real_sub("4","3"),"1")==0);
  assert(basis_native_real_compare(basis_native_real_mul("3","7"),"21")==0);
  assert(basis_native_real_compare(basis_native_real_div("10","2"),"5")==0);
  assert(basis_native_real_compare(basis_native_math_sqrt("16"),"4")==0);
  assert(basis_native_real_compare(basis_native_math_pow("2","5"),"32")==0);
  assert(basis_native_ieee_isfinite("42"));
  puts("basis real tests passed");
  return 0;
}
