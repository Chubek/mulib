/** @file mulib-texruntime.h
 * @brief Numeric expression language with named recursive functions and conditional calls.
 * Define MULIB_IMPL_TEXRUNTIME in one translation unit. C11, no third-party libraries.
 * Grammar: { fn name(param) = expression ; } [eval] expression [;]
 * Expression: finite numbers, unary +/-; +,-,*,/; comparisons; ternary; function calls.
 * Deliberately NOT a full TeX interpreter, document compiler, or general scripting VM.
 */
#ifndef MULIB_TEXRUNTIME_H
#define MULIB_TEXRUNTIME_H
#include "mulib-typecore.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Evaluate a program under bounded recursion/instruction counts. Returns 1 on success.
 * On failure, returns 0 and preserves *out. max_steps==0 uses 100000.
 * max_depth==0 uses 64; max_depth must be <=256. */
int mulib_texruntime_eval(const char *program,size_t max_steps,unsigned max_depth,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TEXRUNTIME) && !defined(MULIB_TEXRUNTIME_IMPLEMENTED)
#define MULIB_TEXRUNTIME_IMPLEMENTED
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_texruntime_fn_ {
    char name[64],param[64];
    const char *begin,*end;
} mulib_texruntime_fn_;
typedef struct mulib_texruntime_ctx_ {
    mulib_texruntime_fn_ functions[64];
    size_t count,steps,max_steps;
    unsigned max_depth,expression_depth;
} mulib_texruntime_ctx_;
typedef struct mulib_texruntime_parser_ {
    const char *p,*end;
    const char *param;
    double arg;
    mulib_texruntime_ctx_ *ctx;
    unsigned depth;
    int ok;
} mulib_texruntime_parser_;
static void mulib_texruntime_ws_(mulib_texruntime_parser_ *p){
    while(p->p<p->end&&isspace((unsigned char)*p->p))p->p++;
}
static int mulib_texruntime_tick_(mulib_texruntime_parser_ *p){
    if(p->ctx->steps>=p->ctx->max_steps){p->ok=0;return 0;}
    p->ctx->steps++;return 1;
}
static int mulib_texruntime_identifier_(mulib_texruntime_parser_ *p,char out[64]){
    mulib_texruntime_ws_(p);
    if(p->p>=p->end||!(isalpha((unsigned char)*p->p)||*p->p=='_'))return 0;
    size_t n=0;
    while(p->p<p->end&&(isalnum((unsigned char)*p->p)||*p->p=='_')){
        if(n>=63){p->ok=0;return 0;}
        out[n++]=*p->p++;
    }
    out[n]=0;return 1;
}
static int mulib_texruntime_char_(mulib_texruntime_parser_ *p,char c){
    mulib_texruntime_ws_(p);
    if(p->p>=p->end||*p->p!=c)return 0;
    p->p++;return 1;
}
static double mulib_texruntime_expr_(mulib_texruntime_parser_ *p,int evaluate);
static double mulib_texruntime_call_(mulib_texruntime_parser_ *p,const char *name,double arg,int evaluate){
    if(!evaluate)return 0;
    for(size_t i=0;i<p->ctx->count;i++){
        const mulib_texruntime_fn_ *f=&p->ctx->functions[i];
        if(strcmp(f->name,name))continue;
        if(p->depth>=p->ctx->max_depth){p->ok=0;return 0;}
        mulib_texruntime_parser_ body={f->begin,f->end,f->param,arg,p->ctx,p->depth+1,1};
        double result=mulib_texruntime_expr_(&body,1);
        mulib_texruntime_ws_(&body);
        if(!body.ok||body.p!=body.end){p->ok=0;return 0;}
        return result;
    }
    p->ok=0;return 0;
}
static double mulib_texruntime_primary_(mulib_texruntime_parser_ *p,int evaluate){
    if(!p->ok||!mulib_texruntime_tick_(p))return 0;
    mulib_texruntime_ws_(p);
    if(mulib_texruntime_char_(p,'(')){
        double v=mulib_texruntime_expr_(p,evaluate);
        if(!mulib_texruntime_char_(p,')'))p->ok=0;
        return v;
    }
    if(p->p<p->end&&(isdigit((unsigned char)*p->p)||*p->p=='.')){
        char *endptr=NULL;
        double v=strtod(p->p,&endptr);
        if(endptr==p->p||endptr>p->end||!isfinite(v)){p->ok=0;return 0;}
        p->p=endptr;return evaluate?v:0;
    }
    char ident[64];
    if(!mulib_texruntime_identifier_(p,ident)){p->ok=0;return 0;}
    if(mulib_texruntime_char_(p,'(')){
        double arg=mulib_texruntime_expr_(p,evaluate);
        if(!mulib_texruntime_char_(p,')'))p->ok=0;
        return p->ok?mulib_texruntime_call_(p,ident,arg,evaluate):0;
    }
    if(p->param&&!strcmp(p->param,ident))return evaluate?p->arg:0;
    p->ok=0;return 0;
}
static double mulib_texruntime_unary_(mulib_texruntime_parser_ *p,int evaluate){
    if(!mulib_texruntime_tick_(p))return 0;
    int neg=0;
    while(p->ok){
        if(mulib_texruntime_char_(p,'+')){if(!mulib_texruntime_tick_(p))break;continue;}
        if(mulib_texruntime_char_(p,'-')){neg^=1;if(!mulib_texruntime_tick_(p))break;continue;}
        break;
    }
    double result=mulib_texruntime_primary_(p,evaluate);
    return neg?-result:result;
}
static double mulib_texruntime_mul_(mulib_texruntime_parser_ *p,int evaluate){
    double a=mulib_texruntime_unary_(p,evaluate);
    while(p->ok){
        mulib_texruntime_ws_(p);if(p->p>=p->end)break;
        char op=*p->p;if(op!='*'&&op!='/')break;p->p++;
        double b=mulib_texruntime_unary_(p,evaluate);
        if(evaluate){if(op=='/'&&b==0){p->ok=0;return 0;}a=op=='*'?a*b:a/b;if(!isfinite(a))p->ok=0;}
    }
    return a;
}
static double mulib_texruntime_add_(mulib_texruntime_parser_ *p,int evaluate){
    double a=mulib_texruntime_mul_(p,evaluate);
    while(p->ok){
        mulib_texruntime_ws_(p);if(p->p>=p->end)break;
        char op=*p->p;if(op!='+'&&op!='-')break;p->p++;
        double b=mulib_texruntime_mul_(p,evaluate);
        if(evaluate){a=op=='+'?a+b:a-b;if(!isfinite(a))p->ok=0;}
    }
    return a;
}
static double mulib_texruntime_cmp_(mulib_texruntime_parser_ *p,int evaluate){
    double a=mulib_texruntime_add_(p,evaluate);
    while(p->ok){
        mulib_texruntime_ws_(p);if(p->p>=p->end)break;
        char op=*p->p;if(op!='<'&&op!='>'&&op!='='&&op!='!')break;
        p->p++;int eq=p->p<p->end&&*p->p=='=';
        if(eq)p->p++;
        if((op=='='||op=='!')&&!eq){p->ok=0;return 0;}
        double b=mulib_texruntime_add_(p,evaluate);
        if(evaluate){
            switch(op){case '<':a=(double)(eq?a<=b:a<b);break;
                case '>':a=(double)(eq?a>=b:a>b);break;
                case '=':a=(double)(a==b);break;
                case '!':a=(double)(a!=b);break;}
        }
    }
    return a;
}
static double mulib_texruntime_expr_(mulib_texruntime_parser_ *p,int evaluate){
    if(p->ctx->expression_depth>=128){p->ok=0;return 0;}
    p->ctx->expression_depth++;
    double condition=mulib_texruntime_cmp_(p,evaluate);
    double result=condition;
    if(p->ok&&mulib_texruntime_char_(p,'?')){
        double when_true=mulib_texruntime_expr_(p,evaluate&&condition!=0);
        if(!mulib_texruntime_char_(p,':'))p->ok=0;
        double when_false= p->ok ? mulib_texruntime_expr_(p,evaluate&&condition==0) : 0;
        result=evaluate?(condition?when_true:when_false):0;
    }
    p->ctx->expression_depth--;
    return result;
}
int mulib_texruntime_eval(const char *source,size_t max_steps,unsigned max_depth,double *out){
    if(!source||!out||max_depth>256)return 0;
    mulib_texruntime_ctx_ *ctx=(mulib_texruntime_ctx_*)calloc(1,sizeof(*ctx));
    if(!ctx)return 0;
    ctx->max_steps=max_steps?max_steps:100000;
    ctx->max_depth=max_depth?max_depth:64;
    mulib_texruntime_parser_ p={source,source+strlen(source),NULL,0,ctx,0,1};
    while(p.ok){
        mulib_texruntime_ws_(&p);
        if((size_t)(p.end-p.p)<2||p.p[0]!='f'||p.p[1]!='n'||
           (p.end-p.p>2&&(isalnum((unsigned char)p.p[2])||p.p[2]=='_')))break;
        p.p+=2;if(ctx->count>=64){p.ok=0;break;}
        mulib_texruntime_fn_ *f=&ctx->functions[ctx->count];
        if(!mulib_texruntime_identifier_(&p,f->name)||!mulib_texruntime_char_(&p,'(')||
           !mulib_texruntime_identifier_(&p,f->param)||!mulib_texruntime_char_(&p,')')||
           !mulib_texruntime_char_(&p,'=')){p.ok=0;break;}
        mulib_texruntime_ws_(&p);f->begin=p.p;
        size_t parentheses=0;
        while(p.p<p.end){
            char c=*p.p;
            if(c=='(')parentheses++;
            if(c==')'&&parentheses)parentheses--;
            if(c==';'&&!parentheses)break;
            p.p++;
        }
        f->end=p.p;
        if(p.p==p.end||f->begin==f->end){p.ok=0;break;}
        p.p++;ctx->count++;
    }
    if(p.ok){
        mulib_texruntime_ws_(&p);
        if(p.end-p.p>=4&&!memcmp(p.p,"eval",4)&&
           (p.end-p.p==4||isspace((unsigned char)p.p[4])))p.p+=4;
    }
    double result=0;
    if(p.ok)result=mulib_texruntime_expr_(&p,1);
    if(p.ok){mulib_texruntime_char_(&p,';');mulib_texruntime_ws_(&p);}
    int ok=p.ok&&p.p==p.end&&isfinite(result);
    if(ok)*out=result;
    free(ctx);return ok;
}
#ifdef __cplusplus
}
#endif
#endif
