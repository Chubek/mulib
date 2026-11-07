/** @file mulib-shiraz.h
 * @brief Shiraz: Windows AMD64 PE/COFF loading, interpretation and helper-based C AOT.
 * Define MULIB_IMPL_SHIRAZ, PE, COFF, ENDIAN and STRBUF in one C11 TU.
 * The optional Windows runtime/registry API at the end of this header is
 * separately implemented by library/shiraz, not by the implementation switch.
 * See manual/mulib-shiraz.md for the supported machine/API profile.
 */
#ifndef MULIB_SHIRAZ_H
#define MULIB_SHIRAZ_H
#include "mulib-pe.h"
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C" {
#endif
enum mulib_shiraz_result {
    MULIB_SHIRAZ_OK = 0, MULIB_SHIRAZ_DONE = 1, MULIB_SHIRAZ_AGAIN = 2,
    MULIB_SHIRAZ_EINVAL = -1, MULIB_SHIRAZ_EFORMAT = -2,
    MULIB_SHIRAZ_EBOUNDS = -3, MULIB_SHIRAZ_EUNSUPPORTED = -4,
    MULIB_SHIRAZ_ENOMEM = -5, MULIB_SHIRAZ_EIMPORT = -6,
    MULIB_SHIRAZ_EIO = -7, MULIB_SHIRAZ_ELIMIT = -8
};
typedef struct mulib_shiraz mulib_shiraz_t;
typedef struct mulib_shiraz_error {
    int code;
    uint64_t address;
    char message[192];
} mulib_shiraz_error_t;
/** Zero fields select 64 MiB image, 1 MiB stack, 16 MiB heap, preferred PE base
 * (0x140000000 for COFF), and COFF entry symbol "main". Strings are borrowed
 * only during load. Image, stack and heap limits are independent. */
typedef struct mulib_shiraz_options {
    size_t max_image, stack_size, heap_size;
    uint64_t image_base;
    const char *entry_symbol;
} mulib_shiraz_options_t;
/** Register order: RAX RCX RDX RBX RSP RBP RSI RDI R8..R15. */
typedef struct mulib_shiraz_cpu {
    uint64_t reg[16], pc, flags, steps;
    uint32_t exit_code;
    int halted, fault;
} mulib_shiraz_cpu_t;
/** Host runs at the callee entry: RCX/RDX/R8/R9 then stack+40 are arguments.
 * Write RAX for return, or call exit(). Return zero or a negative Shiraz error.
 * DLL/name are borrowed; ordinal imports have an empty name. */
typedef int (*mulib_shiraz_host_fn)(void *, mulib_shiraz_t *, const char *, const char *, uint16_t);
/** Deep-copy and relocate a PE32+ executable or one AMD64 COFF object. Output
 * unchanged on failure. Free an existing output before reusing it. */
int mulib_shiraz_load(mulib_shiraz_t **out, const void *bytes, size_t length,
    const mulib_shiraz_options_t *options, mulib_shiraz_error_t *error);
/** Bounded file-loading convenience; same ownership as load. */
int mulib_shiraz_load_file(mulib_shiraz_t **out, const char *path,
    const mulib_shiraz_options_t *options, mulib_shiraz_error_t *error);
/** NULL-safe destruction; detaching/destroying the borrowed host is caller-owned. */
void mulib_shiraz_destroy(mulib_shiraz_t *vm);
/** Borrow CPU state; changing it is intended for debuggers and host adapters. */
mulib_shiraz_cpu_t *mulib_shiraz_cpu(mulib_shiraz_t *vm);
/** Borrow diagnostic, valid until next operation/destruction. */
const mulib_shiraz_error_t *mulib_shiraz_error(const mulib_shiraz_t *vm);
/** Install a borrowed host callback/context. NULL removes it. */
int mulib_shiraz_bind(mulib_shiraz_t *vm, mulib_shiraz_host_fn host, void *user);
/** Import enumeration; names are owned by vm. */
size_t mulib_shiraz_import_count(const mulib_shiraz_t *vm);
int mulib_shiraz_import(const mulib_shiraz_t *vm, size_t index,
    const char **dll, const char **name, uint16_t *ordinal);
/** Checked guest-memory access. No native pointers are exposed to the guest.
 * Writes to executable sections are rejected, preserving AOT code identity. */
int mulib_shiraz_read(mulib_shiraz_t *vm, uint64_t address, void *out, size_t bytes);
int mulib_shiraz_write(mulib_shiraz_t *vm, uint64_t address, const void *data, size_t bytes);
/** Read a Microsoft x64 integer/pointer argument (index zero is RCX). */
int mulib_shiraz_argument(mulib_shiraz_t *vm, unsigned index, uint64_t *out);
/** Allocate zeroed, 16-byte aligned guest heap memory; zero on exhaustion. */
uint64_t mulib_shiraz_allocate(mulib_shiraz_t *vm, size_t bytes);
/** Release an exact guest heap allocation; invalid/double frees fail. Freed
 * blocks are coalesced and reused. The VM releases every remaining allocation. */
int mulib_shiraz_release(mulib_shiraz_t *vm, uint64_t address);
/** Halt with a Windows 32-bit exit status (Linux CLI exposes low 8 bits). */
void mulib_shiraz_exit(mulib_shiraz_t *vm, uint32_t code);
/** Execute at most budget instructions/import calls. AGAIN is resumable;
 * faults are sticky. A zero budget performs no work. */
int mulib_shiraz_run(mulib_shiraz_t *vm, size_t budget);
/** Internal representation shared with generated C. Do not fabricate records. */
typedef struct mulib_shiraz_operand {
    unsigned kind, reg, base, index, scale, width;
    uint64_t value;
    int64_t displacement;
} mulib_shiraz_operand_t;
typedef struct mulib_shiraz_instruction {
    unsigned opcode, width, condition, length;
    uint64_t address, next, target;
    mulib_shiraz_operand_t dst, src;
} mulib_shiraz_instruction_t;
/** Decode an executable guest address without changing CPU state. */
int mulib_shiraz_decode(mulib_shiraz_t *vm, uint64_t address, mulib_shiraz_instruction_t *out);
/** Execute one decoded record. Used by AOT; checks the instruction address. */
int mulib_shiraz_execute(mulib_shiraz_t *vm, const mulib_shiraz_instruction_t *instruction);
/** Handle a return sentinel or imported function at current PC. Returns
 * EUNSUPPORTED for an address outside the translation's known code graph. */
int mulib_shiraz_dispatch(mulib_shiraz_t *vm);
/** Emit a complete C program with predecoded reachable instructions and an
 * explicit PC dispatcher. No runtime instruction decoding/fallback. Includes
 * original input as data for relocation/import setup. Atomic output; max_nodes
 * zero selects 100000. Link generated C with libshiraz and libshiraz-win32.
 * Entry must still be at its initial state. */
int mulib_shiraz_emit_c(mulib_shiraz_t *vm, size_t max_nodes, mulib_strbuf_t *out);

/* Separately compiled Windows runtime, shared by interpreted and AOT programs. */
typedef struct mulib_shiraz_runtime mulib_shiraz_runtime_t;
/** Create a runtime. NULL registry_path selects an in-memory registry; otherwise
 * an existing Shiraz hive is loaded, or a new one is created on explicit save.
 * Runtime owns handles, files and registry; it must outlive attached VMs. */
mulib_shiraz_runtime_t *mulib_shiraz_runtime_create(const char *registry_path);
void mulib_shiraz_runtime_destroy(mulib_shiraz_runtime_t *runtime);
/** Resolve every import before attaching; unsupported imports fail by name. */
int mulib_shiraz_runtime_attach(mulib_shiraz_runtime_t *runtime, mulib_shiraz_t *vm);
/** Explicitly persist registry changes with atomic replacement. */
int mulib_shiraz_runtime_save(mulib_shiraz_runtime_t *runtime);
/** Case-insensitive ASCII key/value names; HKCU/HKLM/HKCR/HKU roots. Types are
 * Windows REG_SZ=1, REG_BINARY=3 and REG_DWORD=4. Copies data. */
int mulib_shiraz_registry_set(mulib_shiraz_runtime_t *, const char *key,
    const char *name, uint32_t type, const void *data, size_t bytes);
/** Buffer size is in/out; NULL data queries size. Insufficient buffer returns
 * EBOUNDS and required size. Missing value returns EIMPORT. */
int mulib_shiraz_registry_get(mulib_shiraz_runtime_t *, const char *key,
    const char *name, uint32_t *type, void *data, size_t *bytes);
