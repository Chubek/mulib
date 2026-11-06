#define MULIB_IMPL_DYNBOX
#include "mulib-dynbox.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FN(f) ((mulib_dynbox_fn)(f))
static int32_t increment(int32_t x) { return x + 7; }
static int8_t narrow(int8_t a, uint8_t b, int16_t c, uint16_t d) { return (int8_t)(a + b + c + d); }
static double mixed(int64_t a, double b, float c, void *d) { return a + b + c + *(int *)d; }
static int64_t integers(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f, int64_t g, int64_t h) {
  return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 8*h;
}
static double floats(double a, double b, double c, double d, double e, double f, double g, double h, double i, double j) {
  return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 8*h + 9*i + 10*j;
}
typedef struct pair { double x; int64_t y; } pair;
typedef struct fp_pair { double x, y; } fp_pair;
typedef struct big { double x, y, z; int64_t k; } big;
typedef struct triple { float a, b, c; } triple;
typedef struct ld_box { long double x; } ld_box;
static pair pair_fn(pair a, double b) { a.x += b; a.y -= 13; return a; }
static fp_pair fp_fn(fp_pair a, fp_pair b) { a.x += b.y; a.y -= b.x; return a; }
static big big_fn(big a, pair b, int64_t n) { a.x += b.x; a.k += b.y + n; return a; }
static triple triple_fn(triple a) { a.a += 1; a.b += 2; a.c += 3; return a; }
static long double ld_fn(int64_t a, long double b, double c) { return a + b + c; }
static ld_box ld_box_fn(ld_box x) { x.x += 2; return x; }
/* Aggregate assignment must roll back BOTH register classes on overflow. */
static double rollback(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f, pair p, double x) {
  return a+b+c+d+e+f+p.x+p.y+x;
}
static double variadic(int32_t n, ...) {
  va_list ap;
  double total = 0;
  int i;
  va_start(ap, n);
  for (i = 0; i < n; ++i) total += va_arg(ap, double);
  va_end(ap); return total;
}
static long double variadic_mixed(int32_t ignored, ...) {
  va_list ap;
  int a;
  double b;
  long double c;
  pair p;
  (void)ignored; va_start(ap, ignored);
  a = va_arg(ap, int); b = va_arg(ap, double); c = va_arg(ap, long double); p = va_arg(ap, pair);
  va_end(ap); return a + b + c + p.x + p.y;
}
static void *pointer_fn(void *p) { return p; }
static int touched;
static void void_fn(void) { ++touched; }
static int64_t zero_fn(void) { return INT64_C(-1234567890123); }

