/* Linked against the assembly emitted by test_microcc; no MuLIB runtime. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#define DECLARE(name) extern uint64_t microcc_##name(uint64_t,uint64_t)
DECLARE(add);DECLARE(sub);DECLARE(mul);DECLARE(and);DECLARE(or);DECLARE(xor);
DECLARE(shl);DECLARE(shr);DECLARE(sar);DECLARE(eq);DECLARE(slt);DECLARE(ult);
DECLARE(add_imm);DECLARE(sub_imm);DECLARE(mul_imm);DECLARE(and_imm);DECLARE(or_imm);DECLARE(xor_imm);
DECLARE(shl_imm);DECLARE(shr_imm);DECLARE(sar_imm);DECLARE(eq_imm);DECLARE(slt_imm);DECLARE(ult_imm);DECLARE(add_wide);
#undef DECLARE
extern uint64_t microcc_loop(uint64_t),microcc_anf(uint64_t),microcc_dep(uint64_t);
extern uint64_t microcc_super(uint64_t),microcc_click(uint64_t),microcc_diamond(uint64_t);
extern uint64_t microcc_hazards(uint64_t),microcc_bigframe(uint64_t);
extern uint64_t microcc_far(uint64_t);
extern uint64_t microcc_memory(uint64_t *,uint64_t);
extern uint64_t microcc_call(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,
                           uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
static size_t calls;
uint64_t microcc_mutate(uint64_t *p,uint64_t x) {
  ++calls;*p=microcc_add(*p,x);return *p+9;
}
uint64_t microcc_ten(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,
                    uint64_t f,uint64_t g,uint64_t h,uint64_t i,uint64_t j) {
  ++calls;return a+2*b+3*c+4*d+5*e+6*f+7*g+8*h+9*i+10*j;
}
static uint64_t arithmetic_shift(uint64_t a,uint64_t b) {
  unsigned k;
  for(k=0;k<(b&63);++k) a=(a>>1)|(a&UINT64_C(0x8000000000000000));
  return a;
}
static void binary(uint64_t a,uint64_t b) {
  const uint64_t immediate=UINT64_MAX-16;
  uint64_t signed_less=(a>>63)!=(b>>63)?a>>63:(uint64_t)(a<b);
  assert(microcc_add(a,b)==a+b);assert(microcc_sub(a,b)==a-b);assert(microcc_mul(a,b)==a*b);
  assert(microcc_and(a,b)==(a&b));assert(microcc_or(a,b)==(a|b));assert(microcc_xor(a,b)==(a^b));
  assert(microcc_shl(a,b)==a<<(b&63));assert(microcc_shr(a,b)==a>>(b&63));
  assert(microcc_sar(a,b)==arithmetic_shift(a,b));assert(microcc_eq(a,b)==(uint64_t)(a==b));
  assert(microcc_slt(a,b)==signed_less);assert(microcc_ult(a,b)==(uint64_t)(a<b));
  assert(microcc_add_imm(a,b)==a+immediate);assert(microcc_sub_imm(a,b)==a-immediate);
  assert(microcc_mul_imm(a,b)==a*immediate);assert(microcc_and_imm(a,b)==(a&immediate));
  assert(microcc_or_imm(a,b)==(a|immediate));assert(microcc_xor_imm(a,b)==(a^immediate));
  assert(microcc_shl_imm(a,b)==a<<(immediate&63));assert(microcc_shr_imm(a,b)==a>>(immediate&63));
  assert(microcc_sar_imm(a,b)==arithmetic_shift(a,immediate));
  assert(microcc_eq_imm(a,b)==(uint64_t)(a==immediate));
  assert(microcc_slt_imm(a,b)==((a>>63)?(uint64_t)(a<immediate):0));
  assert(microcc_ult_imm(a,b)==(uint64_t)(a<immediate));
  assert(microcc_add_wide(a,b)==a+(UINT64_C(1)<<40));
  assert(microcc_hazards(a)==(a+3)*11);
  assert(microcc_bigframe(a)==a);
}
int main(void) {
  static const uint64_t boundaries[]={0,1,2,63,64,65,UINT64_MAX,UINT64_MAX-1,
    UINT64_C(0x8000000000000000),UINT64_C(0x7fffffffffffffff),UINT64_C(0x100000000),UINT64_C(0xffffffff)};
  uint64_t state=UINT64_C(0xcafe010203040506),a,b,word;size_t i,j;
  for(i=0;i<sizeof(boundaries)/sizeof(boundaries[0]);++i)
    for(j=0;j<sizeof(boundaries)/sizeof(boundaries[0]);++j) binary(boundaries[i],boundaries[j]);
  for(i=0;i<1000;++i) {
    state=state*UINT64_C(6364136223846793005)+1;a=state;
    state=state*UINT64_C(6364136223846793005)+1;b=state;binary(a,b);
    word=0;calls=0;assert(microcc_memory(&word,a)==5*a+9 && word==2*a && calls==1);
    calls=0;
    assert(microcc_call(a,b,3,4,5,6,7,8,9,10)==2*a+2*b+9+16+25+36+49+64+81+100);
    assert(calls==1);
  }
  for(i=0;i<50;++i) {
    uint64_t expected=i%2?22:11;
    assert(microcc_loop(i)==expected);assert(microcc_anf(i)==expected);
    assert(microcc_dep(i)==expected);assert(microcc_super(i)==expected);assert(microcc_click(i)==expected);
  }
  assert(microcc_diamond(0)==20 && microcc_diamond(1)==10);
  assert(microcc_far(0)==UINT64_C(0x123456789abcdef0)+299 && microcc_far(1)==42);
  return 0;
}
