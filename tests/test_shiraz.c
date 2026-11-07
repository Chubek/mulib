#define _POSIX_C_SOURCE 200809L
#include "../library/shiraz/core.c"
#include "../library/shiraz/runtime.c"
#include "../library/shiraz/kernel32.c"
#include "../library/shiraz/registry.c"
#define MULIB_IMPL_PEBUILDER
#define MULIB_IMPL_OBJFILEBUILDER
#define MULIB_IMPL_OBJFILEWRITER
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_BYCUTILS
#include "mulib-pebuilder.h"
#include <assert.h>

static mulib_shiraz_t *load_code(const unsigned char *code, size_t n) {
    mulib_buffer_t pe={0}; mulib_shiraz_t *v=NULL;
    assert(mulib_pebuilder_text(&pe,code,n,0));
    assert(!mulib_shiraz_load(&v,pe.data,pe.length,NULL,NULL));
    memset(pe.data,0,pe.length); mulib_buffer_free(&pe); /* VM must own its input. */
    return v;
}
static void execution(void) {
    /* sum 5+4+3+2+1, preserving carry across dec, with a stack local. */
    const unsigned char code[]={0x48,0x83,0xec,0x28,0x31,0xc0,0xb9,5,0,0,0,
        0x01,0xc8,0xff,0xc9,0x75,0xfa,0x89,0x44,0x24,0x20,
        0x8b,0x44,0x24,0x20,0x48,0x83,0xc4,0x28,0xc3};
    mulib_shiraz_t *v=load_code(code,sizeof(code)); mulib_strbuf_t aot={0}; uint64_t heap,x=UINT64_C(0x123456789abcdef0),y=0;
    assert(mulib_strbuf_append(&aot,"preserved"));
    assert(mulib_shiraz_emit_c(v,2,&aot)==MULIB_SHIRAZ_ELIMIT && !strcmp(aot.data,"preserved"));
    assert(!mulib_shiraz_emit_c(v,100,&aot)); assert(strstr(aot.data,"switch(mulib_shiraz_cpu"));
    assert(!strstr(aot.data,"mulib_shiraz_run(")); mulib_strbuf_free(&aot);
    assert(mulib_shiraz_run(v,0)==MULIB_SHIRAZ_AGAIN);
    assert(mulib_shiraz_run(v,3)==MULIB_SHIRAZ_AGAIN);
    assert(mulib_shiraz_run(v,100)==MULIB_SHIRAZ_DONE && mulib_shiraz_cpu(v)->exit_code==15);
    assert(mulib_shiraz_run(v,1)==MULIB_SHIRAZ_DONE);
    heap=mulib_shiraz_allocate(v,8); assert(heap && !mulib_shiraz_read(v,heap,&y,8) && !y);
    assert(!mulib_shiraz_write(v,heap,&x,8) && !mulib_shiraz_read(v,heap,&y,8) && x==y);
    assert(!mulib_shiraz_release(v,heap));
    assert(mulib_shiraz_release(v,heap)==MULIB_SHIRAZ_EBOUNDS);
    assert(mulib_shiraz_read(v,heap,&y,8)==MULIB_SHIRAZ_EBOUNDS);
    assert(mulib_shiraz_allocate(v,8)==heap && !mulib_shiraz_read(v,heap,&y,8) && !y);
    assert(mulib_shiraz_read(v,UINT64_MAX,&y,8)==MULIB_SHIRAZ_EBOUNDS);
    assert(mulib_shiraz_write(v,v->entry,&x,1)==MULIB_SHIRAZ_EBOUNDS);
    mulib_shiraz_destroy(v);
    { /* Sign/overflow/parity flags and zero extension of 32-bit writes. */
        const unsigned char flags[]={0xb8,0xff,0xff,0xff,0x7f,0x83,0xc0,1,0xc3};
        v=load_code(flags,sizeof(flags)); v->cpu.reg[0]=UINT64_MAX;
        assert(mulib_shiraz_run(v,2)==MULIB_SHIRAZ_AGAIN);
        assert(v->cpu.reg[0]==UINT64_C(0x80000000) && (v->cpu.flags&2048) && (v->cpu.flags&128) && !(v->cpu.flags&1));
        mulib_shiraz_destroy(v);
    }
    { const unsigned char fault[]={0x0f,0x0b};
        v=load_code(fault,sizeof(fault)); assert(mulib_shiraz_run(v,1)==MULIB_SHIRAZ_EUNSUPPORTED);
        assert(mulib_shiraz_error(v)->address==v->entry); assert(mulib_shiraz_run(v,1)==MULIB_SHIRAZ_EUNSUPPORTED);
        mulib_shiraz_destroy(v);
    }
    { const unsigned char loop[]={0xeb,0xfe};
        v=load_code(loop,sizeof(loop)); assert(!mulib_shiraz_emit_c(v,1,&aot));
        assert(mulib_shiraz_run(v,100)==MULIB_SHIRAZ_AGAIN && v->cpu.steps==100);
        mulib_strbuf_free(&aot); mulib_shiraz_destroy(v);
    }
    { const unsigned char ret[]={0xc3};
        v=load_code(ret,sizeof(ret)); assert(mulib_shiraz_allocate(v,16));
        assert(mulib_shiraz_emit_c(v,10,&aot)==MULIB_SHIRAZ_EINVAL);
        mulib_shiraz_destroy(v);
    }
}
static void malformed(void) {
    mulib_buffer_t pe={0}; mulib_shiraz_t *v=NULL,*sentinel=(mulib_shiraz_t *)(uintptr_t)1;
    mulib_shiraz_options_t o={0}; mulib_shiraz_error_t error; size_t i;
    assert(mulib_pebuilder_text(&pe,"\xc3",1,0));
    for(i=0;i<pe.length;i+=13) {
        v=sentinel; assert(mulib_shiraz_load(&v,pe.data,i,NULL,&error)<0 && v==sentinel);
    }
    o.max_image=100; assert(mulib_shiraz_load(&v,pe.data,pe.length,&o,&error)==MULIB_SHIRAZ_ELIMIT && v==sentinel);
    o.max_image=0; o.image_base=UINT64_C(0x150000000);
    assert(mulib_shiraz_load(&v,pe.data,pe.length,&o,&error)==MULIB_SHIRAZ_EUNSUPPORTED);
    pe.data[132]=0x4c; pe.data[133]=1;
    assert(mulib_shiraz_load(&v,pe.data,pe.length,NULL,&error)==MULIB_SHIRAZ_EUNSUPPORTED);
    mulib_buffer_free(&pe); mulib_shiraz_destroy(NULL);
}
static void registry(void) {
    const char *path="shiraz-test.hive"; mulib_shiraz_runtime_t *r; uint32_t type=0; unsigned char dword[]={42,0,0,0},out[4]; size_t n=1;
    remove(path); r=mulib_shiraz_runtime_create(path); assert(r);
    assert(!mulib_shiraz_registry_set(r,"HKCU\\Software\\Shiraz","Answer",4,dword,4));
    assert(mulib_shiraz_registry_get(r,"hkcu/software/shiraz","ANSWER",&type,out,&n)==MULIB_SHIRAZ_EBOUNDS && n==4 && type==4);
    assert(!mulib_shiraz_registry_get(r,"HKCU\\Software\\Shiraz","answer",&type,out,&n) && !memcmp(out,dword,4));
    assert(!mulib_shiraz_runtime_save(r)); mulib_shiraz_runtime_destroy(r);
    r=mulib_shiraz_runtime_create(path); assert(r); n=0;
    assert(!mulib_shiraz_registry_get(r,"HKCU\\Software\\Shiraz","answer",&type,NULL,&n) && n==4);
    assert(!mulib_shiraz_registry_delete(r,"HKCU\\Software\\Shiraz","answer"));
    assert(mulib_shiraz_registry_get(r,"HKCU\\Software\\Shiraz","answer",&type,NULL,&n)==MULIB_SHIRAZ_EIMPORT);
    assert(mulib_shiraz_registry_set(r,"invalid","x",4,dword,4)==MULIB_SHIRAZ_EINVAL);
    assert(mulib_shiraz_registry_set(r,"HKCU","x",4,dword,3)==MULIB_SHIRAZ_EUNSUPPORTED);
    mulib_shiraz_runtime_destroy(r); remove(path);
}
static void imports(void) {
    unsigned char payload[512]={0}; mulib_buffer_t file={0}; mulib_pe_t pe;
    mulib_shiraz_t *v=NULL; mulib_shiraz_runtime_t *r=mulib_shiraz_runtime_create(NULL);
    mulib_strbuf_t aot={0}; mulib_shiraz_options_t options={0}; size_t directory,i;
    assert(r);
    /* mov ecx,42; call [rip+ExitProcess IAT]. Deliberately no RET: ExitProcess
       is noreturn, so AOT must not decode the adjacent import/data padding. */
    payload[0]=0xb9; payload[1]=42; payload[5]=0xff; payload[6]=0x15;
    mulib_endian_store_le32(payload+7,0x190-11);
    mulib_endian_store_le32(payload+0x100,0x1180);
    mulib_endian_store_le32(payload+0x10c,0x1140);
    mulib_endian_store_le32(payload+0x110,0x1190);
    memcpy(payload+0x140,"KERNEL32.dll",13);
    mulib_endian_store_le32(payload+0x180,0x11c0);
    memcpy(payload+0x1c2,"ExitProcess",12);
    assert(mulib_pebuilder_text(&file,payload,sizeof(payload),0));
    assert(!mulib_pe_open(&pe,file.data,file.length)); directory=pe.directory_offset+8;
    mulib_endian_store_le32(file.data+directory,0x1100);
    mulib_endian_store_le32(file.data+directory+4,40);
    assert(!mulib_shiraz_load(&v,file.data,file.length,NULL,NULL));
    assert(mulib_shiraz_import_count(v)==1);
    assert(!mulib_shiraz_runtime_attach(r,v)); assert(!mulib_shiraz_emit_c(v,10,&aot));
    assert(mulib_shiraz_run(v,10)==MULIB_SHIRAZ_DONE && v->cpu.exit_code==42);
    mulib_shiraz_destroy(v); v=NULL; mulib_strbuf_free(&aot);
    /* Missing host and unknown names are reported, never silently stubbed. */
    assert(!mulib_shiraz_load(&v,file.data,file.length,NULL,NULL));
    assert(mulib_shiraz_run(v,10)==MULIB_SHIRAZ_EIMPORT); mulib_shiraz_destroy(v); v=NULL;
    file.data[512+0x1c2]='?'; assert(!mulib_shiraz_load(&v,file.data,file.length,NULL,NULL));
    assert(mulib_shiraz_runtime_attach(r,v)==MULIB_SHIRAZ_EIMPORT);
    assert(strstr(mulib_shiraz_error(v)->message,"?xitProcess")); mulib_shiraz_destroy(v); v=NULL;
    file.data[512+0x1c2]='E';
    mulib_endian_store_le32(file.data+512+0x110,pe.image_size-4);
    assert(mulib_shiraz_load(&v,file.data,file.length,NULL,NULL)==MULIB_SHIRAZ_EBOUNDS && !v);
    mulib_endian_store_le32(file.data+512+0x110,0x1190);
    mulib_endian_store_le32(file.data+directory+4,20);
    assert(mulib_shiraz_load(&v,file.data,file.length,NULL,NULL)==MULIB_SHIRAZ_EFORMAT && !v);
    mulib_endian_store_le32(file.data+directory+4,40);
    /* Mutation exercises loader cleanup and bounded table/string decoding. */
    options.max_image=65536; options.stack_size=256; options.heap_size=256;
    for(i=0;i<file.length;i+=3) {
        unsigned char saved=file.data[i]; file.data[i]^=0xff;
        if (!mulib_shiraz_load(&v,file.data,file.length,&options,NULL)) { (void)mulib_shiraz_run(v,10); mulib_shiraz_destroy(v); v=NULL; }
        file.data[i]=saved;
    }
    mulib_buffer_free(&file); mulib_shiraz_runtime_destroy(r);
}
int main(void) { execution(); malformed(); registry(); imports(); return 0; }
