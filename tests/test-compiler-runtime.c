#define MULIB_IMPL_GC
#define MULIB_IMPL_GC_ROOTS
#define MULIB_IMPL_VM_HOST
#define MULIB_IMPL_VM_VALUE
#define MULIB_IMPL_VM_MODULE
#define MULIB_IMPL_BYTECODE
#define MULIB_IMPL_BYTECODE_VERIFY
#include "mulib-gc-roots.h"
#include "mulib-vm-host.h"
#include "mulib-vm-value.h"
#include "mulib-vm-module.h"
#include <assert.h>
static int host(void *user,const int64_t *args,size_t n,int64_t *out) {
    assert(n==1);*out=args[0]+1;return *(int *)user;
}
int main(void) {
    mulib_gc_heap_t heap={0},other={0};mulib_gc_roots_t roots={0};size_t reclaimed=99;
    mulib_gc_object_t *a=mulib_gc_alloc(&heap,1),*b=mulib_gc_alloc(&heap,1),*c=mulib_gc_alloc(&heap,0);
    mulib_gc_object_t *foreign=mulib_gc_alloc(&other,0);
    assert(a&&b&&c&&foreign&&mulib_gc_set(&heap,a,0,b)&&mulib_gc_set(&heap,b,0,a));
    assert(!mulib_gc_set(&heap,a,1,c)&&!mulib_gc_set(&heap,a,0,foreign));
    assert(!mulib_gc_collect(&heap,&foreign,1,&reclaimed)&&reclaimed==99&&heap.count==3);
    assert(mulib_gc_roots_add(&roots,a)&&mulib_gc_roots_add(&roots,a)&&roots.count==1);
    assert(mulib_gc_roots_collect(&heap,&roots,&reclaimed)&&reclaimed==1&&heap.count==2);
    assert(mulib_gc_roots_collect(&heap,&roots,&reclaimed)&&reclaimed==0);
    assert(mulib_gc_roots_remove(&roots,a)&&!mulib_gc_roots_remove(&roots,a));
    assert(mulib_gc_roots_collect(&heap,&roots,&reclaimed)&&reclaimed==2&&heap.count==0);
    assert(mulib_gc_roots_collect(&heap,&roots,&reclaimed)&&reclaimed==0);
    mulib_gc_roots_free(&roots);mulib_gc_heap_free(&heap);mulib_gc_heap_free(&other);
    mulib_vm_value_t value={0},copy={0};char binary[]={'a',0,'z'};
    assert(mulib_vm_value_bytes(&value,binary,3)&&mulib_vm_value_copy(&copy,&value));
    assert(value.value.str.bytes!=copy.value.str.bytes&&!memcmp(copy.value.str.bytes,binary,3));
    assert(mulib_vm_value_bytes(&value,value.value.str.bytes+1,2)&&value.value.str.length==2);
    assert(mulib_vm_value_copy(&copy,&copy)&&copy.value.str.length==3);
    mulib_vm_value_t invalid={0};invalid.kind=(mulib_vm_value_kind_t)-1;
    assert(!mulib_vm_value_copy(&copy,&invalid)&&copy.kind==MULIB_VM_BYTES);
    mulib_vm_value_free(&value);mulib_vm_value_free(&copy);mulib_vm_value_free(&copy);
    mulib_vm_host_t h={0};size_t id;int ok=1;int64_t arg=41,result=99;
    assert(mulib_vm_host_register(&h,host,&ok,1,&id)&&id==0);
    assert(!mulib_vm_host_call(&h,0,NULL,0,&result)&&result==99);
    assert(mulib_vm_host_call(&h,0,&arg,1,&result)&&result==42);
    ok=0;result=99;assert(!mulib_vm_host_call(&h,0,&arg,1,&result)&&result==99);
    assert(!mulib_vm_host_register(&h,host,&ok,1025,&id));mulib_vm_host_free(&h);
    mulib_vm_modules_t modules={0};mulib_bc_instruction_t code[]={{MULIB_BC_PUSH,42},{MULIB_BC_HALT,0}};
    char name[]="main";
    assert(mulib_vm_module_add(&modules,name,code,2,0));code[0].operand=7;name[0]='x';
    const mulib_vm_module_t *m=mulib_vm_module_find(&modules,"main");
    assert(m&&m->code[0].operand==42&&!mulib_vm_module_find(&modules,"Main"));
    assert(!mulib_vm_module_add(&modules,"main",code,2,0));
    code[0].op=MULIB_BC_ADD;assert(!mulib_vm_module_add(&modules,"bad",code,2,0)&&modules.count==1);
    mulib_vm_modules_free(&modules);
    return 0;
}
