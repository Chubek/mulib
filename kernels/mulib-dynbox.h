/**
 * @file mulib-dynbox.h
 * @brief Native runtime-typed calls, variadics, structures and callback closures.
 * Original dependency-free Linux AMD64 SysV backend; not a libffi wrapper.
 * Define MULIB_IMPL_DYNBOX in one C11 TU. Other platforms return ENOTSUP.
 * Descriptors must exactly match trusted native prototypes. Not a sandbox.
 */
#ifndef MULIB_DYNBOX_H
#define MULIB_DYNBOX_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_DYNBOX_MAX_ARGS 1024u
#define MULIB_DYNBOX_MAX_FIELDS 1024u
#define MULIB_DYNBOX_MAX_BYTES 65536u
typedef enum mulib_dynbox_kind {
  MULIB_DYNBOX_VOID, MULIB_DYNBOX_SINT8, MULIB_DYNBOX_UINT8,
  MULIB_DYNBOX_SINT16, MULIB_DYNBOX_UINT16, MULIB_DYNBOX_SINT32, MULIB_DYNBOX_UINT32,
  MULIB_DYNBOX_SINT64, MULIB_DYNBOX_UINT64, MULIB_DYNBOX_SLONG, MULIB_DYNBOX_ULONG,
  MULIB_DYNBOX_FLOAT, MULIB_DYNBOX_DOUBLE, MULIB_DYNBOX_LONGDOUBLE,
  MULIB_DYNBOX_POINTER, MULIB_DYNBOX_STRUCT
} mulib_dynbox_kind_t;
/** @brief Immutable natural-C type descriptor. Use built-ins or struct_init;
 * no packed fields, bitfields, unions, vectors or complex types. */
typedef struct mulib_dynbox_type {
  mulib_dynbox_kind_t kind;
  size_t size, alignment;
  const struct mulib_dynbox_type *const *fields;
  size_t field_count;
} mulib_dynbox_type_t;
extern const mulib_dynbox_type_t mulib_dynbox_type_void;
extern const mulib_dynbox_type_t mulib_dynbox_type_sint8, mulib_dynbox_type_uint8;
extern const mulib_dynbox_type_t mulib_dynbox_type_sint16, mulib_dynbox_type_uint16;
extern const mulib_dynbox_type_t mulib_dynbox_type_sint32, mulib_dynbox_type_uint32;
extern const mulib_dynbox_type_t mulib_dynbox_type_sint64, mulib_dynbox_type_uint64;
extern const mulib_dynbox_type_t mulib_dynbox_type_slong, mulib_dynbox_type_ulong;
extern const mulib_dynbox_type_t mulib_dynbox_type_float, mulib_dynbox_type_double;
extern const mulib_dynbox_type_t mulib_dynbox_type_longdouble, mulib_dynbox_type_pointer;
typedef struct mulib_dynbox_cif mulib_dynbox_cif_t;
typedef struct mulib_dynbox_closure mulib_dynbox_closure_t;
/** @brief Erased native function address; cast back only via call/typed ABI use. */
typedef void (*mulib_dynbox_fn)(void);
enum mulib_dynbox_result {
  MULIB_DYNBOX_OK = 0, MULIB_DYNBOX_EINVAL = -1, MULIB_DYNBOX_ENOMEM = -2,
  MULIB_DYNBOX_ENOTSUP = -3, MULIB_DYNBOX_ERANGE = -4, MULIB_DYNBOX_EOS = -5
};
/** @brief Build natural struct layout with checked padding. Fields are borrowed
 * until prepare; optional offsets has count entries. Transactional. Maximum
 * nesting 32 and 65536 descriptor visits per operation. */
int mulib_dynbox_struct_init(mulib_dynbox_type_t *output,
                             const mulib_dynbox_type_t *const *fields, size_t count,
                             size_t *offsets);
/** @brief Whether a native ABI backend is compiled in, not OS executable-memory permission. */
int mulib_dynbox_supported(void);
/** @brief Prepare an owned fixed-arity call interface. *output must be NULL.
 * Layouts are copied; type descriptors need not outlive preparation. */
int mulib_dynbox_prepare(mulib_dynbox_cif_t **output, const mulib_dynbox_type_t *result,
                         size_t count, const mulib_dynbox_type_t *const *arguments);
/** @brief Prepare variadic interface with 1..count fixed arguments. Tail types
 * must already reflect C default promotions (double/int, not float/small ints). */
int mulib_dynbox_prepare_variadic(mulib_dynbox_cif_t **output, const mulib_dynbox_type_t *result,
                                  size_t fixed, size_t count,
                                  const mulib_dynbox_type_t *const *arguments);
/** @brief Unmap/free interface. NULL succeeds; native unmap failure retains ownership.
 * All calls and associated closures must have finished first. */
