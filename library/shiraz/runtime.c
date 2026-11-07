#include "internal.h"

int shiraz_equal(const char *a, const char *b) {
    while (*a && *b) {
        unsigned x=(unsigned char)*a++,y=(unsigned char)*b++;
        if (x>='A' && x<='Z') x+=32;
        if (y>='A' && y<='Z') y+=32;
        if (x!=y) return 0;
    }
    return !*a && !*b;
}
int shiraz_get(mulib_shiraz_t *v, uint64_t address, unsigned width, uint64_t *out) {
    unsigned char bytes[8]; uint64_t x=0; unsigned i; int rc;
    if (!width || width>8) return MULIB_SHIRAZ_EINVAL;
    rc=mulib_shiraz_read(v,address,bytes,width); if (rc) return rc;
    for (i=0;i<width;++i) x|=(uint64_t)bytes[i]<<(i*8);
    *out=x; return 0;
}
int shiraz_put(mulib_shiraz_t *v, uint64_t address, unsigned width, uint64_t x) {
    unsigned char bytes[8]; unsigned i;
    if (!width || width>8) return MULIB_SHIRAZ_EINVAL;
    for(i=0;i<width;++i) bytes[i]=(unsigned char)(x>>(i*8));
    return mulib_shiraz_write(v,address,bytes,width);
}
int shiraz_text(mulib_shiraz_t *v, uint64_t address, char *out, size_t cap, int wide) {
    size_t i; uint64_t x; int rc;
    if (!address) { if (cap) *out=0; return MULIB_SHIRAZ_EINVAL; }
    for(i=0;i<cap;++i) {
        if (address>UINT64_MAX-i*(wide?2u:1u)) return MULIB_SHIRAZ_EBOUNDS;
        rc=shiraz_get(v,address+i*(wide?2u:1u),wide?2:1,&x); if (rc) return rc;
        /* ANSI strings are bytes. W APIs currently accept ASCII UTF-16 only. */
        if (wide && x>127) return MULIB_SHIRAZ_EUNSUPPORTED;
        out[i]=(char)x; if (!x) return 0;
    }
    return MULIB_SHIRAZ_ELIMIT;
}
int shiraz_args(mulib_shiraz_t *v, uint64_t *a, unsigned n) {
    unsigned i; int rc; for(i=0;i<n;++i) { rc=mulib_shiraz_argument(v,i,&a[i]); if (rc) return rc; } return 0;
}
unsigned shiraz_handle_new(mulib_shiraz_runtime_t *r) {
    unsigned i; for(i=4;i<SHIRAZ_HANDLES;++i) if (!r->handles[i].file && !r->handles[i].key) return i; return 0;
}
static shiraz_api shiraz_resolve(const char *dll, const char *name, uint16_t ordinal) {
    shiraz_api api; if (ordinal) return NULL;
    if (!*dll || shiraz_equal(dll,"kernel32.dll") || shiraz_equal(dll,"kernelbase.dll")) {
        api=shiraz_kernel32_find(name); if (api) return api;
    }
    if (!*dll || shiraz_equal(dll,"advapi32.dll")) return shiraz_advapi32_find(name);
    return NULL;
}
static int shiraz_host(void *user, mulib_shiraz_t *v, const char *dll, const char *name, uint16_t ordinal) {
    shiraz_api api=shiraz_resolve(dll,name,ordinal);
    return api?api((mulib_shiraz_runtime_t *)user,v,name):MULIB_SHIRAZ_EIMPORT;
}
mulib_shiraz_runtime_t *mulib_shiraz_runtime_create(const char *path) {
    mulib_shiraz_runtime_t *r=(mulib_shiraz_runtime_t *)calloc(1,sizeof(*r));
    if (!r) return NULL;
    if (path) { if (mulib_kvdb_open(path,1,&r->registry)) { free(r); return NULL; } r->persistent=1; }
    else r->registry=mulib_kvdb_create();
    if (!r->registry) { free(r); return NULL; }
    r->handles[1].file=stdin; r->handles[2].file=stdout; r->handles[3].file=stderr;
    return r;
}
void mulib_shiraz_runtime_destroy(mulib_shiraz_runtime_t *r) {
    unsigned i; if (!r) return;
    for(i=4;i<SHIRAZ_HANDLES;++i) { if (r->handles[i].file) fclose(r->handles[i].file); free(r->handles[i].key); }
    mulib_kvdb_close(r->registry); free(r);
}
int mulib_shiraz_runtime_save(mulib_shiraz_runtime_t *r) {
    if (!r) return MULIB_SHIRAZ_EINVAL;
    return r->persistent && mulib_kvdb_commit(r->registry)!=MULIB_KVDB_OK?MULIB_SHIRAZ_EIO:0;
}
int mulib_shiraz_runtime_attach(mulib_shiraz_runtime_t *r, mulib_shiraz_t *v) {
    size_t i; if (!r || !v) return MULIB_SHIRAZ_EINVAL;
    for(i=0;i<mulib_shiraz_import_count(v);++i) {
        const char *dll,*name; uint16_t ordinal;
        if (mulib_shiraz_import(v,i,&dll,&name,&ordinal)) return MULIB_SHIRAZ_EINVAL;
        if (!shiraz_resolve(dll,name,ordinal)) {
            /* The diagnostic belongs to the VM and is writable by its host. */
            mulib_shiraz_error_t *e=(mulib_shiraz_error_t *)mulib_shiraz_error(v);
            e->code=MULIB_SHIRAZ_EIMPORT; e->address=0;
            (void)snprintf(e->message,sizeof(e->message),"unresolved import %s!%.80s (ordinal %u)",dll,name,ordinal);
            return MULIB_SHIRAZ_EIMPORT;
        }
    }
    return mulib_shiraz_bind(v,shiraz_host,r);
}
