#line 70 "presumed.h"
typedef unsigned long long Word;
typedef unsigned long long Word;
typedef int Callback(int);
extern Callback *callback;
extern Word wide;
Word wide = 0ull;
long signed_wide;
extern char plain_char;
enum E { negative = -1, positive = 2 };
struct Child {
    union { const int a; volatile int b; };
    signed int narrow : 3;
    unsigned : 0;
    _Bool flag : 1;
};
struct Parent { struct Child children[2]; int *const pointer; };
struct Tail { int length; const unsigned data[]; };
union TailWrapper { struct Tail tail; unsigned plain; };
struct Opaque;
extern struct Opaque *opaque;
void old_style();
void span(int x[static restrict 4]);
void span(int *x);
void dynamic(unsigned n, register int x[static n]);
void variable(int x[*]);
void variadic(register unsigned x, ...);
_Noreturn void stop(void);
void upgrade();
void upgrade(register unsigned x);

#line 120 "not-an-open-file.c"
unsigned mask(unsigned x, unsigned secret) { return (x & 255u) | (secret ^ secret); }
unsigned external_call(unsigned x);
unsigned opaque_body(unsigned x) { return external_call(x); }
unsigned volatile_body(volatile unsigned x) { return x; }
unsigned lookup(unsigned x) { unsigned a[2] = {3u, 9u}; return x < 2u ? a[x] : 0u; }
unsigned unsafe_lookup(unsigned x) { unsigned a[2] = {3u, 9u}; return a[x]; }
unsigned width_test(unsigned x) { return x & 65535u; }
static inline unsigned local(void) { return 0u; }
inline unsigned inline_only(unsigned x) { return x; }