int mulib_shiraz_registry_delete(mulib_shiraz_runtime_t *, const char *key, const char *name);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SHIRAZ) && !defined(MULIB_SHIRAZ_IMPLEMENTED)
#define MULIB_SHIRAZ_IMPLEMENTED
#include <limits.h>
enum { MULIB_SHIRAZ_NOP_, MULIB_SHIRAZ_MOV_, MULIB_SHIRAZ_LEA_,
    MULIB_SHIRAZ_ADD_, MULIB_SHIRAZ_OR_, MULIB_SHIRAZ_AND_, MULIB_SHIRAZ_SUB_,
    MULIB_SHIRAZ_XOR_, MULIB_SHIRAZ_CMP_, MULIB_SHIRAZ_TEST_, MULIB_SHIRAZ_PUSH_,
    MULIB_SHIRAZ_POP_, MULIB_SHIRAZ_CALL_, MULIB_SHIRAZ_JMP_, MULIB_SHIRAZ_JCC_,
    MULIB_SHIRAZ_RET_, MULIB_SHIRAZ_LEAVE_, MULIB_SHIRAZ_MOVZX_, MULIB_SHIRAZ_MOVSX_,
    MULIB_SHIRAZ_IMUL_, MULIB_SHIRAZ_INC_, MULIB_SHIRAZ_DEC_ };
