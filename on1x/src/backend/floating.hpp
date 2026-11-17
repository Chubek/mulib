#pragma once
#include <on1x/backend/mir.hpp>
#include <bit>
#include <cfloat>
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) || defined(__NO_SIGNED_ZEROS__) || defined(__ASSOCIATIVE_MATH__) || defined(__RECIPROCAL_MATH__)
#error "On1x numeric evaluation requires strict IEEE math"
#endif
#ifdef __clang__
#pragma float_control(precise, on, push)
#endif
#if defined(__linux__) && defined(__x86_64__) && defined(__SSE2__)
#include <xmmintrin.h>
#endif

namespace on1x::mir::detail {
inline std::uint64_t canonical_float(std::uint64_t bits){
 return (bits&UINT64_C(0x7fffffffffffffff))>UINT64_C(0x7ff0000000000000)?UINT64_C(0x7ff8000000000000):bits;
}
inline std::int64_t encoded_float(double value){return std::bit_cast<std::int64_t>(canonical_float(std::bit_cast<std::uint64_t>(value)));}
inline double decoded_float(std::int64_t bits){return std::bit_cast<double>(bits);}
inline bool floating_op(Op op){return op>=Op::FloatAdd&&op<=Op::FloatSignbit;}
struct FloatResult { std::int64_t value=0; const char* trap=nullptr; };
inline FloatResult floating(Op op,std::int64_t a,std::int64_t b,const Location& at){
 if(effect(op)!=Effect::Pure){
#if defined(__linux__) && defined(__x86_64__) && defined(__SSE2__) && (defined(__GNUC__) || defined(__clang__)) && FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 && DBL_MIN_EXP == -1021 && FLT_EVAL_METHOD == 0
  (void)at;
  if((_mm_getcsr()&0xffc0u)!=0x1f80u)return {0,"unsupported f64 environment"};
#else
  throw Error(at,"ON1X-MIR-UNSUPPORTED: f64 evaluation requires Linux x86_64 SSE2 binary64 evaluation");
#endif
 }
 auto bits=canonical_float(std::bit_cast<std::uint64_t>(a));
 if(op==Op::FloatFromBits||op==Op::FloatToBits)return {std::bit_cast<std::int64_t>(bits)};
 if(op==Op::FloatIsNaN)return {(bits&UINT64_C(0x7fffffffffffffff))>UINT64_C(0x7ff0000000000000)};
 if(op==Op::FloatIsFinite)return {(bits&UINT64_C(0x7ff0000000000000))!=UINT64_C(0x7ff0000000000000)};
 if(op==Op::FloatSignbit)return {static_cast<std::int64_t>(bits>>63)};
 if(op==Op::FloatNeg)return {std::bit_cast<std::int64_t>(canonical_float(bits^UINT64_C(0x8000000000000000)))};
 if(op==Op::FloatFromI64){volatile std::int64_t input=a;volatile double result=static_cast<double>(input);return {encoded_float(result)};}
 volatile double x=decoded_float(a),y=decoded_float(b),result=0;
 switch(op){
 case Op::FloatAdd:result=x+y;break;case Op::FloatSub:result=x-y;break;
 case Op::FloatMul:result=x*y;break;case Op::FloatDiv:result=x/y;break;
 case Op::FloatEq:return {x==y};case Op::FloatNe:return {x!=y};
 case Op::FloatLt:return {x<y};case Op::FloatLe:return {x<=y};
 case Op::FloatGt:return {x>y};case Op::FloatGe:return {x>=y};
 case Op::FloatToI64:
  if((bits&UINT64_C(0x7ff0000000000000))==UINT64_C(0x7ff0000000000000)||x < -0x1p63||x >= 0x1p63)
   return {0,"f64 to i64 out of range"};
  return {static_cast<std::int64_t>(x)};
 default:throw std::logic_error("unmodeled f64 opcode");
 }
 return {encoded_float(result)};
}
inline std::string float_text(std::int64_t value){
 auto bits=canonical_float(std::bit_cast<std::uint64_t>(value));
 auto exponent=static_cast<unsigned>((bits>>52)&0x7ffu);auto fraction=bits&UINT64_C(0x000fffffffffffff);
 if(exponent==0x7ffu)return fraction?"nan":bits>>63?"-inf":"inf";
 std::string digits(13,'0');constexpr char hex[]="0123456789abcdef";
 for(unsigned i=0;i<13;++i)digits[12-i]=hex[(fraction>>(4*i))&15];
 int power=exponent?static_cast<int>(exponent)-1023:fraction?-1022:0;
 return std::string(bits>>63?"-":"")+(exponent?"0x1.":"0x0.")+digits+"p"+(power>=0?"+":"")+std::to_string(power);
}
}
#ifdef __clang__
#pragma float_control(pop)
#endif