int mulib_dynbox_destroy(mulib_dynbox_cif_t *);
/** @brief Number of native arguments, excluding any ABI hidden result pointer. */
size_t mulib_dynbox_argument_count(const mulib_dynbox_cif_t *);
/** @brief Exact native result size (zero for void). */
size_t mulib_dynbox_result_size(const mulib_dynbox_cif_t *);
/** @brief Call with pointers to exact native argument values. Nonvoid result
 * needs at least result_size bytes (no ffi_arg widening). Validation/allocation
 * failures preserve result; execution of a valid target cannot be rolled back.
 * Independent calls through one immutable CIF are reentrant/thread-safe. */
int mulib_dynbox_call(const mulib_dynbox_cif_t *, mulib_dynbox_fn,
                      void *result, size_t result_capacity, const void *const *arguments);
/** @brief Callback receives aligned native values and zero-initialized result.
 * Copy a result of exactly result_size bytes. Pointers expire on return.
 * No exceptions/unwinding across generated code; do not destroy a live closure. */
typedef void (*mulib_dynbox_callback_fn)(const mulib_dynbox_cif_t *, void *result,
                                        void *const *arguments, void *user);
/** @brief Create an RX closure for a nonvariadic CIF. CIF and callback/user
 * are borrowed and must outlive closure. Invocation allocates bounded stack
 * scratch, not heap memory. *output must be NULL. */
int mulib_dynbox_closure_create(mulib_dynbox_closure_t **output, const mulib_dynbox_cif_t *,
                                mulib_dynbox_callback_fn, void *user);
