#define MULIB_IMPL_VM_HOST
#include "mulib-vm-host.h"
#include <assert.h>
static int increment(void *user,const int64_t *args,size_t count,int64_t *out) {
    (void)user;assert(count==1);if(args[0]==INT64_MAX)return 0;*out=args[0]+1;return 1;
}
int main(void) {
    mulib_vm_host_t host={0};size_t id;int64_t input=41,result;
    assert(mulib_vm_host_register(&host,increment,NULL,1,&id));
    assert(mulib_vm_host_call(&host,id,&input,1,&result)&&result==42);
    mulib_vm_host_free(&host);return 0;
}
