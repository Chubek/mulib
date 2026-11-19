/* MuLIB graphical extension: immgtk-codeeditor. C11, header-only. */
#ifndef MULIB_IMMGTK_CODEEDITOR_H
#define MULIB_IMMGTK_CODEEDITOR_H
#include "mulib-immgtk-textedit.h"
typedef enum{MIG_CODE_WORD,MIG_CODE_NUMBER,MIG_CODE_STRING,MIG_CODE_COMMENT,MIG_CODE_PUNCT,MIG_CODE_SPACE}mig_code_token;
static inline size_t mulib_immgtk_codeeditor_token(const char *s,size_t n,mig_code_token *kind){if(!n)return 0;size_t i=1;unsigned char c=s[0];if(c==' '||c=='\t'||c=='\n'){*kind=MIG_CODE_SPACE;while(i<n&&(s[i]==' '||s[i]=='\t'||s[i]=='\n'))i++;}else if(c=='/'&&n>1&&s[1]=='/'){*kind=MIG_CODE_COMMENT;while(i<n&&s[i]!='\n')i++;}else if(c=='"'||c=='\''){*kind=MIG_CODE_STRING;while(i<n){if(s[i]=='\\'&&i+1<n){i+=2;continue;}if(s[i++]==(char)c)break;}}else if(c>='0'&&c<='9'){*kind=MIG_CODE_NUMBER;while(i<n&&((s[i]>='0'&&s[i]<='9')||s[i]=='.'))i++;}else if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'){*kind=MIG_CODE_WORD;while(i<n&&((s[i]>='A'&&s[i]<='Z')||(s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')||s[i]=='_'))i++;}else *kind=MIG_CODE_PUNCT;return i;}
static inline int mulib_immgtk_codeeditor(mulib_immgtk_native_t *g,mulib_immgtk_io_t *io,const char *id,mgx_rect r,mulib_texteditor_t *e){return mulib_immgtk_textedit(g,io,id,r,e);}
#endif