/** @brief Borrow native entry address; use exactly the prepared prototype. */
mulib_dynbox_fn mulib_dynbox_closure_function(const mulib_dynbox_closure_t *);
/** @brief Unmap/free closure. NULL succeeds; failure retains ownership. */
int mulib_dynbox_closure_destroy(mulib_dynbox_closure_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_DYNBOX) && !defined(MULIB_DYNBOX_IMPLEMENTED)
#define MULIB_DYNBOX_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <float.h>
#if defined(__linux__) && defined(__x86_64__) && !defined(__ILP32__)
#define MULIB_DYNBOX_NATIVE_ 1
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
#define MULIB_DYNBOX_ALIGNOF_(t) alignof(t)
extern "C" {
#else
#define MULIB_DYNBOX_ALIGNOF_(t) _Alignof(t)
#endif
#define MULIB_DYNBOX_TYPE_(name, code, type) \
  const mulib_dynbox_type_t mulib_dynbox_type_##name = {code, sizeof(type), MULIB_DYNBOX_ALIGNOF_(type), NULL, 0}
const mulib_dynbox_type_t mulib_dynbox_type_void = {MULIB_DYNBOX_VOID, 0, 1, NULL, 0};
MULIB_DYNBOX_TYPE_(sint8, MULIB_DYNBOX_SINT8, int8_t);
MULIB_DYNBOX_TYPE_(uint8, MULIB_DYNBOX_UINT8, uint8_t);
MULIB_DYNBOX_TYPE_(sint16, MULIB_DYNBOX_SINT16, int16_t);
MULIB_DYNBOX_TYPE_(uint16, MULIB_DYNBOX_UINT16, uint16_t);
MULIB_DYNBOX_TYPE_(sint32, MULIB_DYNBOX_SINT32, int32_t);
MULIB_DYNBOX_TYPE_(uint32, MULIB_DYNBOX_UINT32, uint32_t);
MULIB_DYNBOX_TYPE_(sint64, MULIB_DYNBOX_SINT64, int64_t);
MULIB_DYNBOX_TYPE_(uint64, MULIB_DYNBOX_UINT64, uint64_t);
MULIB_DYNBOX_TYPE_(slong, MULIB_DYNBOX_SLONG, long);
MULIB_DYNBOX_TYPE_(ulong, MULIB_DYNBOX_ULONG, unsigned long);
MULIB_DYNBOX_TYPE_(float, MULIB_DYNBOX_FLOAT, float);
MULIB_DYNBOX_TYPE_(double, MULIB_DYNBOX_DOUBLE, double);
MULIB_DYNBOX_TYPE_(longdouble, MULIB_DYNBOX_LONGDOUBLE, long double);
MULIB_DYNBOX_TYPE_(pointer, MULIB_DYNBOX_POINTER, void *);
#undef MULIB_DYNBOX_TYPE_
/* Classes: 0 padding/none, 1 INTEGER, 2 SSE, 3 MEMORY. SSEUP is not needed
 * without vectors; x87 scalar returns have a separate path. */
typedef struct mulib_dynbox_layout_ {
  size_t size, alignment, stack, scratch;
  unsigned cls[2], reg[2], memory, is_signed, longdouble;
} mulib_dynbox_layout_;
typedef struct mulib_dynbox_memory_ { void *mapping, *entry; size_t mapped; } mulib_dynbox_memory_;
typedef struct mulib_dynbox_frame_ {
  mulib_dynbox_fn function;
  uint64_t gp[6], fp[8];
  unsigned char *stack;
  size_t stack_size;
  uint64_t out_gp[2], out_fp[2];
  unsigned char out_ld[16];
} mulib_dynbox_frame_;
struct mulib_dynbox_cif {
  mulib_dynbox_layout_ result, *args;
  size_t count, fixed, stack_size, scratch_size;
  unsigned fp_count, variadic;
  mulib_dynbox_memory_ code;
};
struct mulib_dynbox_closure {
  const mulib_dynbox_cif_t *cif;
  mulib_dynbox_callback_fn callback;
  void *user;
  mulib_dynbox_memory_ code;
};
int mulib_dynbox_supported(void) {
#ifdef MULIB_DYNBOX_NATIVE_
  return sizeof(void *) == 8 && sizeof(mulib_dynbox_fn) == 8
      && sizeof(float) == 4 && sizeof(double) == 8 && sizeof(long double) == 16
      && LDBL_MANT_DIG == 64;
#else
  return 0;
#endif
}
static int mulib_dynbox_align_(size_t n, size_t a, size_t *out) {
  if (!a || a > 16 || (a & (a - 1)) || n > SIZE_MAX - (a - 1)) return 0;
  *out = (n + a - 1) & ~(a - 1); return 1;
}
static const mulib_dynbox_type_t *mulib_dynbox_builtin_(mulib_dynbox_kind_t kind) {
  switch (kind) {
  case MULIB_DYNBOX_VOID: return &mulib_dynbox_type_void;
  case MULIB_DYNBOX_SINT8: return &mulib_dynbox_type_sint8;
  case MULIB_DYNBOX_UINT8: return &mulib_dynbox_type_uint8;
  case MULIB_DYNBOX_SINT16: return &mulib_dynbox_type_sint16;
  case MULIB_DYNBOX_UINT16: return &mulib_dynbox_type_uint16;
  case MULIB_DYNBOX_SINT32: return &mulib_dynbox_type_sint32;
  case MULIB_DYNBOX_UINT32: return &mulib_dynbox_type_uint32;
  case MULIB_DYNBOX_SINT64: return &mulib_dynbox_type_sint64;
  case MULIB_DYNBOX_UINT64: return &mulib_dynbox_type_uint64;
  case MULIB_DYNBOX_SLONG: return &mulib_dynbox_type_slong;
  case MULIB_DYNBOX_ULONG: return &mulib_dynbox_type_ulong;
  case MULIB_DYNBOX_FLOAT: return &mulib_dynbox_type_float;
  case MULIB_DYNBOX_DOUBLE: return &mulib_dynbox_type_double;
  case MULIB_DYNBOX_LONGDOUBLE: return &mulib_dynbox_type_longdouble;
  case MULIB_DYNBOX_POINTER: return &mulib_dynbox_type_pointer;
  default: return NULL;
  }
}
static int mulib_dynbox_type_check_(const mulib_dynbox_type_t *t, unsigned depth,
                                     size_t *budget, size_t *size, size_t *alignment) {
  const mulib_dynbox_type_t *builtin;
  size_t i, n = 0, a = 1;
  if (!t || depth >= 32 || !*budget) return MULIB_DYNBOX_EINVAL;
  --*budget;
  if (t->kind != MULIB_DYNBOX_STRUCT) {
    builtin = mulib_dynbox_builtin_(t->kind);
    if (!builtin || t->size != builtin->size || t->alignment != builtin->alignment
        || t->fields || t->field_count) return MULIB_DYNBOX_EINVAL;
    *size = t->size; *alignment = t->alignment; return 0;
  }
  if (!t->fields || !t->field_count || t->field_count > MULIB_DYNBOX_MAX_FIELDS) return MULIB_DYNBOX_EINVAL;
  for (i = 0; i < t->field_count; ++i) {
    size_t fs, fa;
    int r = mulib_dynbox_type_check_(t->fields[i], depth + 1, budget, &fs, &fa);
    if (r) return r;
    if (!fs) return MULIB_DYNBOX_EINVAL;
    if (!mulib_dynbox_align_(n, fa, &n) || fs > MULIB_DYNBOX_MAX_BYTES - n) return MULIB_DYNBOX_ERANGE;
    n += fs; if (fa > a) a = fa;
  }
  if (!mulib_dynbox_align_(n, a, &n) || n > MULIB_DYNBOX_MAX_BYTES) return MULIB_DYNBOX_ERANGE;
  if (t->size != n || t->alignment != a) return MULIB_DYNBOX_EINVAL;
  *size = n; *alignment = a; return 0;
}
int mulib_dynbox_struct_init(mulib_dynbox_type_t *out, const mulib_dynbox_type_t *const *fields,
                             size_t count, size_t *offsets) {
  size_t i, n = 0, a = 1, budget = 65536, positions[MULIB_DYNBOX_MAX_FIELDS];
  mulib_dynbox_type_t result;
  if (!out || !fields || !count || count > MULIB_DYNBOX_MAX_FIELDS) return MULIB_DYNBOX_EINVAL;
  for (i = 0; i < count; ++i) {
    size_t fs, fa;
    int r = mulib_dynbox_type_check_(fields[i], 1, &budget, &fs, &fa);
    if (r) return r;
    if (!fs) return MULIB_DYNBOX_EINVAL;
    if (!mulib_dynbox_align_(n, fa, &n) || n > MULIB_DYNBOX_MAX_BYTES || fs > MULIB_DYNBOX_MAX_BYTES - n) return MULIB_DYNBOX_ERANGE;
    positions[i] = n; n += fs; if (fa > a) a = fa;
  }
  if (!mulib_dynbox_align_(n, a, &n) || n > MULIB_DYNBOX_MAX_BYTES) return MULIB_DYNBOX_ERANGE;
  result.kind = MULIB_DYNBOX_STRUCT; result.size = n; result.alignment = a;
  result.fields = fields; result.field_count = count;
  if (offsets) memcpy(offsets, positions, count * sizeof *offsets);
  *out = result; return 0;
}
static void mulib_dynbox_classify_(const mulib_dynbox_type_t *t, size_t at, unsigned cls[2], unsigned *memory) {
  size_t i, pos = 0;
  unsigned kind;
  if (t->kind == MULIB_DYNBOX_LONGDOUBLE) { *memory = 1; return; }
  if (t->kind == MULIB_DYNBOX_STRUCT) {
    for (i = 0; i < t->field_count; ++i) {
      (void)mulib_dynbox_align_(pos, t->fields[i]->alignment, &pos);
      mulib_dynbox_classify_(t->fields[i], at + pos, cls, memory); pos += t->fields[i]->size;
    }
    return;
  }
  kind = t->kind == MULIB_DYNBOX_FLOAT || t->kind == MULIB_DYNBOX_DOUBLE ? 2u : 1u;
  for (i = at / 8; i <= (at + t->size - 1) / 8; ++i)
    if (!cls[i] || kind == 1) cls[i] = kind;
}
static int mulib_dynbox_make_layout_(const mulib_dynbox_type_t *t, mulib_dynbox_layout_ *l, size_t *budget) {
  int r;
  memset(l, 0, sizeof *l);
  r = mulib_dynbox_type_check_(t, 0, budget, &l->size, &l->alignment);
  if (r || !l->size) return r;
  l->longdouble = t->kind == MULIB_DYNBOX_LONGDOUBLE;
  l->is_signed = t->kind == MULIB_DYNBOX_SINT8 || t->kind == MULIB_DYNBOX_SINT16
              || t->kind == MULIB_DYNBOX_SINT32 || t->kind == MULIB_DYNBOX_SINT64 || t->kind == MULIB_DYNBOX_SLONG;
  if (l->size > 16) l->memory = 1;
  else mulib_dynbox_classify_(t, 0, l->cls, &l->memory);
  /* A natural aggregate consisting solely of a (possibly nested) long double
   * has the same X87/X87UP return classes as the scalar. Arguments are MEMORY. */
  if (l->size == 16 && l->memory) l->longdouble = 1;
  return 0;
}
typedef struct mulib_dynbox_code_ { unsigned char bytes[1024]; size_t size; int failed; } mulib_dynbox_code_;
static void mulib_dynbox_byte_(mulib_dynbox_code_ *c, unsigned b) {
  if (c->size == sizeof c->bytes) c->failed = 1;
  else c->bytes[c->size++] = (unsigned char)b;
}
static void mulib_dynbox_u32_(mulib_dynbox_code_ *c, uint32_t value) {
  unsigned i; for (i = 0; i < 4; ++i) mulib_dynbox_byte_(c, value >> (i * 8));
}
static void mulib_dynbox_u64_(mulib_dynbox_code_ *c, uint64_t value) {
  unsigned i; for (i = 0; i < 8; ++i) mulib_dynbox_byte_(c, (unsigned)(value >> (i * 8)));
}
static void mulib_dynbox_bytes_(mulib_dynbox_code_ *c, const unsigned char *s, size_t n) {
  size_t i; for (i = 0; i < n; ++i) mulib_dynbox_byte_(c, s[i]);
}
/* Probe each page while growing a native frame, rather than skipping a thread's
 * stack guard with one large subtraction. Linux AMD64 pages are at least 4 KiB. */
static void mulib_dynbox_stack_(mulib_dynbox_code_ *c, size_t size) {
  while (size) {
    size_t step = size > 4096 ? 4096 : size;
    mulib_dynbox_byte_(c,0x48); mulib_dynbox_byte_(c,0x81); mulib_dynbox_byte_(c,0xec);
    mulib_dynbox_u32_(c,(uint32_t)step);
    mulib_dynbox_byte_(c,0xf6); mulib_dynbox_byte_(c,0x04); mulib_dynbox_byte_(c,0x24); mulib_dynbox_byte_(c,0x00);
    size -= step;
  }
}
/* disp32 addressing avoids signed-byte special cases. All bases are rsp/rbp/r12. */
static void mulib_dynbox_mem_(mulib_dynbox_code_ *c, unsigned opcode, unsigned reg, unsigned base, size_t offset) {
  mulib_dynbox_byte_(c, 0x48u | (reg >= 8 ? 4u : 0u) | (base >= 8 ? 1u : 0u));
  mulib_dynbox_byte_(c, opcode); mulib_dynbox_byte_(c, 0x80u | ((reg & 7u) << 3) | (base & 7u));
  if ((base & 7u) == 4) mulib_dynbox_byte_(c, 0x24);
  mulib_dynbox_u32_(c, (uint32_t)offset);
}
static void mulib_dynbox_xmm_(mulib_dynbox_code_ *c, int store, unsigned reg, unsigned base, size_t offset) {
  mulib_dynbox_byte_(c, 0xf2);
  if (base >= 8) mulib_dynbox_byte_(c, 0x41);
  mulib_dynbox_byte_(c, 0x0f); mulib_dynbox_byte_(c, store ? 0x11 : 0x10);
  mulib_dynbox_byte_(c, 0x80u | (reg << 3) | (base & 7u));
  if ((base & 7u) == 4) mulib_dynbox_byte_(c, 0x24);
  mulib_dynbox_u32_(c, (uint32_t)offset);
}
static int mulib_dynbox_map_(mulib_dynbox_memory_ *out, const mulib_dynbox_code_ *code) {
  if (code->failed) return MULIB_DYNBOX_ERANGE;
#ifdef MULIB_DYNBOX_NATIVE_
  {
    long page = sysconf(_SC_PAGESIZE);
    size_t size;
    void *p;
    int fd;
    if (page <= 0) return MULIB_DYNBOX_EOS;
    size = code->size + 16;
    size += ((size_t)page - size % (size_t)page) % (size_t)page;
    fd = open("/dev/zero", O_RDWR);
    if (fd < 0) return MULIB_DYNBOX_EOS;
    p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0); (void)close(fd);
    if (p == MAP_FAILED) return MULIB_DYNBOX_EOS;
    /* Readable zero prefix also accommodates indirect-function sanitizer probes. */
    memcpy((unsigned char *)p + 16, code->bytes, code->size);
    if (mprotect(p, size, PROT_READ | PROT_EXEC)) { (void)munmap(p, size); return MULIB_DYNBOX_EOS; }
    out->mapping = p; out->entry = (unsigned char *)p + 16; out->mapped = size; return 0;
  }
#else
  (void)out; return MULIB_DYNBOX_ENOTSUP;
#endif
}
static int mulib_dynbox_unmap_(mulib_dynbox_memory_ *m) {
  if (!m->mapping) return 0;
#ifdef MULIB_DYNBOX_NATIVE_
  if (munmap(m->mapping, m->mapped)) return MULIB_DYNBOX_EOS;
  memset(m, 0, sizeof *m); return 0;
#else
  return MULIB_DYNBOX_ENOTSUP;
#endif
}
static const unsigned mulib_dynbox_gp_regs_[6] = {7,6,2,1,8,9};
static int mulib_dynbox_bridge_(mulib_dynbox_cif_t *cif) {
  static const unsigned char start[] = {0xf3,0x0f,0x1e,0xfa,0x55,0x48,0x89,0xe5,0x41,0x54,0x41,0x55,0x49,0x89,0xfc};
  static const unsigned char copy[] = {0x48,0x89,0xe7,0xf3,0xa4};
  static const unsigned char end[] = {0x48,0x8d,0x65,0xf0,0x41,0x5d,0x41,0x5c,0x5d,0xc3};
  mulib_dynbox_code_ c;
  unsigned i;
  memset(&c, 0, sizeof c); mulib_dynbox_bytes_(&c, start, sizeof start);
  mulib_dynbox_mem_(&c, 0x8b, 11, 12, offsetof(mulib_dynbox_frame_, function));
  mulib_dynbox_mem_(&c, 0x8b, 1, 12, offsetof(mulib_dynbox_frame_, stack_size));
  mulib_dynbox_mem_(&c, 0x8b, 6, 12, offsetof(mulib_dynbox_frame_, stack));
  mulib_dynbox_stack_(&c, cif->stack_size);
  mulib_dynbox_bytes_(&c, copy, sizeof copy);
  for (i = 0; i < 8; ++i) mulib_dynbox_xmm_(&c, 0, i, 12, offsetof(mulib_dynbox_frame_, fp) + i * 8);
  for (i = 0; i < 6; ++i) mulib_dynbox_mem_(&c, 0x8b, mulib_dynbox_gp_regs_[i], 12, offsetof(mulib_dynbox_frame_, gp) + i * 8);
  mulib_dynbox_byte_(&c, 0xb8); mulib_dynbox_u32_(&c, cif->fp_count);
  mulib_dynbox_byte_(&c, 0x41); mulib_dynbox_byte_(&c, 0xff); mulib_dynbox_byte_(&c, 0xd3);
  mulib_dynbox_mem_(&c, 0x89, 0, 12, offsetof(mulib_dynbox_frame_, out_gp));
  mulib_dynbox_mem_(&c, 0x89, 2, 12, offsetof(mulib_dynbox_frame_, out_gp) + 8);
  for (i = 0; i < 2; ++i) mulib_dynbox_xmm_(&c, 1, i, 12, offsetof(mulib_dynbox_frame_, out_fp) + i * 8);
  if (cif->result.longdouble) {
    mulib_dynbox_byte_(&c, 0x41); mulib_dynbox_byte_(&c, 0xdb); mulib_dynbox_byte_(&c, 0xbc); mulib_dynbox_byte_(&c, 0x24);
    mulib_dynbox_u32_(&c, (uint32_t)offsetof(mulib_dynbox_frame_, out_ld));
  }
  mulib_dynbox_bytes_(&c, end, sizeof end); return mulib_dynbox_map_(&cif->code, &c);
}
static int mulib_dynbox_prepare_(mulib_dynbox_cif_t **out, const mulib_dynbox_type_t *result,
                                  size_t fixed, size_t count, const mulib_dynbox_type_t *const *types, int variadic) {
  mulib_dynbox_cif_t *c;
  size_t i, j, budget = 65536, scratch = 0;
  unsigned gp, fp = 0;
  int r;
  if (!out || *out || !result || (count && !types) || count > MULIB_DYNBOX_MAX_ARGS
      || (variadic && (!fixed || fixed > count))) return MULIB_DYNBOX_EINVAL;
  if (!mulib_dynbox_supported()) return MULIB_DYNBOX_ENOTSUP;
  c = (mulib_dynbox_cif_t *)calloc(1, sizeof *c);
  if (!c) return MULIB_DYNBOX_ENOMEM;
  c->args = (mulib_dynbox_layout_ *)calloc(count ? count : 1, sizeof *c->args);
  if (!c->args) { free(c); return MULIB_DYNBOX_ENOMEM; }
  c->count = count; c->fixed = fixed; c->variadic = (unsigned)variadic;
  r = mulib_dynbox_make_layout_(result, &c->result, &budget);
  if (r) goto fail;
  gp = c->result.memory && !c->result.longdouble ? 1u : 0u;
  scratch = c->result.size;
  for (i = 0; i < count; ++i) {
    mulib_dynbox_layout_ *l = &c->args[i];
    unsigned need_gp = 0, need_fp = 0;
    r = mulib_dynbox_make_layout_(types[i], l, &budget);
    if (r) goto fail;
    if (!l->size || (variadic && i >= fixed && (types[i]->kind == MULIB_DYNBOX_FLOAT
        || types[i]->kind == MULIB_DYNBOX_SINT8 || types[i]->kind == MULIB_DYNBOX_UINT8
        || types[i]->kind == MULIB_DYNBOX_SINT16 || types[i]->kind == MULIB_DYNBOX_UINT16))) { r = MULIB_DYNBOX_EINVAL; goto fail; }
    if (!mulib_dynbox_align_(scratch, l->alignment, &scratch) || scratch > MULIB_DYNBOX_MAX_BYTES
        || l->size > MULIB_DYNBOX_MAX_BYTES - scratch) { r = MULIB_DYNBOX_ERANGE; goto fail; }
    l->scratch = scratch; scratch += l->size;
    for (j = 0; j < 2; ++j) { need_gp += l->cls[j] == 1; need_fp += l->cls[j] == 2; }
    if (l->memory || gp + need_gp > 6 || fp + need_fp > 8) {
      size_t a = l->alignment > 8 ? l->alignment : 8;
      l->memory = 1;
      if (!mulib_dynbox_align_(c->stack_size, a, &c->stack_size)
          || c->stack_size > MULIB_DYNBOX_MAX_BYTES || l->size > MULIB_DYNBOX_MAX_BYTES - c->stack_size) { r = MULIB_DYNBOX_ERANGE; goto fail; }
      l->stack = c->stack_size; c->stack_size += l->size;
      (void)mulib_dynbox_align_(c->stack_size, 8, &c->stack_size);
    } else for (j = 0; j < 2; ++j) {
      if (l->cls[j] == 1) l->reg[j] = gp++;
      else if (l->cls[j] == 2) l->reg[j] = fp++;
    }
  }
  c->scratch_size = scratch; c->fp_count = fp;
  (void)mulib_dynbox_align_(c->stack_size, 16, &c->stack_size);
  r = mulib_dynbox_bridge_(c);
  if (r) goto fail;
  *out = c; return 0;
fail:
  free(c->args); free(c); return r;
}
int mulib_dynbox_prepare(mulib_dynbox_cif_t **out, const mulib_dynbox_type_t *result,
                         size_t count, const mulib_dynbox_type_t *const *types) {
  return mulib_dynbox_prepare_(out, result, count, count, types, 0);
}
int mulib_dynbox_prepare_variadic(mulib_dynbox_cif_t **out, const mulib_dynbox_type_t *result,
                                  size_t fixed, size_t count, const mulib_dynbox_type_t *const *types) {
  return mulib_dynbox_prepare_(out, result, fixed, count, types, 1);
}
int mulib_dynbox_destroy(mulib_dynbox_cif_t *c) {
  int r;
  if (!c) return 0;
  r = mulib_dynbox_unmap_(&c->code);
  if (r) return r;
  free(c->args); free(c); return 0;
}
size_t mulib_dynbox_argument_count(const mulib_dynbox_cif_t *c) { return c ? c->count : 0; }
size_t mulib_dynbox_result_size(const mulib_dynbox_cif_t *c) { return c ? c->result.size : 0; }
static uint64_t mulib_dynbox_word_load_(const unsigned char *p, size_t n, unsigned sign) {
  uint64_t value = 0;
  memcpy(&value, p, n);
  if (sign && n < 8 && (value & (UINT64_C(1) << (n * 8 - 1)))) value |= UINT64_MAX << (n * 8);
  return value;
}
static void mulib_dynbox_result_unpack_(const mulib_dynbox_layout_ *l, const mulib_dynbox_frame_ *f, void *out) {
  size_t i;
  unsigned gp = 0, fp = 0;
  if (l->longdouble) { memcpy(out, f->out_ld, l->size); return; }
  for (i = 0; i < 2 && i * 8 < l->size; ++i) {
    size_t n = l->size - i * 8; if (n > 8) n = 8;
    if (l->cls[i] == 1) memcpy((unsigned char *)out + i * 8, &f->out_gp[gp++], n);
    else if (l->cls[i] == 2) memcpy((unsigned char *)out + i * 8, &f->out_fp[fp++], n);
  }
}
int mulib_dynbox_call(const mulib_dynbox_cif_t *c, mulib_dynbox_fn function,
                      void *result, size_t capacity, const void *const *args) {
  mulib_dynbox_frame_ f;
  void (*bridge)(mulib_dynbox_frame_ *);
  size_t i, j;
  if (!c || !function || (c->count && !args) || (c->result.size && !result)) return MULIB_DYNBOX_EINVAL;
  if (capacity < c->result.size) return MULIB_DYNBOX_ERANGE;
  for (i = 0; i < c->count; ++i) if (!args[i]) return MULIB_DYNBOX_EINVAL;
  memset(&f, 0, sizeof f); f.function = function; f.stack_size = c->stack_size;
  if (c->stack_size && !(f.stack = (unsigned char *)calloc(1, c->stack_size))) return MULIB_DYNBOX_ENOMEM;
  if (c->result.memory && !c->result.longdouble) memcpy(&f.gp[0], &result, sizeof result);
  for (i = 0; i < c->count; ++i) {
    const mulib_dynbox_layout_ *l = &c->args[i];
    if (l->memory) { memcpy(f.stack + l->stack, args[i], l->size); continue; }
    for (j = 0; j < 2 && j * 8 < l->size; ++j) {
      size_t n = l->size - j * 8; uint64_t word;
      if (n > 8) n = 8;
      word = mulib_dynbox_word_load_((const unsigned char *)args[i] + j * 8, n, l->is_signed);
      if (l->cls[j] == 1) f.gp[l->reg[j]] = word;
      else if (l->cls[j] == 2) f.fp[l->reg[j]] = word;
    }
  }
  memcpy(&bridge, &c->code.entry, sizeof bridge); bridge(&f);
  if (c->result.size && (!c->result.memory || c->result.longdouble)) mulib_dynbox_result_unpack_(&c->result, &f, result);
  free(f.stack); return 0;
}
/* Generated entry saves all incoming registers before entering ordinary C.
 * Scratch and argument-pointer arrays live in its aligned, probed native frame. */
