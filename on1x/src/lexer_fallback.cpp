#include <on1x/generated_scan.h>
#include <cstring>
#include <string>
namespace {
bool alpha(unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z');}
bool digit(unsigned char c){return c>='0'&&c<='9';}
}
extern "C" int on1x_generated_scan(const char* source,size_t n,on1x_token_callback cb,void* ctx,size_t*error){
 size_t pos=0;
 while(pos<n){
  unsigned char c=(unsigned char)source[pos];
   if(c==' '||c=='\t'||c=='\r'||c=='\n'){++pos;continue;}
  if(c=='#'){while(pos<n&&source[pos]!='\n')++pos;continue;}
  size_t start=pos;
   if(alpha(c)||c=='_'){++pos;while(pos<n&&(alpha((unsigned char)source[pos])||digit((unsigned char)source[pos])||source[pos]=='_'))++pos;}
   else if(digit(c)){++pos;while(pos<n&&digit((unsigned char)source[pos]))++pos;}
  else if(c=='"'){
   ++pos;bool done=false;
   while(pos<n){if(source[pos]=='"'){++pos;done=true;break;}
    if(source[pos]=='\\'){++pos;if(pos==n)break;if(!std::strchr("\\\"nrt0",source[pos]))break;}
    else if(source[pos]=='\n'||source[pos]=='\r')break;
    ++pos;
   }
   if(!done){if(error)*error=start;return 1;}
  }
  else if(pos+1<n && (std::strncmp(source+pos,"==",2)==0||std::strncmp(source+pos,"!=",2)==0||std::strncmp(source+pos,"<=",2)==0||std::strncmp(source+pos,">=",2)==0||std::strncmp(source+pos,"&&",2)==0||std::strncmp(source+pos,"||",2)==0||std::strncmp(source+pos,"->",2)==0))pos+=2;
  else if(std::strchr("+*-/%<>=!;:,(){}[]",c))++pos;
  else {if(error)*error=start;return 1;}
  std::string token(source+start,pos-start);cb(ctx,token.c_str(),start);
 }
 return 0;
}
