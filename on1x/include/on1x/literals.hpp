#pragma once
#include <on1x/ast.hpp>
#include <on1x/literal_scan.h>
#include <charconv>
#include <string_view>

namespace on1x::literals {
inline constexpr std::size_t max_format_parts=256;
inline bool formattable(const Type& type){
 return type.is_numeric()||type==Type::Bool||type==Type::String||type==Type::Char;
}
inline bool scalar(std::uint32_t cp){
 return cp<=0x10ffff && (cp<0xd800||cp>0xdfff);
}
inline std::uint32_t character(std::string_view text,const Location& at){
 auto fail=[&](const char* message)->std::uint32_t{throw Error(at,message,"ON1X-LITERAL-CHAR");};
 if(!text.starts_with("#\\")||text.size()<3||on1x_lit_character(text.data(),text.size())!=text.size())
  return fail("invalid character literal spelling");
 text.remove_prefix(2);
 if(text=="newline")return 10;
 if(text=="tab")return 9;
 if(text=="return")return 13;
 if(text=="space")return 32;
 if(text=="nul"||text=="null")return 0;
 if(text=="alarm")return 7;
 if(text=="backspace")return 8;
 if(text=="escape")return 27;
 if(text=="delete")return 127;
 if(text=="pound")return 0xa3;
 auto first=static_cast<unsigned char>(text.front());
 if(text.size()==1&&first<128)return first;
 std::uint32_t value=0;
 if(first<128){
  int base=16;
  const bool bmp=text.starts_with("u+");
  if(text.starts_with("u+")||text.starts_with("U+"))text.remove_prefix(2);
  else if(text.starts_with('x')||text.starts_with('X'))text.remove_prefix(1);
  else if(text.starts_with('o')||text.starts_with('O')){text.remove_prefix(1);base=8;}
  else return fail("unknown named character");
  auto [end,ec]=std::from_chars(text.data(),text.data()+text.size(),value,base);
  if(ec==std::errc::result_out_of_range)return fail("character is outside the Unicode scalar range");
  if(ec!=std::errc{}||end!=text.data()+text.size())return fail("invalid character code point digits");
  if(bmp&&value>0xffff)return fail("lowercase u+ character is outside its 16-bit range");
 }else{
  auto length=first>=0xf0&&first<=0xf4?4u:first>=0xe0&&first<=0xef?3u:first>=0xc2&&first<=0xdf?2u:0u;
  if(length!=text.size())return fail("character literal must contain one valid UTF-8 scalar");
  value=first&((1u<<(7-length))-1);
  for(std::size_t i=1;i<length;++i){
   auto next=static_cast<unsigned char>(text[i]);
   if((next&0xc0)!=0x80)return fail("invalid UTF-8 character");
   value=(value<<6)|(next&0x3f);
  }
  if(value<(length==2?0x80u:length==3?0x800u:0x10000u))return fail("invalid overlong UTF-8 character");
 }
 if(!scalar(value))return fail("character is outside the Unicode scalar range");
 return value;
}
inline unsigned radix(std::string_view s){
 if(s.starts_with("0b")||s.starts_with("0B"))return 2;
 if(s.starts_with("0o")||s.starts_with("0O"))return 8;
 return 0;
}
// These source spellings elaborate to existing PPX integers. Decimal/hex
// source and wire semantics, including macro literal classifications, stay put.
inline std::string radix_integer(std::string_view s,const Location& at,bool negative=false){
 auto base=radix(s);
 if(!base)return (negative?"-":"")+std::string(s);
 if(s.size()>1024)throw Error(at,"integer literal exceeds 1024-byte limit","ON1X-NUMERIC-LIMIT");
 s.remove_prefix(2);
 std::string_view suffix;
 unsigned width=64;
 auto u=s.find('u');
 if(u!=s.npos){
  suffix=s.substr(u);s=s.substr(0,u);
  width=suffix=="u8"?8:suffix=="u16"?16:suffix=="u32"?32:64;
 }
 auto maximum=suffix.empty()?(UINT64_MAX>>1)+(negative?1:0):width==64?UINT64_MAX:(UINT64_C(1)<<width)-1;
 std::uint64_t value=0;
 auto [end,ec]=std::from_chars(s.data(),s.data()+s.size(),value,static_cast<int>(base));
 if(negative&&!suffix.empty())throw Error(at,"unsigned literal cannot be negative","ON1X-NUMERIC-LITERAL");
 if(ec!=std::errc{}||end!=s.data()+s.size()||value>maximum)
  throw Error(at,"integer literal out of "+(suffix.empty()?std::string("i64"):std::string(suffix))+" range","ON1X-NUMERIC-RANGE");
 return (negative&&value?"-":"")+std::to_string(value)+std::string(suffix);
}
inline char escape(char c){
 switch(c){case 'n':return '\n';case 'r':return '\r';case 't':return '\t';case '0':return '\0';default:return c;}
}
inline bool byte_payload(std::string_view s){
 if(s.size()>2*max_source_bytes||s.size()%2)return false;
 for(unsigned char c:s)if(on1x_lit_hex(c)<0)return false;
 return true;
}
}
