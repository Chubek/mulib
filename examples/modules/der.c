#define MULIB_IMPL_DER
#include "mulib-der.h"
#include <assert.h>
int main(void)
{
  static const unsigned char encoded[]={0x30,3,2,1,42};
  mulib_der_cursor_t cursor={encoded,sizeof encoded,0};mulib_der_value_t value;uint32_t number;
  assert(mulib_der_take(&cursor,0x30,&value)&&cursor.offset==cursor.length);
  cursor=mulib_der_contents(&value);
  assert(mulib_der_take(&cursor,2,&value)&&mulib_der_u32(&value,&number)&&number==42);return 0;
}