typedef struct mulib_shiraz_region_ { uint64_t address; size_t size; unsigned flags; } mulib_shiraz_region_;
typedef struct mulib_shiraz_import_ { char dll[128], name[256]; uint16_t ordinal; } mulib_shiraz_import_;
typedef struct mulib_shiraz_block_ {
    size_t offset, size;
    int available;
    struct mulib_shiraz_block_ *next;
} mulib_shiraz_block_;
struct mulib_shiraz {
    unsigned char *image, *stack, *heap, *file;
    size_t image_size, stack_size, heap_size, heap_used, file_size, region_count, import_count;
    uint64_t base, entry;
    mulib_shiraz_region_ regions[96];
    mulib_shiraz_import_ *imports;
    mulib_shiraz_block_ *blocks;
    mulib_shiraz_cpu_t cpu;
    mulib_shiraz_error_t error;
    mulib_shiraz_host_fn host;
    void *user;
    int changed;
    char entry_symbol[256];
};
#define MULIB_SHIRAZ_STACK_ UINT64_C(0x700000000000)
#define MULIB_SHIRAZ_HEAP_ UINT64_C(0x600000000000)
#define MULIB_SHIRAZ_IMPORT_ UINT64_C(0xffff000000000000)
static int mulib_shiraz_fail_(mulib_shiraz_t *v, int code, uint64_t at, const char *text) {
    if (v) { v->error.code = code; v->error.address = at;
        (void)snprintf(v->error.message, sizeof(v->error.message), "%.191s", text); }
    return code;
}
static int mulib_shiraz_load_error_(mulib_shiraz_error_t *e, int code, const char *text) {
    if (e) { memset(e,0,sizeof(*e)); e->code=code; (void)snprintf(e->message,sizeof(e->message),"%.191s",text); }
    return code;
}
static uint64_t mulib_shiraz_u_(const unsigned char *p, unsigned n) {
    uint64_t x = 0; unsigned i; for (i = 0; i < n; ++i) x |= (uint64_t)p[i] << (8*i); return x;
}
static void mulib_shiraz_put_(unsigned char *p, uint64_t x, unsigned n) {
    unsigned i; for (i = 0; i < n; ++i) p[i] = (unsigned char)(x >> (8*i));
}
static uint64_t mulib_shiraz_sign_(uint64_t x, unsigned n) {
    uint64_t bit = UINT64_C(1) << (n*8-1); return (x ^ bit) - bit;
}
static int mulib_shiraz_range_(uint64_t address, size_t n, uint64_t base, size_t size) {
    return address >= base && address - base <= size && n <= size - (size_t)(address-base);
}
static unsigned char *mulib_shiraz_memory_(mulib_shiraz_t *v, uint64_t a, size_t n, unsigned permission) {
    size_t i;
    if (mulib_shiraz_range_(a,n,v->base,v->image_size)) {
        for (i = 0; i < v->region_count; ++i) {
            mulib_shiraz_region_ *r = &v->regions[i];
            if (mulib_shiraz_range_(a,n,r->address,r->size) && (r->flags & permission) == permission)
                return v->image + (size_t)(a-v->base);
        }
    }
    if (permission != 4 && mulib_shiraz_range_(a,n,MULIB_SHIRAZ_STACK_,v->stack_size))
        return v->stack + (size_t)(a-MULIB_SHIRAZ_STACK_);
    if (permission != 4 && mulib_shiraz_range_(a,n,MULIB_SHIRAZ_HEAP_,v->heap_used)) {
        mulib_shiraz_block_ *b;
        for (b=v->blocks;b;b=b->next)
            if (!b->available && mulib_shiraz_range_(a,n,MULIB_SHIRAZ_HEAP_+b->offset,b->size))
                return v->heap + (size_t)(a-MULIB_SHIRAZ_HEAP_);
    }
    return NULL;
}
int mulib_shiraz_read(mulib_shiraz_t *v, uint64_t a, void *out, size_t n) {
    unsigned char *p; if (!v || (!out && n)) return MULIB_SHIRAZ_EINVAL;
    p = mulib_shiraz_memory_(v,a,n,1);
    if (!p) return mulib_shiraz_fail_(v,MULIB_SHIRAZ_EBOUNDS,a,"guest read outside readable memory");
    if (n) memcpy(out,p,n);
    return 0;
}
int mulib_shiraz_write(mulib_shiraz_t *v, uint64_t a, const void *data, size_t n) {
    unsigned char *p; if (!v || (!data && n)) return MULIB_SHIRAZ_EINVAL;
    p = mulib_shiraz_memory_(v,a,n,2);
    if (!p) return mulib_shiraz_fail_(v,MULIB_SHIRAZ_EBOUNDS,a,"guest write outside writable memory");
    if (n) { memcpy(p,data,n); v->changed=1; }
    return 0;
}
static int mulib_shiraz_read_int_(mulib_shiraz_t *v, uint64_t a, unsigned n, uint64_t *out) {
    unsigned char p[8]; int rc = mulib_shiraz_read(v,a,p,n); if (!rc) *out = mulib_shiraz_u_(p,n); return rc;
}
static int mulib_shiraz_write_int_(mulib_shiraz_t *v, uint64_t a, unsigned n, uint64_t x) {
    unsigned char p[8]; mulib_shiraz_put_(p,x,n); return mulib_shiraz_write(v,a,p,n);
}
int mulib_shiraz_argument(mulib_shiraz_t *v, unsigned i, uint64_t *out) {
    static const unsigned regs[] = {1,2,8,9};
    if (!v || !out || i > 255) return MULIB_SHIRAZ_EINVAL;
    if (i < 4) { *out = v->cpu.reg[regs[i]]; return 0; }
    if (v->cpu.reg[4] > UINT64_MAX - 8*(uint64_t)(i+1)) return MULIB_SHIRAZ_EBOUNDS;
    return mulib_shiraz_read_int_(v,v->cpu.reg[4]+8*(uint64_t)(i+1),8,out);
}
uint64_t mulib_shiraz_allocate(mulib_shiraz_t *v, size_t n) {
    mulib_shiraz_block_ *b,**tail;
    if (!v || !n || n > SIZE_MAX-15) return 0;
    n = (n+15)&~(size_t)15;
    for(tail=&v->blocks;(b=*tail)!=NULL;tail=&b->next) {
        if (b->available && b->size>=n) {
            if (b->size>n) {
                mulib_shiraz_block_ *rest=(mulib_shiraz_block_ *)malloc(sizeof(*rest));
                if (!rest) return 0;
                rest->offset=b->offset+n; rest->size=b->size-n; rest->available=1; rest->next=b->next;
                b->next=rest; b->size=n;
            }
            b->available=0; v->changed=1; memset(v->heap+b->offset,0,n); return MULIB_SHIRAZ_HEAP_+b->offset;
        }
    }
    if (n>v->heap_size-v->heap_used) return 0;
    b=(mulib_shiraz_block_ *)calloc(1,sizeof(*b)); if (!b) return 0;
    b->offset=v->heap_used; b->size=n; *tail=b; v->heap_used+=n; v->changed=1;
    return MULIB_SHIRAZ_HEAP_+b->offset;
}
int mulib_shiraz_release(mulib_shiraz_t *v, uint64_t address) {
    mulib_shiraz_block_ *b; int found=0;
    if (!v) return MULIB_SHIRAZ_EINVAL;
    for(b=v->blocks;b;b=b->next) if (MULIB_SHIRAZ_HEAP_+b->offset==address && !b->available) { b->available=1; found=1; break; }
    if (!found) return MULIB_SHIRAZ_EBOUNDS;
    for(b=v->blocks;b && b->next;) {
        if (b->available && b->next->available) { mulib_shiraz_block_ *next=b->next; b->size+=next->size; b->next=next->next; free(next); }
        else b=b->next;
    }
    return 0;
}
void mulib_shiraz_exit(mulib_shiraz_t *v, uint32_t code) { if (v) { v->cpu.exit_code=code; v->cpu.halted=1; } }
mulib_shiraz_cpu_t *mulib_shiraz_cpu(mulib_shiraz_t *v) { return v ? &v->cpu : NULL; }
const mulib_shiraz_error_t *mulib_shiraz_error(const mulib_shiraz_t *v) { return v ? &v->error : NULL; }
int mulib_shiraz_bind(mulib_shiraz_t *v, mulib_shiraz_host_fn f, void *u) {
    if (!v) return MULIB_SHIRAZ_EINVAL;
    v->host=f; v->user=u; return 0;
}
size_t mulib_shiraz_import_count(const mulib_shiraz_t *v) { return v ? v->import_count : 0; }
int mulib_shiraz_import(const mulib_shiraz_t *v, size_t i, const char **d, const char **n, uint16_t *o) {
    if (!v || !d || !n || !o) return MULIB_SHIRAZ_EINVAL;
    if (i >= v->import_count) return MULIB_SHIRAZ_EBOUNDS;
    *d=v->imports[i].dll; *n=v->imports[i].name; *o=v->imports[i].ordinal; return 0;
}
void mulib_shiraz_destroy(mulib_shiraz_t *v) {
    if (v) { mulib_shiraz_block_ *b=v->blocks;
        while(b) { mulib_shiraz_block_ *next=b->next; free(b); b=next; }
        free(v->image); free(v->stack); free(v->heap); free(v->file); free(v->imports); free(v); }
}
static int mulib_shiraz_add_import_(mulib_shiraz_t *v, const char *dll, const char *name, uint16_t ordinal, uint64_t *address) {
    size_t i; mulib_shiraz_import_ *p;
    if (strlen(dll)>=128 || strlen(name)>=256) return MULIB_SHIRAZ_ELIMIT;
    for (i=0;i<v->import_count;++i) if (!strcmp(dll,v->imports[i].dll) && !strcmp(name,v->imports[i].name)
        && ordinal==v->imports[i].ordinal) { *address=MULIB_SHIRAZ_IMPORT_+16*i; return 0; }
    if (v->import_count==4096) return MULIB_SHIRAZ_ELIMIT;
    p=(mulib_shiraz_import_ *)realloc(v->imports,(v->import_count+1)*sizeof(*p));
    if (!p) return MULIB_SHIRAZ_ENOMEM;
    v->imports=p; p+=v->import_count; strcpy(p->dll,dll); strcpy(p->name,name); p->ordinal=ordinal;
    *address=MULIB_SHIRAZ_IMPORT_+16*v->import_count++; return 0;
}
static unsigned char *mulib_shiraz_rva_(mulib_shiraz_t *v, uint64_t rva, size_t n) {
    return rva <= v->image_size && n <= v->image_size-(size_t)rva ? v->image+(size_t)rva : NULL;
}
static const char *mulib_shiraz_string_(mulib_shiraz_t *v, uint64_t rva, size_t max) {
    unsigned char *p=mulib_shiraz_rva_(v,rva,1); size_t n;
    if (!p) return NULL;
    n=v->image_size-(size_t)rva; if (n>max) n=max;
    return memchr(p,0,n) ? (const char *)p : NULL;
}
static int mulib_shiraz_region_add_(mulib_shiraz_t *v, uint64_t rva, size_t n, uint32_t flags) {
    size_t i; mulib_shiraz_region_ *r;
    if (!n) return 0;
    if (v->region_count==96 || !mulib_shiraz_rva_(v,rva,n)) return MULIB_SHIRAZ_EFORMAT;
    for (i=0;i<v->region_count;++i) {
        r=&v->regions[i];
        if (v->base+rva < r->address+r->size && r->address < v->base+rva+n) return MULIB_SHIRAZ_EFORMAT;
    }
    r=&v->regions[v->region_count++]; r->address=v->base+rva; r->size=n;
    r->flags=(flags & 0x40000000u ? 1u:0u) | (flags & 0x20000000u ? 5u : flags & 0x80000000u ? 2u:0u);
    return 0;
}
static int mulib_shiraz_pe_(mulib_shiraz_t *v, const mulib_shiraz_options_t *o) {
    mulib_pe_t p; mulib_pe_section_t s; mulib_pe_directory_t d; size_t i; int rc;
    rc=mulib_pe_open(&p,v->file,v->file_size); if (rc) return MULIB_SHIRAZ_EFORMAT;
    if (p.bits!=64 || p.coff.machine!=0x8664 || (p.coff.characteristics&0x2000) ||
        !(p.coff.characteristics&2) || (p.subsystem!=2 && p.subsystem!=3)) return MULIB_SHIRAZ_EUNSUPPORTED;
    if (!p.entry || p.coff.section_count>95 || p.image_size>o->max_image) return MULIB_SHIRAZ_ELIMIT;
    v->base=o->image_base ? o->image_base:p.image_base; v->image_size=p.image_size;
    if (!v->base || v->base>=MULIB_SHIRAZ_HEAP_ || v->image_size>MULIB_SHIRAZ_HEAP_-v->base) return MULIB_SHIRAZ_EBOUNDS;
    v->image=(unsigned char *)calloc(1,v->image_size); if (!v->image) return MULIB_SHIRAZ_ENOMEM;
    memcpy(v->image,v->file,p.headers_size);
    rc=mulib_shiraz_region_add_(v,0,p.headers_size,0x40000000); if (rc) return rc;
    for (i=0;i<p.coff.section_count;++i) {
        size_t span; rc=mulib_pe_section(&p,i,&s); if (rc) return MULIB_SHIRAZ_EFORMAT;
        span=s.virtual_size>s.size?s.virtual_size:s.size;
        rc=mulib_shiraz_region_add_(v,s.address,span,s.flags); if (rc) return rc;
        if (s.size) memcpy(v->image+s.address,v->file+s.offset,s.size);
    }
    v->entry=v->base+p.entry;
    if (!mulib_shiraz_memory_(v,v->entry,1,4)) return MULIB_SHIRAZ_EFORMAT;
    /* TLS, delay imports, CLR and load-config initialization need additional
       runtime machinery; reject instead of silently skipping initialization. */
    for (i=0;i<p.directory_count;++i) {
        rc=mulib_pe_directory(&p,i,&d); if (rc) return MULIB_SHIRAZ_EFORMAT;
        if (d.size && (i==9 || i==10 || i==13 || i==14)) return MULIB_SHIRAZ_EUNSUPPORTED;
        if (d.size && i!=4 && !mulib_shiraz_rva_(v,d.address,d.size)) return MULIB_SHIRAZ_EBOUNDS;
    }
    memset(&d,0,sizeof(d)); if (p.directory_count>5) (void)mulib_pe_directory(&p,5,&d);
    if (v->base!=p.image_base && !d.size) return MULIB_SHIRAZ_EUNSUPPORTED;
    for (i=0;i<d.size;) {
        unsigned char *b; uint32_t page,n; size_t j;
        if (d.size-i<8) return MULIB_SHIRAZ_EFORMAT;
        b=v->image+d.address+i; page=mulib_endian_load_le32(b); n=mulib_endian_load_le32(b+4);
        if (n<8 || (n&1) || n>d.size-i) return MULIB_SHIRAZ_EFORMAT;
        for (j=8;j<n;j+=2) {
            unsigned r=(unsigned)mulib_shiraz_u_(b+j,2), type=r>>12; unsigned char *q;
            if (!type) continue;
            if (type!=10) return MULIB_SHIRAZ_EUNSUPPORTED;
            q=mulib_shiraz_rva_(v,(uint64_t)page+(r&4095),8); if (!q) return MULIB_SHIRAZ_EBOUNDS;
            mulib_shiraz_put_(q,mulib_shiraz_u_(q,8)+v->base-p.image_base,8);
        }
        i+=n;
    }
    memset(&d,0,sizeof(d)); if (p.directory_count>1) (void)mulib_pe_directory(&p,1,&d);
    for (i=0;i<d.size;i+=20) {
        unsigned char *b; uint32_t lookup,iat,name; size_t j; const char *dll; char dll_copy[128];
        if (d.size-i<20) return MULIB_SHIRAZ_EFORMAT;
        b=v->image+d.address+i;
        if (!mulib_shiraz_u_(b,8) && !mulib_shiraz_u_(b+8,8) && !mulib_shiraz_u_(b+16,4)) break;
        lookup=mulib_endian_load_le32(b); name=mulib_endian_load_le32(b+12); iat=mulib_endian_load_le32(b+16);
        if (!lookup) lookup=iat;
        dll=mulib_shiraz_string_(v,name,128); if (!dll || !*dll || !iat || !lookup) return MULIB_SHIRAZ_EFORMAT;
        strcpy(dll_copy,dll);
        for (j=0;;j+=8) {
            unsigned char *q=mulib_shiraz_rva_(v,(uint64_t)lookup+j,8), *dest;
            const char *symbol=""; uint16_t ordinal=0; uint64_t value,addr;
            if (!q) return MULIB_SHIRAZ_EBOUNDS;
            value=mulib_shiraz_u_(q,8); if (!value) break;
            if (value>>63) { if (value & UINT64_C(0x7fffffffffff0000)) return MULIB_SHIRAZ_EFORMAT; ordinal=(uint16_t)value; }
            else { if (value>UINT32_MAX) return MULIB_SHIRAZ_EFORMAT; symbol=mulib_shiraz_string_(v,value+2,256); if (!symbol || !*symbol) return MULIB_SHIRAZ_EFORMAT; }
            rc=mulib_shiraz_add_import_(v,dll_copy,symbol,ordinal,&addr); if (rc) return rc;
            dest=mulib_shiraz_rva_(v,(uint64_t)iat+j,8); if (!dest) return MULIB_SHIRAZ_EBOUNDS;
            mulib_shiraz_put_(dest,addr,8);
            if (j/8>=4096) return MULIB_SHIRAZ_ELIMIT;
        }
    }
    if (d.size && i>=d.size) return MULIB_SHIRAZ_EFORMAT;
    return 0;
}
static int mulib_shiraz_coff_(mulib_shiraz_t *v, const mulib_shiraz_options_t *o) {
    mulib_coff_t c; mulib_coff_section_t s; mulib_coff_symbol_t sym;
    uint64_t offsets[96], *values=NULL; size_t i,j,total=4096; int rc=MULIB_SHIRAZ_EFORMAT;
    if (mulib_coff_open(&c,v->file,v->file_size)) return rc;
    if (c.machine!=0x8664 || c.optional_size || !c.section_count || c.section_count>95) return MULIB_SHIRAZ_EUNSUPPORTED;
    v->base=o->image_base?o->image_base:UINT64_C(0x140000000);
    for (i=0;i<c.section_count;++i) {
        if (mulib_coff_section(&c,i,&s)) return rc;
        /* A single object's COMDAT sections are retained. Link directives need
           a multi-object/default-library linker and are not interpreted. */
        { const char *name; size_t length;
            if (mulib_coff_section_name(&c,i,&name,&length)) return MULIB_SHIRAZ_EFORMAT;
            if (s.size && length==8 && !memcmp(name,".drectve",8)) return MULIB_SHIRAZ_EUNSUPPORTED;
        }
        offsets[i]=total;
        if (s.size>o->max_image || total>o->max_image-s.size || total+s.size>SIZE_MAX-4095) return MULIB_SHIRAZ_ELIMIT;
        total=(total+s.size+4095)&~(size_t)4095;
    }
    if (c.symbol_count>1000000 || c.symbol_count>(SIZE_MAX-total)/16 || total+c.symbol_count*16>o->max_image) return MULIB_SHIRAZ_ELIMIT;
    v->image_size=total+c.symbol_count*16;
    if (!v->base || v->base>=MULIB_SHIRAZ_HEAP_ || v->image_size>MULIB_SHIRAZ_HEAP_-v->base) return MULIB_SHIRAZ_EBOUNDS;
    v->image=(unsigned char *)calloc(1,v->image_size);
    values=(uint64_t *)calloc(c.symbol_count?c.symbol_count:1,sizeof(*values));
    if (!v->image || !values) { rc=MULIB_SHIRAZ_ENOMEM; goto done; }
    for (i=0;i<c.section_count;++i) {
        (void)mulib_coff_section(&c,i,&s);
        rc=mulib_shiraz_region_add_(v,offsets[i],s.size,s.flags); if (rc) goto done;
        if (s.size && s.offset) memcpy(v->image+offsets[i],v->file+s.offset,s.size);
    }
    if (c.symbol_count) { rc=mulib_shiraz_region_add_(v,total,c.symbol_count*16,0x60000000); if (rc) goto done; }
    for (i=0;i<c.symbol_count;i+=1+sym.auxiliary_count) {
        const char *name; size_t n;
        if (mulib_coff_symbol(&c,i,&sym) || mulib_coff_symbol_name(&c,i,&name,&n)) { rc=MULIB_SHIRAZ_EFORMAT; goto done; }
        if (sym.section_index>0) {
            (void)mulib_coff_section(&c,(size_t)sym.section_index-1,&s);
            if (sym.value>s.size) { rc=MULIB_SHIRAZ_EBOUNDS; goto done; }
            values[i]=v->base+offsets[sym.section_index-1]+sym.value;
            if (strlen(v->entry_symbol)==n && !memcmp(name,v->entry_symbol,n)) v->entry=values[i];
        } else if (sym.section_index==-1) values[i]=sym.value;
        else if (!sym.section_index && sym.storage_class==2 && !sym.value) {
            char text[256]; uint64_t address; int indirect=n>6 && !memcmp(name,"__imp_",6);
            if (indirect) { name+=6; n-=6; }
            if (!n || n>=sizeof(text)) { rc=MULIB_SHIRAZ_ELIMIT; goto done; }
            memcpy(text,name,n); text[n]=0;
            rc=mulib_shiraz_add_import_(v,"",text,0,&address); if (rc) goto done;
            v->image[total+16*i]=0xff; v->image[total+16*i+1]=0x25;
            mulib_shiraz_put_(v->image+total+16*i+6,address,8);
            values[i]=v->base+total+16*i+(indirect?6:0);
        } else if (sym.section_index!=-2) { rc=MULIB_SHIRAZ_EUNSUPPORTED; goto done; }
    }
    for (i=0;i<c.section_count;++i) {
        (void)mulib_coff_section(&c,i,&s);
        for (j=0;j<s.relocation_count;++j) {
            const unsigned char *r=v->file+s.relocation_offset+j*10; uint32_t at=mulib_endian_load_le32(r), si=mulib_endian_load_le32(r+4);
            unsigned type=(unsigned)mulib_shiraz_u_(r+8,2), width=type==1?8:4; uint64_t value; unsigned char *q;
            if (!type) continue;
            if (si>=c.symbol_count || mulib_coff_symbol(&c,si,&sym) || at>s.size || width>s.size-at) { rc=MULIB_SHIRAZ_EBOUNDS; goto done; }
            q=v->image+offsets[i]+at; value=values[si];
            if (type==1) value+=mulib_shiraz_u_(q,8);
            else if (type==2 || type==3) {
                value+=mulib_shiraz_u_(q,4); if (type==3) value-=v->base;
                if (value>UINT32_MAX) { rc=MULIB_SHIRAZ_EBOUNDS; goto done; }
            } else if (type>=4 && type<=9) {
                value+=mulib_shiraz_sign_(mulib_shiraz_u_(q,4),4)-(v->base+offsets[i]+at+4+type-4);
                if (value!=mulib_shiraz_sign_(value&UINT32_MAX,4)) { rc=MULIB_SHIRAZ_EUNSUPPORTED; goto done; }
            } else { rc=MULIB_SHIRAZ_EUNSUPPORTED; goto done; }
            mulib_shiraz_put_(q,value,width);
        }
    }
    rc=mulib_shiraz_memory_(v,v->entry,1,4)?0:MULIB_SHIRAZ_EFORMAT;
done: free(values); return rc;
}
int mulib_shiraz_load(mulib_shiraz_t **out, const void *bytes, size_t n, const mulib_shiraz_options_t *options, mulib_shiraz_error_t *error) {
    mulib_shiraz_options_t o={0}; mulib_shiraz_t *v; int rc;
    if (error) memset(error,0,sizeof(*error));
    if (!out || !bytes || !n) return mulib_shiraz_load_error_(error,MULIB_SHIRAZ_EINVAL,"empty input or invalid load arguments");
    if (options) o=*options;
    if (!o.max_image) o.max_image=64u*1024u*1024u;
    if (!o.stack_size) o.stack_size=1024u*1024u;
    if (!o.heap_size) o.heap_size=16u*1024u*1024u;
    if (!o.entry_symbol) o.entry_symbol="main";
    if (n>o.max_image || o.stack_size<256 || o.stack_size>UINT32_MAX || o.heap_size>UINT32_MAX || strlen(o.entry_symbol)>=256)
        return mulib_shiraz_load_error_(error,MULIB_SHIRAZ_ELIMIT,"image, stack, heap or entry-name limit exceeded");
    v=(mulib_shiraz_t *)calloc(1,sizeof(*v)); if (!v) return mulib_shiraz_load_error_(error,MULIB_SHIRAZ_ENOMEM,"VM allocation failed");
    v->file=(unsigned char *)malloc(n); if (!v->file) { mulib_shiraz_destroy(v); return mulib_shiraz_load_error_(error,MULIB_SHIRAZ_ENOMEM,"input allocation failed"); }
    memcpy(v->file,bytes,n); v->file_size=n; strcpy(v->entry_symbol,o.entry_symbol);
    rc=n>=2 && !memcmp(bytes,"MZ",2)?mulib_shiraz_pe_(v,&o):mulib_shiraz_coff_(v,&o);
    if (!rc) {
        v->stack_size=o.stack_size; v->heap_size=o.heap_size;
        v->stack=(unsigned char *)calloc(1,v->stack_size); v->heap=(unsigned char *)calloc(1,v->heap_size);
        if (!v->stack || !v->heap) rc=MULIB_SHIRAZ_ENOMEM;
        v->cpu.pc=v->entry; v->cpu.flags=2;
        v->cpu.reg[4]=MULIB_SHIRAZ_STACK_+(v->stack_size&~(size_t)15)-40;
    }
    if (rc) {
        mulib_shiraz_fail_(v,rc,0,"unsupported or malformed PE/COFF image, relocation, or entry symbol");
        if (error) *error=v->error;
        mulib_shiraz_destroy(v); return rc;
    }
    *out=v; return 0;
}
int mulib_shiraz_load_file(mulib_shiraz_t **out, const char *path, const mulib_shiraz_options_t *o, mulib_shiraz_error_t *e) {
    FILE *f; long length; unsigned char *p; size_t n,limit=o && o->max_image?o->max_image:64u*1024u*1024u; int rc;
    if (!out || !path) return mulib_shiraz_load_error_(e,MULIB_SHIRAZ_EINVAL,"invalid file-loading arguments");
    f=fopen(path,"rb"); if (!f) return mulib_shiraz_load_error_(e,MULIB_SHIRAZ_EIO,"cannot open input file");
    if (fseek(f,0,SEEK_END) || (length=ftell(f))<0 || fseek(f,0,SEEK_SET)) { fclose(f); return mulib_shiraz_load_error_(e,MULIB_SHIRAZ_EIO,"cannot determine input file size"); }
    if ((uint64_t)length>limit) { fclose(f); return mulib_shiraz_load_error_(e,MULIB_SHIRAZ_ELIMIT,"input file exceeds image limit"); }
    n=(size_t)length; p=(unsigned char *)malloc(n?n:1); if (!p) { fclose(f); return mulib_shiraz_load_error_(e,MULIB_SHIRAZ_ENOMEM,"input allocation failed"); }
    rc=fread(p,1,n,f)==n?0:MULIB_SHIRAZ_EIO; if (fclose(f)) rc=MULIB_SHIRAZ_EIO;
    if (!rc) rc=mulib_shiraz_load(out,p,n,o,e);
    else (void)mulib_shiraz_load_error_(e,rc,"cannot read input file");
    free(p); return rc;
}

