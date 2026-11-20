/** @file mulib-color-cube.h
 * Read standard unit-domain 3D .cube LUTs.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No combined 1D/3D or arbitrary domains; partial scratch on failure.
 */
#ifndef MULIB_COLOR_CUBE_H
#define MULIB_COLOR_CUBE_H
#include "mulib-img-core.h"
#include "mulib-color-lut3d.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline bool mulib_color_cube_space(char c){return c==' '||c=='\t'||c=='\r'||c=='\v'||c=='\f';}
static inline const char *mulib_color_cube_skip(const char *p){while(mulib_color_cube_space(*p))p++;return p;}
static inline bool mulib_color_cube_end(const char *p){p=mulib_color_cube_skip(p);return !*p||*p=='#';}
/* Internal primitive: cursor/out must be nonnull, *cursor must point into an
 * accessible NUL-terminated line of at most 511 bytes, as read below.
 * Locale-independent decimal parser, no scanf numeric-overflow UB.
 * Keep 19 significant digits; excessively small values round to zero. */
static inline bool mulib_color_cube_number(const char **cursor,float *out){
    const char *p=mulib_color_cube_skip(*cursor);
    int sign=1,exponent=0,explicit_exp=0,exp_sign=1;
    unsigned digits=0,kept=0;
    bool fraction=false;
    long double mantissa=0;
    if(*p=='+'||*p=='-'){if(*p=='-')sign=-1;p++;}
    for(;;p++){
        if(*p>='0'&&*p<='9'){
            unsigned d=(unsigned)(*p-'0');
            digits++;
            if(!kept&&!d){if(fraction)exponent--;}
            else if(kept<19){mantissa=mantissa*10+d;kept++;if(fraction)exponent--;}
            else if(!fraction)exponent++;
        }else if(*p=='.'&&!fraction)fraction=true;
        else break;
    }
    if(!digits)return false;
    if(*p=='e'||*p=='E'){
        p++;
        if(*p=='+'||*p=='-'){if(*p=='-')exp_sign=-1;p++;}
        if(*p<'0'||*p>'9')return false;
        while(*p>='0'&&*p<='9'){
            if(explicit_exp>1000)return false;
            explicit_exp=explicit_exp*10+(*p++-'0');
        }
    }
    if(*p&&!mulib_color_cube_space(*p)&&*p!='#')return false;
    long double value=mantissa?sign*mantissa*powl(10.0L,exponent+exp_sign*explicit_exp):0;
    if(!isfinite(value)||fabsl(value)>FLT_MAX)return false;
    *out=(float)value;
    *cursor=p;
    return true;
}
/* FILE starts at beginning. cap is accessible floats, line_budget bounds total
 * input work, each line <=511 bytes (excluding LF). Unit-domain 3D only.
 * Reject NUL, trailing junk, duplicate/late headers, nonfinite/overflow data.
 * rgb is scratch and may be partially written on failure; *out stays unchanged.
 * Both must be disjoint. Stream state is consumed even on failure. */
static inline bool mulib_color_cube_read(FILE *f,float *rgb,size_t cap,size_t line_budget,mulib_color_lut3d *out){
    size_t bytes,n=0,edge=0,required=0,lines=0;
    bool min_seen=false,max_seen=false,title_seen=false;
    if(!f||!rgb||!out||!mulib_img_memory_span(rgb,cap,sizeof *rgb,&bytes)||
       !mulib_img_memory_disjoint(rgb,cap,sizeof *rgb,out,1,sizeof *out))return false;
    for(;;){
        char line[512];
        size_t len=0;
        int ch=fgetc(f);
        if(ch==EOF)break;
        if(lines==line_budget)return false;
        lines++;
        while(ch!=EOF&&ch!='\n'){
            if(!ch||len==sizeof line-1)return false;
            line[len++]=(char)ch;
            ch=fgetc(f);
        }
        line[len]=0;
        const char *p=mulib_color_cube_skip(line);
        if(mulib_color_cube_end(p))continue;
        if(!strncmp(p,"TITLE",5)&&mulib_color_cube_space(p[5])){
            if(title_seen||n)return false;
            title_seen=true;
            p=mulib_color_cube_skip(p+5);
            if(*p++!='"')return false;
            while(*p&&*p!='"')p++;
            if(*p!='"'||!mulib_color_cube_end(p+1))return false;
            continue;
        }
        if(!strncmp(p,"LUT_3D_SIZE",11)&&mulib_color_cube_space(p[11])){
            if(edge||n)return false;
            p=mulib_color_cube_skip(p+11);
            unsigned e=0;
            if(*p<'0'||*p>'9')return false;
            while(*p>='0'&&*p<='9'){
                e=e*10+(unsigned)(*p++-'0');
                if(e>256)return false;
            }
            if(e<2||!mulib_color_cube_end(p)||
               !mulib_media_size_mul(e,e,&required)||
               !mulib_media_size_mul(required,e,&required)||
               !mulib_media_size_mul(required,3,&required)||required>cap)return false;
            edge=e;
            continue;
        }
        bool is_min=!strncmp(p,"DOMAIN_MIN",10)&&mulib_color_cube_space(p[10]);
        bool is_max=!strncmp(p,"DOMAIN_MAX",10)&&mulib_color_cube_space(p[10]);
        float a,b,c;
        if(is_min||is_max){
            if(n||(is_min?min_seen:max_seen))return false;
            p+=10;
            if(!mulib_color_cube_number(&p,&a)||!mulib_color_cube_number(&p,&b)||
               !mulib_color_cube_number(&p,&c)||!mulib_color_cube_end(p))return false;
            if(is_min){if(a!=0||b!=0||c!=0)return false;min_seen=true;}
            else{if(a!=1||b!=1||c!=1)return false;max_seen=true;}
            continue;
        }
        if(!edge||n>=required||
           !mulib_color_cube_number(&p,&a)||!mulib_color_cube_number(&p,&b)||
           !mulib_color_cube_number(&p,&c)||!mulib_color_cube_end(p))return false;
        rgb[n++]=a;rgb[n++]=b;rgb[n++]=c;
    }
    if(ferror(f)||!edge||n!=required)return false;
    mulib_color_lut3d result={rgb,edge,n};
    *out=result;
    return true;
}
#ifdef __cplusplus
}
#endif
#endif
