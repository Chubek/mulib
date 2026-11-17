#include <on1x/numeric.hpp>
#include <bit>
#include <charconv>
#include <limits>

namespace on1x {
bool float_spelling(std::string_view text){
 if(text.empty()||text.size()>1024)return false;
 std::size_t pos=0;
 auto digits=[&]{auto start=pos;while(pos<text.size()&&text[pos]>='0'&&text[pos]<='9')++pos;return pos!=start;};
 if(!digits())return false;
 bool fraction=false,exponent=false;
 if(pos<text.size()&&text[pos]=='.'){fraction=true;++pos;if(!digits())return false;}
 if(pos<text.size()&&(text[pos]=='e'||text[pos]=='E')){
  exponent=true;++pos;if(pos<text.size()&&(text[pos]=='+'||text[pos]=='-'))++pos;
  if(!digits())return false;
 }
 return pos==text.size()&&(fraction||exponent);
}
std::int64_t float_literal_bits(std::string_view text,const Location& at){
 static_assert(sizeof(double)==sizeof(std::int64_t)&&std::numeric_limits<double>::is_iec559&&std::numeric_limits<double>::digits==53);
 if(text.size()>1024)throw Error(at,"float literal exceeds 1024-byte limit","ON1X-NUMERIC-LIMIT");
 if(!float_spelling(text))throw Error(at,"invalid decimal f64 literal","ON1X-NUMERIC-LITERAL");
 double value=0;
 auto [end,ec]=std::from_chars(text.data(),text.data()+text.size(),value,std::chars_format::general);
 if(ec!=std::errc{}||end!=text.data()+text.size())
  throw Error(at,"f64 literal overflows or underflows representable range","ON1X-NUMERIC-RANGE");
 return std::bit_cast<std::int64_t>(value);
}
}