typedef struct mulib_shiraz_decoder_ { unsigned char bytes[15]; size_t n,at; unsigned rex; int bad; } mulib_shiraz_decoder_;
static uint64_t mulib_shiraz_take_(mulib_shiraz_decoder_ *d, unsigned n) {
    uint64_t x; if (n>d->n-d->at) { d->bad=1; return 0; }
    x=mulib_shiraz_u_(d->bytes+d->at,n); d->at+=n; return x;
}
static mulib_shiraz_operand_t mulib_shiraz_reg_(unsigned r, unsigned w) {
    mulib_shiraz_operand_t o={0}; o.kind=1; o.reg=r; o.width=w; return o;
}
static mulib_shiraz_operand_t mulib_shiraz_imm_(uint64_t x, unsigned w) {
    mulib_shiraz_operand_t o={0}; o.kind=3; o.value=x; o.width=w; return o;
}
static mulib_shiraz_operand_t mulib_shiraz_rm_(mulib_shiraz_decoder_ *d, unsigned m, unsigned w) {
    mulib_shiraz_operand_t o={0}; unsigned mod=m>>6,rm=m&7;
    if (mod==3) { if (w==1 && !d->rex && rm>=4) d->bad=1; return mulib_shiraz_reg_(rm+((d->rex&1)?8:0),w); }
    o.kind=2; o.base=16; o.index=16; o.width=w; o.scale=1;
    if (rm==4) {
        unsigned sib=(unsigned)mulib_shiraz_take_(d,1), base=sib&7, index=(sib>>3)&7;
        o.scale=1u<<(sib>>6);
        if (index!=4 || (d->rex&2)) o.index=index+((d->rex&2)?8:0);
        if (base==5 && !mod) o.displacement=(int64_t)(int32_t)(uint32_t)mulib_shiraz_take_(d,4);
        else o.base=base+((d->rex&1)?8:0);
    } else if (rm==5 && !mod) { o.base=17; o.displacement=(int64_t)(int32_t)(uint32_t)mulib_shiraz_take_(d,4); }
    else o.base=rm+((d->rex&1)?8:0);
    if (mod==1) o.displacement=(int8_t)(uint8_t)mulib_shiraz_take_(d,1);
    if (mod==2) o.displacement=(int32_t)(uint32_t)mulib_shiraz_take_(d,4);
    return o;
}
int mulib_shiraz_decode(mulib_shiraz_t *v, uint64_t address, mulib_shiraz_instruction_t *out) {
    mulib_shiraz_decoder_ d; mulib_shiraz_instruction_t p; unsigned op,w=4,m,g,reg; int relative=0,known=1;
    static const unsigned alu[]={MULIB_SHIRAZ_ADD_,MULIB_SHIRAZ_OR_,0,0,MULIB_SHIRAZ_AND_,MULIB_SHIRAZ_SUB_,MULIB_SHIRAZ_XOR_,MULIB_SHIRAZ_CMP_};
    if (!v || !out) return MULIB_SHIRAZ_EINVAL;
    memset(&d,0,sizeof(d)); memset(&p,0,sizeof(p));
    for (d.n=0;d.n<15;++d.n) { unsigned char *b=mulib_shiraz_memory_(v,address+d.n,1,4); if (!b) break; d.bytes[d.n]=*b; }
    if (!d.n) return mulib_shiraz_fail_(v,MULIB_SHIRAZ_EBOUNDS,address,"instruction fetch outside executable memory");
    op=(unsigned)mulib_shiraz_take_(&d,1);
    if (op==0x66) { w=2; op=(unsigned)mulib_shiraz_take_(&d,1); }
    if (op>=0x40 && op<=0x4f) { d.rex=op; if (op&8) w=8; op=(unsigned)mulib_shiraz_take_(&d,1); }
    p.width=w; p.address=address;
    if (op==0x90) { if (d.rex&1) known=0; p.opcode=MULIB_SHIRAZ_NOP_; }
    else if (op>=0xb8 && op<=0xbf) { p.opcode=MULIB_SHIRAZ_MOV_; p.dst=mulib_shiraz_reg_(op-0xb8+((d.rex&1)?8:0),w); p.src=mulib_shiraz_imm_(mulib_shiraz_take_(&d,w),w); }
    else if (op>=0x50 && op<=0x5f && w!=2) { p.opcode=op<0x58?MULIB_SHIRAZ_PUSH_:MULIB_SHIRAZ_POP_; p.width=8; p.dst=mulib_shiraz_reg_((op&7)+((d.rex&1)?8:0),8); }
    else if (op==0x68 || op==0x6a) { p.opcode=MULIB_SHIRAZ_PUSH_; p.width=8; g=op==0x6a?1:4; p.dst=mulib_shiraz_imm_(mulib_shiraz_sign_(mulib_shiraz_take_(&d,g),g),8); if (w==2) known=0; }
    else if (op==0xc3 || op==0xc2) { p.opcode=MULIB_SHIRAZ_RET_; p.target=op==0xc2?mulib_shiraz_take_(&d,2):0; if (w==2) known=0; }
    else if (op==0xc9) { p.opcode=MULIB_SHIRAZ_LEAVE_; if (w==2) known=0; }
    else if (op==0xe8 || op==0xe9 || op==0xeb || (op>=0x70 && op<=0x7f)) {
        p.opcode=op==0xe8?MULIB_SHIRAZ_CALL_:op>=0x70 && op<=0x7f?MULIB_SHIRAZ_JCC_:MULIB_SHIRAZ_JMP_;
        p.condition=op&15; g=(op==0xeb || op<0x80)?1:4; p.target=mulib_shiraz_sign_(mulib_shiraz_take_(&d,g),g); relative=1;
    } else if (op==0x0f) {
        op=(unsigned)mulib_shiraz_take_(&d,1);
        if (op>=0x80 && op<=0x8f) { p.opcode=MULIB_SHIRAZ_JCC_; p.condition=op&15; p.target=mulib_shiraz_sign_(mulib_shiraz_take_(&d,4),4); relative=1; }
        else if (op==0xb6 || op==0xb7 || op==0xbe || op==0xbf || op==0xaf || op==0x1f) {
            m=(unsigned)mulib_shiraz_take_(&d,1); reg=((m>>3)&7)+((d.rex&4)?8:0);
            p.opcode=op==0xaf?MULIB_SHIRAZ_IMUL_:op==0x1f?MULIB_SHIRAZ_NOP_:op>=0xbe?MULIB_SHIRAZ_MOVSX_:MULIB_SHIRAZ_MOVZX_;
            p.dst=mulib_shiraz_reg_(reg,w); p.src=mulib_shiraz_rm_(&d,m,op==0xaf||op==0x1f?w:(op&1)?2:1);
            if (op==0x1f && ((m>>3)&7)) known=0;
        } else known=0;
    } else if (op<=0x3b && (op&7)<=3 && alu[op>>3]) {
        p.opcode=alu[op>>3]; if (!(op&1)) p.width=w=1;
        m=(unsigned)mulib_shiraz_take_(&d,1); reg=((m>>3)&7)+((d.rex&4)?8:0);
        if (w==1 && !d.rex && reg>=4) known=0;
        p.dst=mulib_shiraz_rm_(&d,m,w); p.src=mulib_shiraz_reg_(reg,w);
        if (op&2) { mulib_shiraz_operand_t tmp=p.dst; p.dst=p.src; p.src=tmp; }
    } else if ((op<=0x3d && (op&7)==5 && alu[op>>3]) || op==0xa9) {
        p.opcode=op==0xa9?MULIB_SHIRAZ_TEST_:alu[op>>3]; p.dst=mulib_shiraz_reg_(0,w);
        p.src=mulib_shiraz_imm_(mulib_shiraz_take_(&d,w==8?4:w),w); if (w==8) p.src.value=mulib_shiraz_sign_(p.src.value,4);
    } else if (op==0x89 || op==0x8b || op==0x88 || op==0x8a || op==0x8d || op==0x85 || op==0x84 || op==0x63) {
        if (op==0x88 || op==0x8a || op==0x84) p.width=w=1;
        m=(unsigned)mulib_shiraz_take_(&d,1); reg=((m>>3)&7)+((d.rex&4)?8:0);
        if (w==1 && !d.rex && reg>=4) known=0;
        p.opcode=op==0x8d?MULIB_SHIRAZ_LEA_:op==0x85 || op==0x84?MULIB_SHIRAZ_TEST_:op==0x63?MULIB_SHIRAZ_MOVSX_:MULIB_SHIRAZ_MOV_;
        p.dst=mulib_shiraz_rm_(&d,m,op==0x63?4:w); p.src=mulib_shiraz_reg_(reg,w);
        if (op==0x8b || op==0x8a || op==0x8d || op==0x63) { mulib_shiraz_operand_t tmp=p.dst; p.dst=p.src; p.src=tmp; }
        if (op==0x8d && p.src.kind!=2) known=0;
        if (op==0x63 && w!=8) known=0;
    } else if (op==0x81 || op==0x83 || op==0x80 || op==0xc7 || op==0xc6 || op==0xff) {
        if (op==0x80 || op==0xc6) p.width=w=1;
        m=(unsigned)mulib_shiraz_take_(&d,1); g=(m>>3)&7;
        if (op==0xff) {
            p.opcode=g==0?MULIB_SHIRAZ_INC_:g==1?MULIB_SHIRAZ_DEC_:g==2?MULIB_SHIRAZ_CALL_:g==4?MULIB_SHIRAZ_JMP_:MULIB_SHIRAZ_PUSH_;
            if (g!=0 && g!=1 && g!=2 && g!=4 && g!=6) known=0;
            if (g>=2) p.width=w=8;
            p.dst=mulib_shiraz_rm_(&d,m,w);
        } else {
            unsigned n=op==0x83 || op==0x80 || op==0xc6?1:w==8?4:w;
            p.opcode=op==0xc7 || op==0xc6?MULIB_SHIRAZ_MOV_:alu[g];
            if (!p.opcode || ((op==0xc7 || op==0xc6) && g)) known=0;
            p.dst=mulib_shiraz_rm_(&d,m,w); p.src=mulib_shiraz_imm_(mulib_shiraz_take_(&d,n),w);
            if (n<w) p.src.value=mulib_shiraz_sign_(p.src.value,n);
        }
    } else known=0;
    if (d.bad || !known) return mulib_shiraz_fail_(v,MULIB_SHIRAZ_EUNSUPPORTED,address,"unsupported or truncated AMD64 instruction");
    p.length=(unsigned)d.at; p.next=address+d.at; if (relative) p.target+=p.next;
    *out=p; return 0;
}
static uint64_t mulib_shiraz_address_(mulib_shiraz_t *v, const mulib_shiraz_operand_t *o, uint64_t next) {
    uint64_t a=o->base<16?v->cpu.reg[o->base]:o->base==17?next:0;
    if (o->index<16) a+=v->cpu.reg[o->index]*o->scale;
    return a+(uint64_t)o->displacement;
}
static uint64_t mulib_shiraz_mask_(unsigned w) { return w==8?UINT64_MAX:(UINT64_C(1)<<(w*8))-1; }
static int mulib_shiraz_value_(mulib_shiraz_t *v, const mulib_shiraz_operand_t *o, uint64_t next, uint64_t *x) {
    if (o->kind==1) *x=v->cpu.reg[o->reg]&mulib_shiraz_mask_(o->width);
    else if (o->kind==3) *x=o->value&mulib_shiraz_mask_(o->width);
    else if (o->kind==2) return mulib_shiraz_read_int_(v,mulib_shiraz_address_(v,o,next),o->width,x);
    else return MULIB_SHIRAZ_EINVAL;
    return 0;
}
static int mulib_shiraz_store_(mulib_shiraz_t *v, const mulib_shiraz_operand_t *o, uint64_t next, uint64_t x) {
    uint64_t mask=mulib_shiraz_mask_(o->width); x&=mask;
    if (o->kind==1) { if (o->width<4) x|=v->cpu.reg[o->reg]&~mask; v->cpu.reg[o->reg]=x; return 0; }
    if (o->kind==2) return mulib_shiraz_write_int_(v,mulib_shiraz_address_(v,o,next),o->width,x);
    return MULIB_SHIRAZ_EINVAL;
}
static int mulib_shiraz_push_(mulib_shiraz_t *v, uint64_t x) {
    int rc=mulib_shiraz_write_int_(v,v->cpu.reg[4]-8,8,x); if (!rc) v->cpu.reg[4]-=8; return rc;
}
static int mulib_shiraz_pop_(mulib_shiraz_t *v, uint64_t *x) {
    int rc=mulib_shiraz_read_int_(v,v->cpu.reg[4],8,x); if (!rc) v->cpu.reg[4]+=8; return rc;
}
static int mulib_shiraz_condition_(uint64_t f, unsigned c) {
    int cf=(f&1)!=0,pf=(f&4)!=0,zf=(f&64)!=0,sf=(f&128)!=0,of=(f&2048)!=0;
    int b=0; switch(c>>1) { case 0:b=of;break;case 1:b=cf;break;case 2:b=zf;break;case 3:b=cf||zf;break;
        case 4:b=sf;break;case 5:b=pf;break;case 6:b=sf!=of;break;case 7:b=zf||(sf!=of);break; }
    return (c&1)?!b:b;
}
static void mulib_shiraz_flags_(mulib_shiraz_t *v, unsigned op, unsigned w, uint64_t a, uint64_t b, uint64_t r) {
    uint64_t mask=mulib_shiraz_mask_(w), sign=UINT64_C(1)<<(w*8-1),f=2; unsigned parity;
    r&=mask;
    parity=(unsigned)(r&255); parity^=parity>>4; parity^=parity>>2; parity^=parity>>1;
    if (!(parity&1)) f|=4;
    if (!r) f|=64;
    if (r&sign) f|=128;
    if (op==MULIB_SHIRAZ_ADD_ || op==MULIB_SHIRAZ_INC_) { if (r<a) f|=1; if ((~(a^b)&(a^r)&sign)) f|=2048; }
    if (op==MULIB_SHIRAZ_SUB_ || op==MULIB_SHIRAZ_CMP_ || op==MULIB_SHIRAZ_DEC_) { if (a<b) f|=1; if (((a^b)&(a^r)&sign)) f|=2048; }
    if (op==MULIB_SHIRAZ_INC_ || op==MULIB_SHIRAZ_DEC_) f=(f&~UINT64_C(1))|(v->cpu.flags&1);
    v->cpu.flags=f;
}
int mulib_shiraz_execute(mulib_shiraz_t *v, const mulib_shiraz_instruction_t *p) {
    uint64_t a=0,b=0,r=0,next; int rc=0,store=0;
    if (!v || !p || p->address!=v->cpu.pc || p->width<1 || p->width>8) return MULIB_SHIRAZ_EINVAL;
    if (v->cpu.fault) return v->cpu.fault;
    if (v->cpu.halted) return MULIB_SHIRAZ_DONE;
    next=p->next; ++v->cpu.steps;
    switch(p->opcode) {
    case MULIB_SHIRAZ_NOP_: break;
    case MULIB_SHIRAZ_MOV_: case MULIB_SHIRAZ_MOVZX_: case MULIB_SHIRAZ_MOVSX_:
        rc=mulib_shiraz_value_(v,&p->src,p->next,&r);
        if (p->opcode==MULIB_SHIRAZ_MOVSX_) r=mulib_shiraz_sign_(r,p->src.width);
        store=1; break;
    case MULIB_SHIRAZ_LEA_: r=mulib_shiraz_address_(v,&p->src,p->next); store=1; break;
    case MULIB_SHIRAZ_PUSH_: rc=mulib_shiraz_value_(v,&p->dst,p->next,&a); if (!rc) rc=mulib_shiraz_push_(v,a); break;
    case MULIB_SHIRAZ_POP_: rc=mulib_shiraz_pop_(v,&r); store=1; break;
    case MULIB_SHIRAZ_CALL_: case MULIB_SHIRAZ_JMP_:
        if (p->dst.kind) rc=mulib_shiraz_value_(v,&p->dst,p->next,&next); else next=p->target;
        if (!rc && p->opcode==MULIB_SHIRAZ_CALL_) rc=mulib_shiraz_push_(v,p->next);
        break;
    case MULIB_SHIRAZ_JCC_: if (mulib_shiraz_condition_(v->cpu.flags,p->condition)) next=p->target; break;
    case MULIB_SHIRAZ_RET_: rc=mulib_shiraz_pop_(v,&next); if (!rc) v->cpu.reg[4]+=p->target; break;
    case MULIB_SHIRAZ_LEAVE_: v->cpu.reg[4]=v->cpu.reg[5]; rc=mulib_shiraz_pop_(v,&v->cpu.reg[5]); break;
    default:
        rc=mulib_shiraz_value_(v,&p->dst,p->next,&a); if (rc) break;
        if (p->opcode==MULIB_SHIRAZ_INC_ || p->opcode==MULIB_SHIRAZ_DEC_) b=1;
        else { rc=mulib_shiraz_value_(v,&p->src,p->next,&b); if (rc) break; }
        switch(p->opcode) {
        case MULIB_SHIRAZ_ADD_: case MULIB_SHIRAZ_INC_:r=a+b;break;
        case MULIB_SHIRAZ_SUB_:case MULIB_SHIRAZ_CMP_:case MULIB_SHIRAZ_DEC_:r=a-b;break;
        case MULIB_SHIRAZ_OR_:r=a|b;break;case MULIB_SHIRAZ_AND_:case MULIB_SHIRAZ_TEST_:r=a&b;break;
        case MULIB_SHIRAZ_XOR_:r=a^b;break;
        case MULIB_SHIRAZ_IMUL_: {
            uint64_t sa=mulib_shiraz_sign_(a,p->width),sb=mulib_shiraz_sign_(b,p->width);
            uint64_t aa=(sa>>63)?0-sa:sa,bb=(sb>>63)?0-sb:sb;
            uint64_t limit=(UINT64_C(1)<<(p->width*8-1))-(((sa^sb)>>63)?0:1);
            r=a*b; v->cpu.flags=(v->cpu.flags&~UINT64_C(2049))|((bb && aa>limit/bb)?2049:0); break;
        }
        default:rc=MULIB_SHIRAZ_EUNSUPPORTED;break;
        }
        if (rc) break;
        if (p->opcode!=MULIB_SHIRAZ_IMUL_) mulib_shiraz_flags_(v,p->opcode,p->width,a,b,r);
        store=p->opcode!=MULIB_SHIRAZ_CMP_ && p->opcode!=MULIB_SHIRAZ_TEST_; break;
    }
    if (!rc && store) rc=mulib_shiraz_store_(v,&p->dst,p->next,r);
    if (rc) { v->cpu.fault=rc; return rc; }
    v->cpu.pc=next;
    if (!next) mulib_shiraz_exit(v,(uint32_t)v->cpu.reg[0]);
    return v->cpu.halted?MULIB_SHIRAZ_DONE:0;
}
int mulib_shiraz_dispatch(mulib_shiraz_t *v) {
    uint64_t index,next; int rc;
    if (!v) return MULIB_SHIRAZ_EINVAL;
    if (v->cpu.fault) return v->cpu.fault;
    if (v->cpu.halted) return MULIB_SHIRAZ_DONE;
    if (!v->cpu.pc) { mulib_shiraz_exit(v,(uint32_t)v->cpu.reg[0]); return MULIB_SHIRAZ_DONE; }
    index=v->cpu.pc-MULIB_SHIRAZ_IMPORT_;
    if (v->cpu.pc<MULIB_SHIRAZ_IMPORT_ || index%16 || index/16>=v->import_count)
        return v->cpu.fault=mulib_shiraz_fail_(v,MULIB_SHIRAZ_EUNSUPPORTED,v->cpu.pc,"branch target absent from AOT translation");
    index/=16;
    if (!v->host) return v->cpu.fault=mulib_shiraz_fail_(v,MULIB_SHIRAZ_EIMPORT,v->cpu.pc,v->imports[index].name);
    ++v->cpu.steps;
    rc=v->host(v->user,v,v->imports[index].dll,v->imports[index].name,v->imports[index].ordinal);
    if (rc) return v->cpu.fault=mulib_shiraz_fail_(v,rc<0?rc:MULIB_SHIRAZ_EIMPORT,v->cpu.pc,v->imports[index].name);
    if (v->cpu.halted) return MULIB_SHIRAZ_DONE;
    rc=mulib_shiraz_pop_(v,&next); if (rc) return v->cpu.fault=rc;
    v->cpu.pc=next; if (!next) { mulib_shiraz_exit(v,(uint32_t)v->cpu.reg[0]); return MULIB_SHIRAZ_DONE; }
    return 0;
}
int mulib_shiraz_run(mulib_shiraz_t *v, size_t budget) {
    size_t i; int rc;
    if (!v) return MULIB_SHIRAZ_EINVAL;
    if (v->cpu.fault) return v->cpu.fault;
    if (v->cpu.halted) return MULIB_SHIRAZ_DONE;
    for(i=0;i<budget;++i) {
        mulib_shiraz_instruction_t p;
        if (!v->cpu.pc || v->cpu.pc>=MULIB_SHIRAZ_IMPORT_) rc=mulib_shiraz_dispatch(v);
        else { rc=mulib_shiraz_decode(v,v->cpu.pc,&p); if (!rc) rc=mulib_shiraz_execute(v,&p); }
        if (rc) { if (rc<0) v->cpu.fault=rc; return rc; }
    }
    return MULIB_SHIRAZ_AGAIN;
}
static int mulib_shiraz_emit_operand_(mulib_strbuf_t *s, const mulib_shiraz_operand_t *o) {
    char text[256]; (void)snprintf(text,sizeof(text),"{%u,%u,%u,%u,%u,%u,UINT64_C(0x%" PRIx64 "),(-INT64_C(%" PRIu64 ")%s)}",
        o->kind,o->reg,o->base,o->index,o->scale,o->width,o->value,
        o->displacement<0?(uint64_t)(-(o->displacement+1)):(uint64_t)o->displacement,
        o->displacement<0?"-1":"*-1");
    return mulib_strbuf_append(s,text);
}
static int mulib_shiraz_noreturn_(mulib_shiraz_t *v, const mulib_shiraz_instruction_t *p) {
    uint64_t target=p->target; unsigned char *bytes;
    if (p->opcode!=MULIB_SHIRAZ_CALL_) return 0;
    if (p->dst.kind) {
        if (p->dst.kind!=2 || p->dst.base<16 || p->dst.index<16) return 0;
        bytes=mulib_shiraz_memory_(v,mulib_shiraz_address_(v,&p->dst,p->next),8,1);
        if (!bytes) return 0;
        target=mulib_shiraz_u_(bytes,8);
    } else if ((bytes=mulib_shiraz_memory_(v,target,14,4))!=NULL && bytes[0]==0xff && bytes[1]==0x25 && !mulib_shiraz_u_(bytes+2,4)) {
        target=mulib_shiraz_u_(bytes+6,8); /* COFF import jump thunk */
    }
    if (target<MULIB_SHIRAZ_IMPORT_ || (target-MULIB_SHIRAZ_IMPORT_)%16) return 0;
    target=(target-MULIB_SHIRAZ_IMPORT_)/16;
    return target<v->import_count && !v->imports[target].ordinal && !strcmp(v->imports[target].name,"ExitProcess");
}
int mulib_shiraz_emit_c(mulib_shiraz_t *v, size_t limit, mulib_strbuf_t *out) {
    mulib_strbuf_t s={0}; mulib_shiraz_instruction_t *nodes=NULL;
    uint64_t *queue=NULL; unsigned char *seen=NULL; size_t count=0,head=0,tail=0,i,j; int rc=MULIB_SHIRAZ_ENOMEM; char text[512];
    if (!v || !out || v->cpu.steps || v->cpu.pc!=v->entry || v->changed || v->cpu.flags!=2 ||
        v->cpu.halted || v->cpu.fault || v->cpu.exit_code) return MULIB_SHIRAZ_EINVAL;
    for(i=0;i<16;++i) if (v->cpu.reg[i]!=(i==4?MULIB_SHIRAZ_STACK_+(v->stack_size&~(size_t)15)-40:0)) return MULIB_SHIRAZ_EINVAL;
    if (!limit) limit=100000;
    if (limit>SIZE_MAX/sizeof(*nodes) || limit>SIZE_MAX/sizeof(*queue)) return MULIB_SHIRAZ_ELIMIT;
    nodes=(mulib_shiraz_instruction_t *)calloc(limit,sizeof(*nodes)); queue=(uint64_t *)calloc(limit,sizeof(*queue));
    seen=(unsigned char *)calloc(1,v->image_size); if (!nodes || !queue || !seen) goto done;
    queue[tail++]=v->entry; seen[v->entry-v->base]=1;
    while(head<tail) {
        mulib_shiraz_instruction_t *p=&nodes[count]; uint64_t successors[2]; size_t n=0;
        rc=mulib_shiraz_decode(v,queue[head++],p); if (rc) goto done;
        ++count;
        if (p->opcode!=MULIB_SHIRAZ_RET_ && p->opcode!=MULIB_SHIRAZ_JMP_ && !mulib_shiraz_noreturn_(v,p)) successors[n++]=p->next;
        if ((p->opcode==MULIB_SHIRAZ_CALL_ || p->opcode==MULIB_SHIRAZ_JMP_ || p->opcode==MULIB_SHIRAZ_JCC_) && !p->dst.kind) successors[n++]=p->target;
        for(j=0;j<n;++j) {
            uint64_t a=successors[j];
            if (a>=MULIB_SHIRAZ_IMPORT_) continue;
            if (!mulib_shiraz_memory_(v,a,1,4)) { rc=MULIB_SHIRAZ_EBOUNDS; goto done; }
            if (seen[a-v->base]) continue;
            if (tail==limit) { rc=MULIB_SHIRAZ_ELIMIT; goto done; }
            seen[a-v->base]=1; queue[tail++]=a;
        }
    }
    rc=MULIB_SHIRAZ_ENOMEM;
    if (!mulib_strbuf_append(&s,"/* Generated by Shiraz: helper-based AOT, no instruction decoder. */\n#include <mulib-shiraz.h>\nstatic const unsigned char image[]={\n")) goto done;
    for(i=0;i<v->file_size;++i) { (void)snprintf(text,sizeof(text),"%u,%s",v->file[i],i%24==23?"\n":""); if (!mulib_strbuf_append(&s,text)) goto done; }
    if (!mulib_strbuf_append(&s,"\n};\nstatic const mulib_shiraz_instruction_t code[]={\n")) goto done;
    for(i=0;i<count;++i) {
        mulib_shiraz_instruction_t *p=&nodes[i];
        (void)snprintf(text,sizeof(text),"{%u,%u,%u,%u,UINT64_C(0x%" PRIx64 "),UINT64_C(0x%" PRIx64 "),UINT64_C(0x%" PRIx64 "),",p->opcode,p->width,p->condition,p->length,p->address,p->next,p->target);
        if (!mulib_strbuf_append(&s,text) || !mulib_shiraz_emit_operand_(&s,&p->dst) || !mulib_strbuf_putc(&s,',') || !mulib_shiraz_emit_operand_(&s,&p->src) || !mulib_strbuf_append(&s,"},\n")) goto done;
    }
    if (!mulib_strbuf_append(&s,"};\nint main(void) {\n mulib_shiraz_t *v=NULL; mulib_shiraz_runtime_t *r; mulib_shiraz_options_t o={0}; int rc;\n")) goto done;
    (void)snprintf(text,sizeof(text)," o.max_image=%zu; o.stack_size=%zu; o.heap_size=%zu; o.image_base=UINT64_C(0x%" PRIx64 ");\n",v->file_size>v->image_size?v->file_size:v->image_size,v->stack_size,v->heap_size,v->base);
    if (!mulib_strbuf_append(&s,text) || !mulib_strbuf_append(&s," o.entry_symbol=\"")) goto done;
    for(i=0;v->entry_symbol[i];++i) { (void)snprintf(text,sizeof(text),"\\%03o",(unsigned char)v->entry_symbol[i]); if (!mulib_strbuf_append(&s,text)) goto done; }
    if (!mulib_strbuf_append(&s,"\";\n rc=mulib_shiraz_load(&v,image,sizeof(image),&o,NULL); if(rc) return 125;\n r=mulib_shiraz_runtime_create(getenv(\"SHIRAZ_REGISTRY\"));\n if(!r || mulib_shiraz_runtime_attach(r,v)) { mulib_shiraz_runtime_destroy(r); mulib_shiraz_destroy(v); return 125; }\n while(!mulib_shiraz_cpu(v)->halted) {\n switch(mulib_shiraz_cpu(v)->pc) {\n")) goto done;
    for(i=0;i<count;++i) { (void)snprintf(text,sizeof(text)," case UINT64_C(0x%" PRIx64 "): rc=mulib_shiraz_execute(v,&code[%zu]); break;\n",nodes[i].address,i); if (!mulib_strbuf_append(&s,text)) goto done; }
    if (!mulib_strbuf_append(&s," default: rc=mulib_shiraz_dispatch(v); break;\n }\n if(rc<0) { fprintf(stderr,\"Shiraz: %s at 0x%llx\\n\",mulib_shiraz_error(v)->message,(unsigned long long)mulib_shiraz_cpu(v)->pc); break; }\n }\n if(rc>=0) rc=(int)(mulib_shiraz_cpu(v)->exit_code&255); else rc=125;\n if(mulib_shiraz_runtime_save(r)) rc=125;\n mulib_shiraz_runtime_destroy(r); mulib_shiraz_destroy(v); return rc;\n}\n")) goto done;
    mulib_strbuf_free(out); *out=s; memset(&s,0,sizeof(s)); rc=0;
done: free(nodes); free(queue); free(seen); mulib_strbuf_free(&s); return rc;
}
#endif
