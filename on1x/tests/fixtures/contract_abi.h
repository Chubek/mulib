#ifndef ON1X_CONTRACT_ABI_FIXTURE_H
#define ON1X_CONTRACT_ABI_FIXTURE_H
/* Test ABI only: statuses assigned by tests are not independent reviews. */
typedef short fixture_short;
struct contract_pair { int first, second; };
int contract_add(int a, int b);
fixture_short contract_narrow(fixture_short x);
void contract_note(int x);
int contract_count(void);
static inline long long contract_inline(long long x) { return x; }
int contract_io(int x);
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