static void mulib_dynbox_dispatch_(mulib_dynbox_closure_t *closure, mulib_dynbox_frame_ *f) {
  const mulib_dynbox_cif_t *c = closure->cif;
  size_t i, j, frame = 0, scratch = 0;
  unsigned gp = 0, fp = 0;
  unsigned char *values;
  void **args;
  (void)mulib_dynbox_align_(sizeof *f,16,&frame);
  (void)mulib_dynbox_align_(c->scratch_size,16,&scratch);
  values = (unsigned char *)f + frame; args = (void **)(values + scratch);
  memset(f->out_gp, 0, sizeof f->out_gp); memset(f->out_fp, 0, sizeof f->out_fp); memset(f->out_ld, 0, sizeof f->out_ld);
  memset(values, 0, c->scratch_size);
  for (i = 0; i < c->count; ++i) {
    const mulib_dynbox_layout_ *l = &c->args[i];
    args[i] = values + l->scratch;
    if (l->memory) { memcpy(args[i], f->stack + l->stack, l->size); continue; }
    for (j = 0; j < 2 && j * 8 < l->size; ++j) {
      size_t n = l->size - j * 8; if (n > 8) n = 8;
      if (l->cls[j] == 1) memcpy((unsigned char *)args[i] + j * 8, &f->gp[l->reg[j]], n);
      else if (l->cls[j] == 2) memcpy((unsigned char *)args[i] + j * 8, &f->fp[l->reg[j]], n);
    }
  }
  closure->callback(c, c->result.size ? values : NULL, args, closure->user);
  if (c->result.longdouble) memcpy(f->out_ld, values, c->result.size);
  else if (c->result.memory) {
    void *destination;
    memcpy(&destination, &f->gp[0], sizeof destination); memcpy(destination, values, c->result.size); f->out_gp[0] = f->gp[0];
  } else for (i = 0; i < 2 && i * 8 < c->result.size; ++i) {
    size_t n = c->result.size - i * 8;
    uint64_t word;
    if (n > 8) n = 8;
    word = mulib_dynbox_word_load_(values + i * 8, n, c->result.is_signed);
    if (c->result.cls[i] == 1) f->out_gp[gp++] = word;
    else if (c->result.cls[i] == 2) f->out_fp[fp++] = word;
  }
}
int mulib_dynbox_closure_create(mulib_dynbox_closure_t **out, const mulib_dynbox_cif_t *cif,
                                mulib_dynbox_callback_fn callback, void *user) {
  static const unsigned char start[] = {0xf3,0x0f,0x1e,0xfa,0x55,0x48,0x89,0xe5};
  mulib_dynbox_closure_t *closure;
  mulib_dynbox_code_ code;
  void (*dispatch)(mulib_dynbox_closure_t *, mulib_dynbox_frame_ *) = mulib_dynbox_dispatch_;
  uint64_t address = 0;
  unsigned i;
  size_t frame = 0, scratch = 0;
  int r;
  if (!out || *out || !cif || !callback) return MULIB_DYNBOX_EINVAL;
  if (!mulib_dynbox_supported() || cif->variadic || sizeof dispatch != sizeof address) return MULIB_DYNBOX_ENOTSUP;
  closure = (mulib_dynbox_closure_t *)calloc(1, sizeof *closure);
  if (!closure) return MULIB_DYNBOX_ENOMEM;
  closure->cif = cif; closure->callback = callback; closure->user = user;
  memset(&code, 0, sizeof code); mulib_dynbox_bytes_(&code, start, sizeof start);
  (void)mulib_dynbox_align_(sizeof(mulib_dynbox_frame_), 16, &frame);
  (void)mulib_dynbox_align_(cif->scratch_size,16,&scratch);
  (void)mulib_dynbox_align_(frame + scratch + cif->count * sizeof(void *),16,&frame);
  mulib_dynbox_stack_(&code,frame);
  for (i = 0; i < 6; ++i) mulib_dynbox_mem_(&code, 0x89, mulib_dynbox_gp_regs_[i], 4, offsetof(mulib_dynbox_frame_, gp) + i * 8);
  for (i = 0; i < 8; ++i) mulib_dynbox_xmm_(&code, 1, i, 4, offsetof(mulib_dynbox_frame_, fp) + i * 8);
  mulib_dynbox_mem_(&code, 0x8d, 0, 5, 16);
  mulib_dynbox_mem_(&code, 0x89, 0, 4, offsetof(mulib_dynbox_frame_, stack));
  mulib_dynbox_byte_(&code, 0x48); mulib_dynbox_byte_(&code, 0xbf); mulib_dynbox_u64_(&code, (uint64_t)(uintptr_t)closure);
  mulib_dynbox_byte_(&code, 0x48); mulib_dynbox_byte_(&code, 0x89); mulib_dynbox_byte_(&code, 0xe6);
  memcpy(&address, &dispatch, sizeof dispatch < sizeof address ? sizeof dispatch : sizeof address);
  mulib_dynbox_byte_(&code, 0x48); mulib_dynbox_byte_(&code, 0xb8); mulib_dynbox_u64_(&code, address);
  mulib_dynbox_byte_(&code, 0xff); mulib_dynbox_byte_(&code, 0xd0);
  mulib_dynbox_mem_(&code, 0x8b, 0, 4, offsetof(mulib_dynbox_frame_, out_gp));
  mulib_dynbox_mem_(&code, 0x8b, 2, 4, offsetof(mulib_dynbox_frame_, out_gp) + 8);
  for (i = 0; i < 2; ++i) mulib_dynbox_xmm_(&code, 0, i, 4, offsetof(mulib_dynbox_frame_, out_fp) + i * 8);
  if (cif->result.longdouble) {
    mulib_dynbox_byte_(&code, 0xdb); mulib_dynbox_byte_(&code, 0xac); mulib_dynbox_byte_(&code, 0x24);
    mulib_dynbox_u32_(&code, (uint32_t)offsetof(mulib_dynbox_frame_, out_ld));
  }
  mulib_dynbox_byte_(&code, 0xc9); mulib_dynbox_byte_(&code, 0xc3);
  r = mulib_dynbox_map_(&closure->code, &code);
  if (r) { free(closure); return r; }
  *out = closure; return 0;
}
mulib_dynbox_fn mulib_dynbox_closure_function(const mulib_dynbox_closure_t *closure) {
  mulib_dynbox_fn function = NULL;
  if (closure) memcpy(&function, &closure->code.entry, sizeof function);
  return function;
}
int mulib_dynbox_closure_destroy(mulib_dynbox_closure_t *closure) {
  int r;
  if (!closure) return 0;
  r = mulib_dynbox_unmap_(&closure->code);
  if (r) return r;
  free(closure); return 0;
}
#ifdef __cplusplus
}
#endif
#undef MULIB_DYNBOX_ALIGNOF_
#endif
