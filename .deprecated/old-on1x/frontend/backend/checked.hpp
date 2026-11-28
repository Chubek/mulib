#pragma once
#include <on1x/backend/mir.hpp>
#include <limits>

namespace on1x::mir::detail {
struct Checked { std::int64_t value=0; const char* trap=nullptr; };
// Shared by reference evaluation and folding; never executes overflowing host
// arithmetic. The independent C11 runtime remains the execution oracle.
inline Checked arithmetic(Op op,std::int64_t a,std::int64_t b){
 constexpr auto lo=std::numeric_limits<std::int64_t>::min(),hi=std::numeric_limits<std::int64_t>::max();
 switch(op){
 case Op::Add:if((b>0&&a>hi-b)||(b<0&&a<lo-b))return {0,"addition overflow"};return {a+b};
 case Op::Sub:if((b<0&&a>hi+b)||(b>0&&a<lo+b))return {0,"subtraction overflow"};return {a-b};
 case Op::Mul:{
  auto magnitude=[](std::int64_t n){return n<0?std::uint64_t(-(n+1))+1:std::uint64_t(n);};
  auto x=magnitude(a),y=magnitude(b);bool negative=(a<0)!=(b<0);
  auto bound=std::uint64_t(hi)+(negative?1:0);
  if(y&&x>bound/y)return {0,"multiplication overflow"};
  auto product=x*y;if(negative&&product==std::uint64_t(hi)+1)return {lo};
  return {negative?-static_cast<std::int64_t>(product):static_cast<std::int64_t>(product)};
 }
 case Op::Div:if(!b||(a==lo&&b==-1))return {0,"invalid division"};return {a/b};
 case Op::Mod:if(!b||(a==lo&&b==-1))return {0,"invalid modulo"};return {a%b};
 case Op::Neg:if(a==lo)return {0,"negation overflow"};return {-a};
 default:throw std::logic_error("invalid checked arithmetic opcode");
 }
}
}
