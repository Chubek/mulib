#define main mune_frontend_allocation_main
#include "test_mune_frontend_alloc.c"
#undef main
#include <stdio.h>

static int count_bytes(void *user,const char *bytes,size_t count)
{
    size_t *size=(size_t *)user;
    (void)bytes; *size+=count; return 1;
}
struct image { unsigned char bytes[16384]; size_t size; };
static int store_bytes(void *user,const char *bytes,size_t count)
{
    struct image *image=(struct image *)user;
    assert(count<=sizeof(image->bytes)-image->size);
    memcpy(image->bytes+image->size,bytes,count); image->size+=count; return 1;
}

int main(void)
{
    size_t k, allocations, baseline;
    mulib_mune_state_t *s;
    mulib_mune_chunk_t *chunk;
    mulib_mune_error_t error;
    const char valid[] =
        "local x={}; local function f(a,...) local y=a+1; "
        "return function() return x,y end,... end; "
        "local g,a=f(41,'ok'); local t,v=g(); "
        "assert(t==x and v==42 and a=='ok'); "
        "return setmetatable({g=g},{__index=function(t,k) return k end})";
    calls=fail_at=0;
    s=mulib_mune_state_create(NULL); assert(s); allocations=calls;
    mulib_mune_state_destroy(s); assert(!live);
    for(k=1;k<=allocations;++k) {
        calls=0; fail_at=k;
        s=mulib_mune_state_create(NULL);
        mulib_mune_state_destroy(s);
        assert(!live);
    }
    fail_at=0;
    chunk=mulib_mune_parse(valid,sizeof(valid)-1,NULL,&error); assert(chunk);
    baseline=live;
    {
        mulib_mune_result_t *r=NULL;
        s=mulib_mune_state_create(NULL); assert(s); calls=0;
        assert(mulib_mune_run(s,chunk,&r,&error)==MULIB_MUNE_OK);
        allocations=calls;
        mulib_mune_result_destroy(r); mulib_mune_state_destroy(s); assert(live==baseline);
    }
    for(k=1;k<=allocations;++k) {
        mulib_mune_result_t *r=NULL;
        mulib_mune_status_t status;
        fail_at=0; s=mulib_mune_state_create(NULL); assert(s);
        calls=0; fail_at=k;
        status=mulib_mune_run(s,chunk,&r,&error);
        if(status!=MULIB_MUNE_MEMORY) fprintf(stderr,"allocation %zu: %d %s\n",k,status,error.message);
        assert(status==MULIB_MUNE_MEMORY && !r);
        fail_at=0; mulib_mune_state_destroy(s); assert(live==baseline);
    }
    mulib_mune_chunk_destroy(chunk); assert(!live);
    {
        int mode;
        const char compiled[]="local a=4; if a>2 then return a+1,{a,2} end";
        for(mode=MULIB_MUNE_VM;mode<=MULIB_MUNE_JIT;++mode) {
            mulib_mune_result_t *r=NULL;
            if(mode==MULIB_MUNE_JIT && !mulib_jit_supported()) continue;
            fail_at=0; chunk=mulib_mune_parse(compiled,sizeof(compiled)-1,NULL,&error); assert(chunk);
            s=mulib_mune_state_create(NULL); assert(s);
            assert(mulib_mune_set_mode(s,(mulib_mune_mode_t)mode)==MULIB_MUNE_OK);
            calls=0;
            assert(mulib_mune_run(s,chunk,&r,&error)==MULIB_MUNE_OK); allocations=calls;
            mulib_mune_result_destroy(r); mulib_mune_state_destroy(s); mulib_mune_chunk_destroy(chunk); assert(!live);
            for(k=1;k<=allocations;++k) {
                mulib_mune_status_t status;
                fail_at=0; r=NULL;
                chunk=mulib_mune_parse(compiled,sizeof(compiled)-1,NULL,&error); assert(chunk);
                s=mulib_mune_state_create(NULL); assert(s);
                assert(mulib_mune_set_mode(s,(mulib_mune_mode_t)mode)==MULIB_MUNE_OK);
                calls=0; fail_at=k;
                status=mulib_mune_run(s,chunk,&r,&error);
                /* Optional optimizer allocations may safely retain the original IR. */
                assert(status==MULIB_MUNE_OK || status==MULIB_MUNE_MEMORY);
                if(status==MULIB_MUNE_OK) assert(mulib_mune_result_get(r,0).as.integer==5);
                else assert(!r);
                fail_at=0; mulib_mune_result_destroy(r); mulib_mune_state_destroy(s); mulib_mune_chunk_destroy(chunk);
                assert(!live);
            }
        }
    }
    {
        const char source[]="return 40+2";
        mulib_mune_bundle_module_t module={"answer",source,sizeof(source)-1};
        size_t size=0;
        struct image image={{0},0};
        fail_at=0; calls=0;
        assert(mulib_mune_bundle_write(&module,1,count_bytes,&size,&error)==MULIB_MUNE_OK);
        allocations=calls; assert(!live && size>100);
        for(k=1;k<=allocations;++k) {
            size=0; calls=0; fail_at=k;
            assert(mulib_mune_bundle_write(&module,1,count_bytes,&size,&error)==MULIB_MUNE_MEMORY);
            assert(!live && !size);
        }
        fail_at=0;
        assert(mulib_mune_bundle_write(&module,1,store_bytes,&image,&error)==MULIB_MUNE_OK);
        s=mulib_mune_state_create(NULL); assert(s); calls=0;
        assert(mulib_mune_load_bundle(s,image.bytes,image.size,&error)==MULIB_MUNE_OK);
        allocations=calls; mulib_mune_state_destroy(s); assert(!live);
        for(k=1;k<=allocations;++k) {
            mulib_mune_status_t status;
            fail_at=0; s=mulib_mune_state_create(NULL); assert(s); calls=0; fail_at=k;
            status=mulib_mune_load_bundle(s,image.bytes,image.size,&error);
            assert(status==MULIB_MUNE_OK || status==MULIB_MUNE_MEMORY);
            fail_at=0; mulib_mune_state_destroy(s); assert(!live);
        }
    }
    puts("Mune runtime allocation failures passed");
    return 0;
}
