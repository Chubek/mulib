/* MuLIB / Amb3r: fpga-fasm. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_FASM in exactly one implementation translation unit.
 * Profile: Scalar feature parsing and writing; not full FASM grammar.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_FASM_H
#define MULIB_FPGA_FASM_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
static inline int ma_fasm_name(const char *s){if(!s||!*s)return 0;for(;*s;s++)if(!(isalnum((unsigned char)*s)||*s=='_'||*s=='.'))return 0;return 1;}
int mulib_fpga_fasm_write(FILE *f,const char *feature,unsigned enabled);
int mulib_fpga_fasm_parse(const char *line,char *feature,size_t cap,unsigned *enabled);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FPGA_FASM) && !defined(MULIB_FPGA_FASM_H_IMPLEMENTED)
#define MULIB_FPGA_FASM_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_fasm_write(FILE *f,const char *feature,unsigned enabled){if(!f||!ma_fasm_name(feature)||enabled>1)return 0;return fprintf(f,"%s = %u\n",feature,enabled)>0;}
int mulib_fpga_fasm_parse(const char *line,char *feature,size_t cap,unsigned *enabled){if(!line||!feature||!cap||!enabled)return 0;while(isspace((unsigned char)*line))line++;size_t n=0;while(*line&&!isspace((unsigned char)*line)&&*line!='='&&*line!='#'){if(n+1>=cap)return 0;feature[n++]=*line++;}feature[n]=0;if(!ma_fasm_name(feature))return 0;while(isspace((unsigned char)*line))line++;unsigned v=1;if(*line=='='){line++;while(isspace((unsigned char)*line))line++;if(*line!='0'&&*line!='1')return 0;v=(unsigned)(*line++-'0');while(isspace((unsigned char)*line))line++;}if(*line&&*line!='#')return 0;*enabled=v;return 1;}
#ifdef __cplusplus
}
#endif
#endif
