#define MULIB_IMPL_VM_VALUE
#include "mulib-vm-value.h"
#include <assert.h>
int main(void) {
    mulib_vm_value_t value={0},copy={0};const char bytes[]={'a',0,'b'};
    assert(mulib_vm_value_bytes(&value,bytes,sizeof bytes)&&mulib_vm_value_copy(&copy,&value));
    mulib_vm_value_free(&value);
    assert(copy.value.str.length==3&&!memcmp(copy.value.str.bytes,bytes,3));
    mulib_vm_value_free(&copy);return 0;
}
