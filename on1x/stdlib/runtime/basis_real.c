#include "basis_native.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void fail(const char *msg) {fprintf(stderr,"On1x Basis real: %s\n",msg);exit(73);}
struct text {struct text *next; char data[64];};
static struct text *pool;
static size_t count;
static int registered;
static void release_pool(void) {
  while(pool){struct text *next=pool->next;free(pool);pool=next;}
}
static const char *format(double v) {
  if(!isfinite(v))fail("NaN/Infinity require future IEEEReal data model");
  if(count>=65536)fail("real snapshot limit");
  if(!registered){if(atexit(release_pool))fail("atexit failed");registered=1;}
  struct text *t=(struct text *)calloc(1,sizeof(*t));
  if(!t)fail("allocation failed");
  if(snprintf(t->data,sizeof(t->data),"%.17g",v)<=0)fail("format failed");
  t->next=pool;pool=t;++count;
  return t->data;
}
static double parse(const char *s) {
  char *end;
  double v;
  if(!s || strlen(s)>128 || !*s)fail("invalid real argument");
  errno=0;v=strtod(s,&end);
  if(end==s || *end || errno==ERANGE || !isfinite(v))fail("invalid or nonfinite real");
  return v;
}
const char *basis_native_real_add(const char *a,const char *b){return format(parse(a)+parse(b));}
const char *basis_native_real_sub(const char *a,const char *b){return format(parse(a)-parse(b));}
const char *basis_native_real_mul(const char *a,const char *b){return format(parse(a)*parse(b));}
const char *basis_native_real_div(const char *a,const char *b){double d=parse(b);if(d==0.0)fail("division by zero");return format(parse(a)/d);}
int64_t basis_native_real_compare(const char *a,const char *b){double x=parse(a),y=parse(b);return (x>y)-(x<y);}
const char *basis_native_real_neg(const char *a){return format(-parse(a));}
const char *basis_native_real_abs(const char *a){return format(fabs(parse(a)));}
const char *basis_native_math_sqrt(const char *a){double x=parse(a);if(x<0)fail("sqrt domain");return format(sqrt(x));}
const char *basis_native_math_exp(const char *a){return format(exp(parse(a)));}
const char *basis_native_math_log(const char *a){double x=parse(a);if(x<=0)fail("log domain");return format(log(x));}
const char *basis_native_math_sin(const char *a){return format(sin(parse(a)));}
const char *basis_native_math_cos(const char *a){return format(cos(parse(a)));}
const char *basis_native_math_pow(const char *a,const char *b){return format(pow(parse(a),parse(b)));}
bool basis_native_ieee_isfinite(const char *a){return isfinite(parse(a));}
