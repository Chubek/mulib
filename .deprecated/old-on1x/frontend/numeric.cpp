#include <on1x/numeric.hpp>
#include <bit>
#include <charconv>
#include <limits>

namespace on1x {
Type numeric_literal_type(std::string_view text){
 bool hex=text.starts_with("0x")||text.starts_with("0X");
 for(auto kind:{Type::U8,Type::U16,Type::U32,Type::U64})if(text.ends_with(type_name(kind)))return kind;
 if(!hex&&text.ends_with("f32"))return Type::F32;
 if(!hex&&text.ends_with("f64"))return Type::F64;
 return !hex&&text.find_first_of(".eE")!=std::string_view::npos?Type::F64:Type::I64;
}
bool unsigned_spelling(std::string_view text){
 auto type=numeric_literal_type(text);if(!type.is_unsigned()||text.size()>1024)return false;
 text.remove_suffix(type_name(type).size());
 bool hex=text.starts_with("0x")||text.starts_with("0X");if(hex)text.remove_prefix(2);
 if(text.empty())return false;
 for(char c:text)if(!(c>='0'&&c<='9')&&!(hex&&((c>='a'&&c<='f')||(c>='A'&&c<='F'))))return false;
 return true;
}
std::int64_t integer_literal_bits(std::string_view text,const Location& at){
 auto type=numeric_literal_type(text);
 if(type.is_unsigned()){
  if(text.size()>1024)throw Error(at,"unsigned literal exceeds 1024-byte limit","ON1X-NUMERIC-LIMIT");
  if(!unsigned_spelling(text))throw Error(at,"invalid unsigned literal spelling","ON1X-NUMERIC-LITERAL");
  text.remove_suffix(type_name(type).size());bool hex=text.starts_with("0x")||text.starts_with("0X");if(hex)text.remove_prefix(2);
  std::uint64_t value=0;auto [end,ec]=std::from_chars(text.data(),text.data()+text.size(),value,hex?16:10);
  unsigned width=type==Type::U8?8:type==Type::U16?16:type==Type::U32?32:64;
  auto maximum=width==64?UINT64_MAX:(UINT64_C(1)<<width)-1;
  if(ec!=std::errc{}||end!=text.data()+text.size()||value>maximum)throw Error(at,"unsigned literal out of "+type_name(type)+" range","ON1X-NUMERIC-RANGE");
  return std::bit_cast<std::int64_t>(value);
 }
 std::int64_t value=0;bool hex=text.starts_with("0x")||text.starts_with("0X");if(hex)text.remove_prefix(2);
 auto [end,ec]=std::from_chars(text.data(),text.data()+text.size(),value,hex?16:10);
 if(type!=Type::I64||ec!=std::errc{}||end!=text.data()+text.size())throw Error(at,"integer literal out of i64 range","ON1X-NUMERIC-RANGE");
 return value;
}
bool float_spelling(std::string_view text){
 if(text.empty()||text.size()>1024)return false;
 bool suffix=text.ends_with("f32")||text.ends_with("f64");if(suffix)text.remove_suffix(3);
 std::size_t pos=0;
 auto digits=[&]{auto start=pos;while(pos<text.size()&&text[pos]>='0'&&text[pos]<='9')++pos;return pos!=start;};
 if(!digits())return false;
 bool fraction=false,exponent=false;
 if(pos<text.size()&&text[pos]=='.'){fraction=true;++pos;if(!digits())return false;}
 if(pos<text.size()&&(text[pos]=='e'||text[pos]=='E')){
  exponent=true;++pos;if(pos<text.size()&&(text[pos]=='+'||text[pos]=='-'))++pos;
  if(!digits())return false;
 }
 return pos==text.size()&&(fraction||exponent||suffix);
}
std::int64_t float_literal_bits(std::string_view text,const Location& at){
 static_assert(sizeof(double)==sizeof(std::int64_t)&&std::numeric_limits<double>::is_iec559&&std::numeric_limits<double>::digits==53);
 if(text.size()>1024)throw Error(at,"float literal exceeds 1024-byte limit","ON1X-NUMERIC-LIMIT");
  if(!float_spelling(text))throw Error(at,"invalid decimal f64 literal","ON1X-NUMERIC-LITERAL");
  auto type=numeric_literal_type(text);
  if(text.ends_with("f32")||text.ends_with("f64"))text.remove_suffix(3);
  if(type==Type::F32){
   static_assert(sizeof(float)==sizeof(std::uint32_t)&&std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
   float value=0;auto [end,ec]=std::from_chars(text.data(),text.data()+text.size(),value,std::chars_format::general);
   if(ec!=std::errc{}||end!=text.data()+text.size())throw Error(at,"f32 literal overflows or underflows representable range","ON1X-NUMERIC-RANGE");
   return static_cast<std::int64_t>(std::bit_cast<std::uint32_t>(value));
  }
 double value=0;
 auto [end,ec]=std::from_chars(text.data(),text.data()+text.size(),value,std::chars_format::general);
 if(ec!=std::errc{}||end!=text.data()+text.size())
  throw Error(at,"f64 literal overflows or underflows representable range","ON1X-NUMERIC-RANGE");
 return std::bit_cast<std::int64_t>(value);
}
}
