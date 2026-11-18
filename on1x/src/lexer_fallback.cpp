#include <on1x/generated_scan.h>
#include <on1x/literal_scan.h>
#include <cstring>
#include <string>
namespace {
bool alpha(unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z');}
bool digit(unsigned char c){return c>='0'&&c<='9';}
bool hex(unsigned char c){return digit(c)||(c>='a'&&c<='f')||(c>='A'&&c<='F');}
}
extern "C" int on1x_generated_scan(const char* source,size_t n,on1x_token_callback cb,void* ctx,size_t*error){
 size_t pos=0;
 while(pos<n){
  unsigned char c=(unsigned char)source[pos];
   if(c==' '||c=='\t'||c=='\r'||c=='\n'){++pos;continue;}
   if(c=='#'&&(pos+1==n||source[pos+1]!='\\')){while(pos<n&&source[pos]!='\n')++pos;continue;}
   size_t start=pos;
   if(c=='#'||(c=='b'&&n-pos>=4&&std::memcmp(source+pos,"b!#\\",4)==0)){
    size_t prefix=c=='b'?2:0,len=on1x_lit_character(source+pos+prefix,n-pos-prefix);
    if(!len){if(error)*error=start;return 1;}
    pos+=prefix+len;
   }else if(c=='"'||((c=='r'||c=='b')&&n-pos>=3&&source[pos+1]=='!'&&source[pos+2]=='"')){
    size_t len=on1x_lit_string(source+pos,n-pos,0);
    if(!len){if(error)*error=start;return 1;}
    pos+=len;
   }
    else if(alpha(c)||c=='_'){++pos;while(pos<n&&(alpha((unsigned char)source[pos])||digit((unsigned char)source[pos])||source[pos]=='_'))++pos;}
     else if(digit(c)){
      int radix=0;
      if(c=='0'&&pos+2<n){
       char kind=source[pos+1],value=source[pos+2];
       if((kind=='x'||kind=='X')&&hex(static_cast<unsigned char>(value)))radix=16;
       else if((kind=='b'||kind=='B')&&value>='0'&&value<='1')radix=2;
       else if((kind=='o'||kind=='O')&&value>='0'&&value<='7')radix=8;
      }
      if(radix){
       pos+=3;while(pos<n){int value=on1x_lit_hex(static_cast<unsigned char>(source[pos]));if(value<0||value>=radix)break;++pos;}
     }else{
      ++pos;while(pos<n&&digit((unsigned char)source[pos]))++pos;
      if(pos+1<n&&source[pos]=='.'&&digit((unsigned char)source[pos+1])){
       pos+=2;while(pos<n&&digit((unsigned char)source[pos]))++pos;
      }
      if(pos<n&&(source[pos]=='e'||source[pos]=='E')){
       auto end=pos+1;if(end<n&&(source[end]=='+'||source[end]=='-'))++end;
       if(end<n&&digit((unsigned char)source[end])){
        pos=end+1;while(pos<n&&digit((unsigned char)source[pos]))++pos;
       }
      }
      }
       bool floating=!radix&&std::string_view(source+start,pos-start).find_first_of(".eE")!=std::string_view::npos;
      for(const char* suffix:{"u8","u16","u32","u64","f32","f64"}){
        if((radix&&suffix[0]=='f')||(floating&&suffix[0]=='u'))continue;
       auto length=std::strlen(suffix);
       if(pos+length<=n&&std::strncmp(source+pos,suffix,length)==0){pos+=length;break;}
      }
     }
   else if(c=='`'){
    bool triple=pos+2<n&&source[pos+1]=='`'&&source[pos+2]=='`',done=false;
    pos+=triple?3:1;
    while(pos<n){
     if(triple){
      if(pos+2<n&&source[pos]=='`'&&source[pos+1]=='`'&&source[pos+2]=='`'){pos+=3;done=true;break;}
      ++pos;
     }else{
      if(source[pos]=='`'){++pos;done=true;break;}
      if(source[pos]=='\n'||source[pos]=='\r')break;
      if(source[pos]=='\\'){++pos;if(pos==n||source[pos]=='\n'||source[pos]=='\r')break;}
      ++pos;
     }
    }
    if(!done){if(error)*error=start;return 1;}
   }
   else if(pos+2<n&&(std::strncmp(source+pos,"...",3)==0||std::strncmp(source+pos,"..=",3)==0))pos+=3;
   else if(pos+1<n&&std::strncmp(source+pos,"..",2)==0)pos+=2;
   else if(pos+1<n && (std::strncmp(source+pos,"==",2)==0||std::strncmp(source+pos,"!=",2)==0||std::strncmp(source+pos,"<=",2)==0||std::strncmp(source+pos,">=",2)==0||std::strncmp(source+pos,"&&",2)==0||std::strncmp(source+pos,"||",2)==0||std::strncmp(source+pos,"->",2)==0||std::strncmp(source+pos,"=>",2)==0))pos+=2;
    else if(pos+1<n&&(std::strncmp(source+pos,"::",2)==0||std::strncmp(source+pos,":=",2)==0||std::strncmp(source+pos,"<-",2)==0))pos+=2;
     else if(std::strchr("+*-/%<>=!;:,(){}[]@$.|?~",c))++pos;
  else {if(error)*error=start;return 1;}
  std::string token(source+start,pos-start);cb(ctx,token.c_str(),start);
 }
 return 0;
}