static void check_calls(void) {
  mulib_dynbox_cif_t *c = NULL;
  const mulib_dynbox_type_t *types[16];
  const void *args[16];
  int32_t a = -12, out = 91;
  size_t i;
  types[0] = &mulib_dynbox_type_sint32; args[0] = &a;
  assert(!mulib_dynbox_prepare(&c, types[0], 1, types));
  assert(mulib_dynbox_argument_count(c) == 1 && mulib_dynbox_result_size(c) == 4);
  assert(mulib_dynbox_call(c, FN(increment), &out, 3, args) == MULIB_DYNBOX_ERANGE && out == 91);
  assert(!mulib_dynbox_call(c, FN(increment), &out, sizeof out, args) && out == -5);
  assert(!mulib_dynbox_destroy(c)); c = NULL;
  {
    int8_t x = -31, result; uint8_t y = 13; int16_t z = -123; uint16_t w = 150;
    types[0] = &mulib_dynbox_type_sint8; types[1] = &mulib_dynbox_type_uint8;
    types[2] = &mulib_dynbox_type_sint16; types[3] = &mulib_dynbox_type_uint16;
    args[0] = &x; args[1] = &y; args[2] = &z; args[3] = &w;
    assert(!mulib_dynbox_prepare(&c, types[0], 4, types));
    assert(!mulib_dynbox_call(c, FN(narrow), &result, sizeof result, args) && result == narrow(x,y,z,w));
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  {
    int64_t x = -9; double y = 1.25, result; float z = 2.5f; int v = 4; void *p = &v;
    types[0] = &mulib_dynbox_type_sint64; types[1] = &mulib_dynbox_type_double;
    types[2] = &mulib_dynbox_type_float; types[3] = &mulib_dynbox_type_pointer;
    args[0] = &x; args[1] = &y; args[2] = &z; args[3] = &p;
    assert(!mulib_dynbox_prepare(&c, types[1], 4, types));
    assert(!mulib_dynbox_call(c, FN(mixed), &result, sizeof result, args) && result == mixed(x,y,z,p));
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  {
    int64_t values[8], result;
    for (i = 0; i < 8; ++i) { values[i] = (int64_t)i - 4; types[i] = &mulib_dynbox_type_sint64; args[i] = &values[i]; }
    assert(!mulib_dynbox_prepare(&c, types[0], 8, types));
    assert(!mulib_dynbox_call(c, FN(integers), &result, sizeof result, args));
    assert(result == integers(values[0],values[1],values[2],values[3],values[4],values[5],values[6],values[7]));
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  {
    double values[10], result;
    for (i = 0; i < 10; ++i) { values[i] = (double)i + .5; types[i] = &mulib_dynbox_type_double; args[i] = &values[i]; }
    assert(!mulib_dynbox_prepare(&c, types[0], 10, types));
    assert(!mulib_dynbox_call(c, FN(floats), &result, sizeof result, args));
    assert(result == floats(values[0],values[1],values[2],values[3],values[4],values[5],values[6],values[7],values[8],values[9]));
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  assert(!mulib_dynbox_prepare(&c, &mulib_dynbox_type_void, 0, NULL));
  assert(!mulib_dynbox_call(c, FN(void_fn), NULL, 0, NULL) && touched == 1);
  assert(!mulib_dynbox_destroy(c)); c = NULL;
  {
    int64_t result;
    assert(!mulib_dynbox_prepare(&c, &mulib_dynbox_type_sint64, 0, NULL));
    assert(!mulib_dynbox_call(c, FN(zero_fn), &result, sizeof result, NULL) && result == zero_fn());
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  {
    void *p = &a, *result = NULL;
    types[0] = &mulib_dynbox_type_pointer; args[0] = &p;
    assert(!mulib_dynbox_prepare(&c, types[0], 1, types));
    assert(!mulib_dynbox_call(c, FN(pointer_fn), &result, sizeof result, args) && result == p);
    assert(!mulib_dynbox_destroy(c));
  }
}
static mulib_dynbox_type_t pair_type, fp_type, big_type, triple_type, ld_type;
static void check_structs(void) {
  static const mulib_dynbox_type_t *const pair_fields[] = {&mulib_dynbox_type_double,&mulib_dynbox_type_sint64};
  static const mulib_dynbox_type_t *const fp_fields[] = {&mulib_dynbox_type_double,&mulib_dynbox_type_double};
  static const mulib_dynbox_type_t *const big_fields[] = {&mulib_dynbox_type_double,&mulib_dynbox_type_double,&mulib_dynbox_type_double,&mulib_dynbox_type_sint64};
  static const mulib_dynbox_type_t *const triple_fields[] = {&mulib_dynbox_type_float,&mulib_dynbox_type_float,&mulib_dynbox_type_float};
  static const mulib_dynbox_type_t *const ld_fields[] = {&mulib_dynbox_type_longdouble};
  const mulib_dynbox_type_t *types[8];
  const void *args[8];
  mulib_dynbox_cif_t *c = NULL;
  size_t offsets[4], i;
  pair p = {1.5,40}, q;
  double x = 2.5;
  assert(!mulib_dynbox_struct_init(&pair_type, pair_fields, 2, offsets));
  assert(pair_type.size == sizeof(pair) && offsets[1] == offsetof(pair,y));
  assert(!mulib_dynbox_struct_init(&fp_type, fp_fields, 2, NULL));
  assert(!mulib_dynbox_struct_init(&big_type, big_fields, 4, NULL));
  assert(!mulib_dynbox_struct_init(&triple_type, triple_fields, 3, NULL));
  assert(!mulib_dynbox_struct_init(&ld_type, ld_fields, 1, NULL));
  types[0] = &pair_type; types[1] = &mulib_dynbox_type_double; args[0] = &p; args[1] = &x;
  assert(!mulib_dynbox_prepare(&c, &pair_type, 2, types));
  assert(!mulib_dynbox_call(c, FN(pair_fn), &q, sizeof q, args) && q.x == 4 && q.y == 27);
  assert(!mulib_dynbox_destroy(c)); c = NULL;
  {
    fp_pair a = {2,3}, b = {4,5}, result;
    types[0] = types[1] = &fp_type; args[0] = &a; args[1] = &b;
    assert(!mulib_dynbox_prepare(&c, &fp_type, 2, types));
    assert(!mulib_dynbox_call(c, FN(fp_fn), &result, sizeof result, args) && result.x == 7 && result.y == -1);
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  {
    big a = {1,2,3,4}, result; int64_t n = 7;
    types[0] = &big_type; types[1] = &pair_type; types[2] = &mulib_dynbox_type_sint64;
    args[0] = &a; args[1] = &p; args[2] = &n;
    assert(!mulib_dynbox_prepare(&c, &big_type, 3, types));
    assert(!mulib_dynbox_call(c, FN(big_fn), &result, sizeof result, args));
    assert(result.x == 2.5 && result.y == 2 && result.z == 3 && result.k == 51);
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  {
    triple a = {1,2,3}, result;
    types[0] = &triple_type; args[0] = &a;
    assert(!mulib_dynbox_prepare(&c, &triple_type, 1, types));
    assert(!mulib_dynbox_call(c, FN(triple_fn), &result, sizeof result, args));
    assert(result.a == 2 && result.b == 4 && result.c == 6);
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  {
    int64_t a = 2; long double b = 3.125L, result; double d = 4.25;
    types[0] = &mulib_dynbox_type_sint64; types[1] = &mulib_dynbox_type_longdouble; types[2] = &mulib_dynbox_type_double;
    args[0] = &a; args[1] = &b; args[2] = &d;
    assert(!mulib_dynbox_prepare(&c, types[1], 3, types));
    assert(!mulib_dynbox_call(c, FN(ld_fn), &result, sizeof result, args) && result == ld_fn(a,b,d));
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  {
    ld_box a = {3.25L}, result;
    types[0] = &ld_type; args[0] = &a;
    assert(!mulib_dynbox_prepare(&c, &ld_type, 1, types));
    assert(!mulib_dynbox_call(c, FN(ld_box_fn), &result, sizeof result, args) && result.x == 5.25L);
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  {
    int64_t values[6]; double result;
    for (i = 0; i < 6; ++i) { values[i] = (int64_t)i; types[i] = &mulib_dynbox_type_sint64; args[i] = &values[i]; }
    types[6] = &pair_type; types[7] = &mulib_dynbox_type_double; args[6] = &p; args[7] = &x;
    assert(!mulib_dynbox_prepare(&c, types[7], 8, types));
    assert(!mulib_dynbox_call(c, FN(rollback), &result, sizeof result, args) && result == 59);
    assert(!mulib_dynbox_destroy(c));
  }
}
static void check_variadics(void) {
  mulib_dynbox_cif_t *c = NULL;
  const mulib_dynbox_type_t *types[12];
  const void *args[12];
  int32_t n = 10;
  double values[10], result;
  size_t i;
  types[0] = &mulib_dynbox_type_sint32; args[0] = &n;
  for (i = 0; i < 10; ++i) { values[i] = i + .5; types[i+1] = &mulib_dynbox_type_double; args[i+1] = &values[i]; }
  assert(!mulib_dynbox_prepare_variadic(&c, &mulib_dynbox_type_double, 1, 11, types));
  assert(!mulib_dynbox_call(c, FN(variadic), &result, sizeof result, args) && result == 50);
  assert(!mulib_dynbox_destroy(c)); c = NULL;
  {
    int32_t a = -2; double b = 3.5; long double d = 5.25L, out; pair p = {1.25,9};
    types[1] = &mulib_dynbox_type_sint32; types[2] = &mulib_dynbox_type_double;
    types[3] = &mulib_dynbox_type_longdouble; types[4] = &pair_type;
    args[1] = &a; args[2] = &b; args[3] = &d; args[4] = &p;
    assert(!mulib_dynbox_prepare_variadic(&c, &mulib_dynbox_type_longdouble, 1, 5, types));
    assert(!mulib_dynbox_call(c, FN(variadic_mixed), &out, sizeof out, args) && out == 17);
    assert(!mulib_dynbox_destroy(c)); c = NULL;
    types[1] = &mulib_dynbox_type_float;
    assert(mulib_dynbox_prepare_variadic(&c, &mulib_dynbox_type_void, 1, 2, types) == MULIB_DYNBOX_EINVAL && !c);
  }
}
static void compare_callback(const mulib_dynbox_cif_t *c, void *result, void *const *args, void *user) {
  const int *a = *(const int *const *)args[0], *b = *(const int *const *)args[1];
  assert(mulib_dynbox_argument_count(c) == 2); ++*(int *)user;
  *(int32_t *)result = (*a > *b) - (*a < *b);
}
static void pair_callback(const mulib_dynbox_cif_t *c, void *result, void *const *args, void *user) {
  (void)c; (void)user; *(pair *)result = pair_fn(*(pair *)args[0], *(double *)args[1]);
}
static void big_callback(const mulib_dynbox_cif_t *c, void *result, void *const *args, void *user) {
  (void)c; (void)user; *(big *)result = big_fn(*(big *)args[0], *(pair *)args[1], *(int64_t *)args[2]);
}
static void ld_callback(const mulib_dynbox_cif_t *c, void *result, void *const *args, void *user) {
  (void)c; (void)user; *(long double *)result = *(long double *)args[0] + 1.25L;
}
static void stack_callback(const mulib_dynbox_cif_t *c, void *result, void *const *args, void *user) {
  size_t i; int64_t total = 0; (void)c; (void)user;
  for (i = 0; i < 8; ++i) total += *(int64_t *)args[i] * (int64_t)(i+1);
  *(int64_t *)result = total;
}
typedef struct blob { uint64_t words[1024]; } blob;
static uint64_t blob_fn(blob a,int64_t shift) {
  return a.words[0]*3 + a.words[511]*5 + a.words[1023]*7 + (uint64_t)shift;
}
static void blob_callback(const mulib_dynbox_cif_t *c,void *result,void *const *args,void *user) {
  (void)c; (void)user; *(uint64_t *)result = blob_fn(*(blob *)args[0],*(int64_t *)args[1]);
}
static void check_large_stack(void) {
  const mulib_dynbox_type_t *fields[1024], *types[2], *large_fields[8];
  const void *args[2];
  mulib_dynbox_type_t blob_type, large_type;
  mulib_dynbox_cif_t *c = NULL;
  mulib_dynbox_closure_t *closure = NULL;
  mulib_dynbox_fn entry;
  uint64_t (*fn)(blob,int64_t), result;
  int64_t shift = 11;
  blob value = {{0}};
  size_t i;
  for (i = 0; i < 1024; ++i) fields[i] = &mulib_dynbox_type_uint64;
  assert(!mulib_dynbox_struct_init(&blob_type,fields,1024,NULL) && blob_type.size == sizeof value);
  value.words[0] = 2; value.words[511] = 3; value.words[1023] = 4;
  types[0] = &blob_type; types[1] = &mulib_dynbox_type_sint64; args[0] = &value; args[1] = &shift;
  assert(!mulib_dynbox_prepare(&c,&mulib_dynbox_type_uint64,2,types));
  assert(!mulib_dynbox_call(c,(mulib_dynbox_fn)blob_fn,&result,sizeof result,args) && result == 60);
  assert(!mulib_dynbox_closure_create(&closure,c,blob_callback,NULL));
  entry = mulib_dynbox_closure_function(closure); memcpy(&fn,&entry,sizeof fn);
  assert(fn(value,shift) == 60);
  assert(!mulib_dynbox_closure_destroy(closure) && !mulib_dynbox_destroy(c)); c = NULL;
  for (i = 0; i < 8; ++i) large_fields[i] = &blob_type;
  assert(!mulib_dynbox_struct_init(&large_type,large_fields,8,NULL) && large_type.size == 65536);
  types[0] = &large_type;
  assert(mulib_dynbox_prepare(&c,&mulib_dynbox_type_uint64,1,types) == MULIB_DYNBOX_ERANGE && !c);
}
typedef struct recursive_context { mulib_dynbox_fn entry; } recursive_context;
static void recursive_callback(const mulib_dynbox_cif_t *c,void *result,void *const *args,void *user) {
  recursive_context *ctx = (recursive_context *)user;
  int64_t n = *(int64_t *)args[0], child, previous = n-1;
  const void *next[] = {&previous};
  if (!n) { *(int64_t *)result = 1; return; }
  assert(!mulib_dynbox_call(c,ctx->entry,&child,sizeof child,next));
  *(int64_t *)result = n*child;
}
static void check_closures(void) {
  const mulib_dynbox_type_t *types[8];
  mulib_dynbox_cif_t *c = NULL;
  mulib_dynbox_closure_t *closure = NULL;
  mulib_dynbox_fn entry;
  size_t i;
  int calls = 0, data[] = {9,1,4,7,-2,3};
  int (*compare)(const void *, const void *);
  types[0] = types[1] = &mulib_dynbox_type_pointer;
  assert(!mulib_dynbox_prepare(&c, &mulib_dynbox_type_sint32, 2, types));
  assert(!mulib_dynbox_closure_create(&closure, c, compare_callback, &calls));
  entry = mulib_dynbox_closure_function(closure); memcpy(&compare, &entry, sizeof compare);
  qsort(data, sizeof data/sizeof data[0], sizeof data[0], compare);
  for (i = 1; i < sizeof data/sizeof data[0]; ++i) assert(data[i-1] <= data[i]);
  assert(calls > 0);
  assert(!mulib_dynbox_closure_destroy(closure)); closure = NULL;
  assert(!mulib_dynbox_destroy(c)); c = NULL;
  {
    pair (*fn)(pair,double), result, p = {4.5,99};
    types[0] = &pair_type; types[1] = &mulib_dynbox_type_double;
    assert(!mulib_dynbox_prepare(&c, &pair_type, 2, types));
    assert(!mulib_dynbox_closure_create(&closure, c, pair_callback, NULL));
    entry = mulib_dynbox_closure_function(closure); memcpy(&fn, &entry, sizeof fn);
    result = fn(p, 2.5); assert(result.x == 7 && result.y == 86);
    assert(!mulib_dynbox_closure_destroy(closure)); closure = NULL;
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  {
    big (*fn)(big,pair,int64_t), result, b = {1,2,3,4}; pair p = {5,6};
    types[0] = &big_type; types[1] = &pair_type; types[2] = &mulib_dynbox_type_sint64;
    assert(!mulib_dynbox_prepare(&c, &big_type, 3, types));
    assert(!mulib_dynbox_closure_create(&closure, c, big_callback, NULL));
    entry = mulib_dynbox_closure_function(closure); memcpy(&fn, &entry, sizeof fn);
    result = fn(b,p,8); assert(result.x == 6 && result.y == 2 && result.z == 3 && result.k == 18);
    assert(!mulib_dynbox_closure_destroy(closure)); closure = NULL;
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  {
    long double (*fn)(long double);
    types[0] = &mulib_dynbox_type_longdouble;
    assert(!mulib_dynbox_prepare(&c, types[0], 1, types));
    assert(!mulib_dynbox_closure_create(&closure, c, ld_callback, NULL));
    entry = mulib_dynbox_closure_function(closure); memcpy(&fn, &entry, sizeof fn);
    assert(fn(2.5L) == 3.75L);
    assert(!mulib_dynbox_closure_destroy(closure)); closure = NULL;
    assert(!mulib_dynbox_destroy(c)); c = NULL;
  }
  {
    int64_t (*fn)(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
    for (i = 0; i < 8; ++i) types[i] = &mulib_dynbox_type_sint64;
    assert(!mulib_dynbox_prepare(&c, types[0], 8, types));
    assert(!mulib_dynbox_closure_create(&closure, c, stack_callback, NULL));
    entry = mulib_dynbox_closure_function(closure); memcpy(&fn, &entry, sizeof fn);
    assert(fn(1,2,3,4,5,6,7,8) == 204);
    assert(!mulib_dynbox_closure_destroy(closure)); assert(!mulib_dynbox_destroy(c));
  }
  {
    int64_t (*fn)(int64_t);
    recursive_context ctx;
    c = NULL; closure = NULL;
    assert(!mulib_dynbox_prepare(&c,&mulib_dynbox_type_sint64,1,types));
    assert(!mulib_dynbox_closure_create(&closure,c,recursive_callback,&ctx));
    ctx.entry = mulib_dynbox_closure_function(closure); memcpy(&fn,&ctx.entry,sizeof fn);
    assert(fn(10) == 3628800);
    assert(!mulib_dynbox_closure_destroy(closure) && !mulib_dynbox_destroy(c));
  }
}
int main(void) {
  mulib_dynbox_cif_t *c = NULL;
  mulib_dynbox_type_t cyclic = {MULIB_DYNBOX_STRUCT, 8, 8, NULL, 1}, output = mulib_dynbox_type_uint8;
  const mulib_dynbox_type_t *cycle[] = {&cyclic};
  size_t offset = 77;
  cyclic.fields = cycle;
  assert(mulib_dynbox_struct_init(&output, cycle, 1, &offset) == MULIB_DYNBOX_EINVAL);
  assert(output.kind == MULIB_DYNBOX_UINT8 && offset == 77);
  assert(mulib_dynbox_prepare(NULL, &mulib_dynbox_type_void, 0, NULL) == MULIB_DYNBOX_EINVAL);
  if (!mulib_dynbox_supported()) {
    assert(mulib_dynbox_prepare(&c, &mulib_dynbox_type_void, 0, NULL) == MULIB_DYNBOX_ENOTSUP); return 0;
  }
  check_calls(); check_structs(); check_variadics(); check_closures(); check_large_stack();
  assert(!mulib_dynbox_destroy(NULL)); assert(!mulib_dynbox_closure_destroy(NULL));
  puts("dynbox: native calls, aggregates, variadics and closures passed"); return 0;
}
