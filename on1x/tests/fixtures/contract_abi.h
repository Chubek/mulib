#ifndef ON1X_CONTRACT_ABI_FIXTURE_H
#define ON1X_CONTRACT_ABI_FIXTURE_H
/* Test ABI only: statuses assigned by tests are not independent reviews. */
typedef short fixture_short;
#ifdef __cplusplus
typedef bool fixture_bool;
#else
typedef _Bool fixture_bool;
#endif
typedef double fixture_double;
typedef float fixture_float;
typedef unsigned char fixture_u8;
typedef unsigned short fixture_u16;
typedef unsigned long long fixture_u64;
struct contract_pair { int first, second; };
int contract_add(int a, int b);
fixture_short contract_narrow(fixture_short x);
void contract_note(int x);
int contract_count(void);
static inline long long contract_inline(long long x) { return x; }
int contract_io(int x);
fixture_bool contract_bool(fixture_bool x);
fixture_double contract_double(fixture_double x);
fixture_double contract_mix(fixture_bool flag, fixture_double x, int n);
fixture_float contract_float(fixture_float x);
fixture_u8 contract_u8(fixture_u8 x);
fixture_u16 contract_u16(fixture_u16 x);
fixture_u64 contract_u64(fixture_u64 x);
unsigned int contract_unsigned(unsigned int x);
int *contract_pointer(int *p, unsigned long n);
struct contract_pair contract_pair(struct contract_pair pair);
int contract_out(int *out);
int contract_variadic(int n, ...);
int contract_callback(int (*callback)(int), int x);
int *contract_create(void);
void contract_drop(int *p);
#ifdef CONTRACT_ALT
int contract_conditional(int x);
#endif
#endif
