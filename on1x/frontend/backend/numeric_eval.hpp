#pragma once
#include "numeric.hpp"
#include "floating.hpp"
#include <cmath>
#include <limits>
#ifdef __clang__
#pragma float_control(precise, on, push)
#endif

namespace on1x::mir::detail {
inline std::uint32_t canonical_single(std::uint32_t bits){
 return (bits&UINT32_C(0x7fffffff))>UINT32_C(0x7f800000)?UINT32_C(0x7fc00000):bits;
}
inline float single_value(std::int64_t bits){return std::bit_cast<float>(static_cast<std::uint32_t>(bits));}
inline std::int64_t single_bits(float value){return canonical_single(std::bit_cast<std::uint32_t>(value));}
inline std::int64_t word_bits(std::uint64_t value){return std::bit_cast<std::int64_t>(value);}
inline FloatResult numeric(const std::string& name,std::int64_t a,std::int64_t b,const Location& at){
 auto signature=numeric_signature(name);
 if(!signature)throw Error(at,"ON1X-MIR-UNSUPPORTED: unknown scalar numeric descriptor");
 auto input=signature->parameters()[0],output=signature->result();
 auto u=std::bit_cast<std::uint64_t>(a),v=std::bit_cast<std::uint64_t>(b);
 if(name.ends_with("_from_bits")){
  if(output.is_unsigned())return {word_bits(u&unsigned_max(output))};
  if(output==Type::F32){
   if(a<0||u>UINT32_MAX)return {0,"f32 bit pattern out of range"};
   return {canonical_single(static_cast<std::uint32_t>(u))};
  }
  return {word_bits(canonical_float(u))};
 }
 if(name.ends_with("_to_bits")){
  if(input.is_unsigned())return {a};
  return {input==Type::F32?static_cast<std::int64_t>(canonical_single(static_cast<std::uint32_t>(u))):word_bits(canonical_float(u))};
 }
 if(name.ends_with("_is_nan")||name.ends_with("_is_finite")||name.ends_with("_signbit")){
  bool single=input==Type::F32;
  auto bits=single?static_cast<std::uint64_t>(canonical_single(static_cast<std::uint32_t>(u))):canonical_float(u);
  auto magnitude=bits&(single?UINT64_C(0x7fffffff):UINT64_C(0x7fffffffffffffff));
  auto infinity=single?UINT64_C(0x7f800000):UINT64_C(0x7ff0000000000000);
  return {name.ends_with("_is_nan")?magnitude>infinity:name.ends_with("_is_finite")?magnitude<infinity:bool(bits>>(single?31:63))};
 }
 bool floating=input.is_float()||output.is_float();
 if(floating)if(auto error=float_environment(at))return {0,error};
 if(name.find("_from_")!=std::string::npos||name.ends_with("_to_i64")){
  if(output.is_unsigned()){
   if(input.is_float()){
    double x=input==Type::F32?static_cast<double>(single_value(a)):decoded_float(a);
    double limit=std::ldexp(1.0,static_cast<int>(unsigned_width(output)));
    if(!std::isfinite(x)||x<0||x>=limit)return {0,"float to unsigned out of range"};
    return {word_bits(static_cast<std::uint64_t>(x))};
   }
   if(input==Type::I64&&a<0)return {0,"negative unsigned conversion"};
   if(u>unsigned_max(output))return {0,"unsigned conversion out of range"};
   return {a};
  }
  if(output==Type::I64){
   if(input.is_unsigned())return u>INT64_MAX?FloatResult{0,"unsigned to i64 out of range"}:FloatResult{a};
   double x=input==Type::F32?static_cast<double>(single_value(a)):decoded_float(a);
   if(!std::isfinite(x)||x < -0x1p63||x >= 0x1p63)return {0,input==Type::F32?"f32 to i64 out of range":"f64 to i64 out of range"};
   return {static_cast<std::int64_t>(x)};
  }
  if(output==Type::F32){
   volatile float result=0;
   if(input.is_unsigned()){volatile std::uint64_t x=u;result=static_cast<float>(x);}
   else if(input==Type::I64){volatile std::int64_t x=a;result=static_cast<float>(x);}
   else{
    volatile double x=decoded_float(a);
    if(std::isnan(x))return {UINT32_C(0x7fc00000)};
    if(x>std::numeric_limits<float>::max()||x < -std::numeric_limits<float>::max()){
     double absolute=x<0?-x:x;
     return {static_cast<std::int64_t>((x<0?UINT32_C(0x80000000):0)|(absolute<0x1.ffffffp127?UINT32_C(0x7f7fffff):UINT32_C(0x7f800000)))};
    }
    result=static_cast<float>(x);
   }
   return {single_bits(result)};
  }
  if(output==Type::F64){
   volatile double result=0;
   if(input.is_unsigned()){volatile std::uint64_t x=u;result=static_cast<double>(x);}
   else if(input==Type::I64){volatile std::int64_t x=a;result=static_cast<double>(x);}
   else{volatile float x=single_value(static_cast<std::int64_t>(canonical_single(static_cast<std::uint32_t>(u))));result=static_cast<double>(x);}
   return {encoded_float(result)};
  }
 }
 auto separator=name.find('_');auto op=name.substr(0,separator);
 if(input.is_unsigned()){
  auto maximum=unsigned_max(input);auto rest=name.substr(separator+1);
  if(op==type_name(input))op=rest;
  if(op=="add"){if(u>maximum-v)return {0,"unsigned addition overflow"};return {word_bits(u+v)};}
  if(op=="sub"){if(u<v)return {0,"unsigned subtraction underflow"};return {word_bits(u-v)};}
  if(op=="mul"){if(v&&u>maximum/v)return {0,"unsigned multiplication overflow"};return {word_bits(u*v)};}
  if(op=="div"||op=="mod"){if(!v)return {0,op=="div"?"unsigned division by zero":"unsigned modulo by zero"};return {word_bits(op=="div"?u/v:u%v)};}
  if(op=="eq")return {u==v};
  if(op=="ne")return {u!=v};
  if(op=="lt")return {u<v};
  if(op=="le")return {u<=v};
  if(op=="gt")return {u>v};
  if(op=="ge")return {u>=v};
  if(op=="wrapping_add")return {word_bits((u+v)&maximum)};
  if(op=="wrapping_sub")return {word_bits((u-v)&maximum)};
  if(op=="wrapping_mul")return {word_bits((u*v)&maximum)};
  if(op=="and")return {word_bits(u&v)};
  if(op=="or")return {word_bits(u|v)};
  if(op=="xor")return {word_bits(u^v)};
  if(op=="not")return {word_bits((~u)&maximum)};
  if(op=="shl"||op=="shr"){
   if(b<0||static_cast<std::uint64_t>(b)>=unsigned_width(input))return {0,"unsigned shift count out of range"};
   auto count=static_cast<unsigned>(b);
   if(op=="shl"&&u>(maximum>>count))return {0,"unsigned left shift overflow"};
   return {word_bits(op=="shl"?u<<count:u>>count)};
  }
 }
 if(input==Type::F32){
  if(op=="neg")return {canonical_single(canonical_single(static_cast<std::uint32_t>(u))^UINT32_C(0x80000000))};
  volatile float x=single_value(a),y=single_value(b),result=0;
  if(op=="add")result=x+y;else if(op=="sub")result=x-y;else if(op=="mul")result=x*y;else if(op=="div")result=x/y;
  else if(op=="eq")return {x==y};else if(op=="ne")return {x!=y};
  else if(op=="lt")return {x<y};else if(op=="le")return {x<=y};else if(op=="gt")return {x>y};else if(op=="ge")return {x>=y};
  else throw Error(at,"ON1X-MIR-UNSUPPORTED: unmodeled f32 operation");
  return {single_bits(result)};
 }
 throw Error(at,"ON1X-MIR-UNSUPPORTED: unmodeled scalar numeric descriptor");
}
inline std::string single_text(std::int64_t value){
 auto bits=canonical_single(static_cast<std::uint32_t>(value));auto exponent=(bits>>23)&0xffu,fraction=bits&UINT32_C(0x7fffff);
 if(exponent==0xffu)return fraction?"nan":bits>>31?"-inf":"inf";
 constexpr char hex[]="0123456789abcdef";std::string digits(6,'0');
 for(unsigned i=0;i<6;++i)digits[5-i]=hex[((fraction<<1)>>(4*i))&15];
 int power=exponent?static_cast<int>(exponent)-127:fraction?-126:0;
 return std::string(bits>>31?"-":"")+(exponent?"0x1.":"0x0.")+digits+"p"+(power>=0?"+":"")+std::to_string(power);
}
}
#ifdef __clang__
#pragma float_control(pop)
#endif
