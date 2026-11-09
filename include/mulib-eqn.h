/** @file mulib-eqn.h
 * @brief Bounded mathematical/logic expression trees, MathML, LaTeX and evaluation.
 * Define MULIB_IMPL_EQN plus SYMBOL, MATHML, TEX, XML, STRBUF and UTF8.
 */
#ifndef MULIB_EQN_H
#define MULIB_EQN_H
#include "mulib-symbol.h"
#include "mulib-tex.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eqn mulib_eqn_t;
typedef enum mulib_eqn_kind { MULIB_EQN_NUMBER,MULIB_EQN_IDENTIFIER,MULIB_EQN_SYMBOL,MULIB_EQN_UNARY,MULIB_EQN_BINARY,MULIB_EQN_CALL,MULIB_EQN_GROUP } mulib_eqn_kind_t;
/** @brief Parse <=64 KiB, <=10000 nodes, depth<=64. Registry is borrowed and must
 * outlive the tree. Explicit operators, (), {}, calls, numbers and symbol
 * names/Unicode/backslash names; no implicit multiplication or TeX execution. */
mulib_eqn_t *mulib_eqn_parse(const mulib_symbol_registry_t *,const char *,size_t,mulib_xml_error_t *);
/** @brief Destroy a complete expression tree; NULL safe. */
void mulib_eqn_destroy(mulib_eqn_t *);
/** @brief Node kind, or GROUP for NULL. */
mulib_eqn_kind_t mulib_eqn_kind(const mulib_eqn_t *);
/** @brief Borrow literal/identifier text or canonical operator name; NULL for NULL. */
const char *mulib_eqn_text(const mulib_eqn_t *);
/** @brief Borrow the registry descriptor, or NULL for nonsymbolic nodes. */
const mulib_symbol_t *mulib_eqn_symbol(const mulib_eqn_t *);
/** @brief Number of children, zero for atoms/NULL. */
size_t mulib_eqn_child_count(const mulib_eqn_t *);
/** @brief Borrow a zero-based child; NULL out of bounds. */
const mulib_eqn_t *mulib_eqn_child(const mulib_eqn_t *,size_t);
/** @brief New owned MathML root; empty source is an empty mrow. */
mulib_xml_node_t *mulib_eqn_mathml(const mulib_eqn_t *,int block,mulib_xml_error_t *);
/** @brief malloc-owned math-mode LaTeX (without dollar delimiters). */
char *mulib_eqn_latex(const mulib_eqn_t *,size_t *length,mulib_xml_error_t *);
/** Resolve a variable to a finite scalar, returning nonzero on success. */
typedef int (*mulib_eqn_resolve_fn)(void *,const char *,double *);
/** @brief Arithmetic, comparisons and short-circuit Boolean logic; standard scalar
 * functions sqrt/abs/sin/cos/exp/ln/frac/root/pow. Unsupported symbolic or
 * quantified operations fail. 1/0, output unchanged on failure. */
int mulib_eqn_eval(const mulib_eqn_t *,mulib_eqn_resolve_fn,void *,double *,mulib_xml_error_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EQN) && !defined(MULIB_EQN_IMPLEMENTED)
#define MULIB_EQN_IMPLEMENTED
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_eqn { mulib_eqn_kind_t kind; char *text; const mulib_symbol_t *symbol; double number; struct mulib_eqn *child[16]; size_t count;unsigned height; };
typedef struct mulib_eqn_parser_ { const char *source;size_t length,at,start,end,nodes,depth;int token;const mulib_symbol_registry_t *registry;const mulib_symbol_t *symbol;mulib_xml_error_t error; } mulib_eqn_parser_;
static int mulib_eqn_fail_(mulib_eqn_parser_ *p,const char *m){if(!p->error.message){p->error.offset=p->start;p->error.message=m;}return 0;}
static char *mulib_eqn_copy_(const char *s,size_t n){char *v=(char *)malloc(n+1);if(v){if(n)memcpy(v,s,n);v[n]=0;}return v;}
void mulib_eqn_destroy(mulib_eqn_t *n){size_t i;if(!n)return;for(i=0;i<n->count;++i)mulib_eqn_destroy(n->child[i]);free(n->text);free(n);}
mulib_eqn_kind_t mulib_eqn_kind(const mulib_eqn_t *n){return n?n->kind:MULIB_EQN_GROUP;}
const char *mulib_eqn_text(const mulib_eqn_t *n){return n?n->text:NULL;}
const mulib_symbol_t *mulib_eqn_symbol(const mulib_eqn_t *n){return n?n->symbol:NULL;}
size_t mulib_eqn_child_count(const mulib_eqn_t *n){return n?n->count:0;}
const mulib_eqn_t *mulib_eqn_child(const mulib_eqn_t *n,size_t i){return n&&i<n->count?n->child[i]:NULL;}
static mulib_eqn_t *mulib_eqn_new_(mulib_eqn_parser_ *p,mulib_eqn_kind_t kind,const char *text,size_t n,const mulib_symbol_t *symbol){
    mulib_eqn_t *v;if(++p->nodes>10000){(void)mulib_eqn_fail_(p,"equation node limit");return NULL;}
    v=(mulib_eqn_t *)calloc(1,sizeof *v);if(!v){(void)mulib_eqn_fail_(p,"equation allocation failure");return NULL;}
    v->kind=kind;v->symbol=symbol;v->height=1;v->text=mulib_eqn_copy_(text,n);if(!v->text){free(v);(void)mulib_eqn_fail_(p,"equation allocation failure");return NULL;}return v;
}
static int mulib_eqn_alpha_(char c){return(c>='A'&&c<='Z')||(c>='a'&&c<='z');}
static int mulib_eqn_digit_(char c){return c>='0'&&c<='9';}
static int mulib_eqn_next_(mulib_eqn_parser_ *p){
    size_t a,n,i;char name[128];
    while(p->at<p->length&&(p->source[p->at]==' '||p->source[p->at]=='\t'||p->source[p->at]=='\r'||p->source[p->at]=='\n'))++p->at;
    p->start=p->at;p->symbol=NULL;if(p->at==p->length){p->token=0;p->end=p->at;return 1;}
    a=p->at;
    if(strchr("(){},",p->source[a])){p->token=(unsigned char)p->source[p->at++];p->end=p->at;return 1;}
    if(mulib_eqn_digit_(p->source[a])||(p->source[a]=='.'&&a+1<p->length&&mulib_eqn_digit_(p->source[a+1]))){
        while(p->at<p->length&&mulib_eqn_digit_(p->source[p->at]))++p->at;
        if(p->at<p->length&&p->source[p->at]=='.'){++p->at;while(p->at<p->length&&mulib_eqn_digit_(p->source[p->at]))++p->at;}
        if(p->at<p->length&&(p->source[p->at]=='e'||p->source[p->at]=='E')){++p->at;if(p->at<p->length&&(p->source[p->at]=='+'||p->source[p->at]=='-'))++p->at;i=p->at;while(p->at<p->length&&mulib_eqn_digit_(p->source[p->at]))++p->at;if(i==p->at)return mulib_eqn_fail_(p,"invalid numeric exponent");}
        p->token=256;p->end=p->at;return 1;
    }
    if(mulib_eqn_alpha_(p->source[a])||p->source[a]=='\\'){
        int escaped=p->source[a]=='\\';p->at+=escaped;
        i=p->at;while(p->at<p->length&&(mulib_eqn_alpha_(p->source[p->at])||mulib_eqn_digit_(p->source[p->at])||(escaped&&p->source[p->at]=='_')))++p->at;
        n=p->at-i;if(!n||n>=sizeof name)return mulib_eqn_fail_(p,"invalid/oversized equation identifier");
        memcpy(name,p->source+i,n);name[n]=0;p->symbol=mulib_symbol_find(p->registry,name);
        if(escaped&&!p->symbol)return mulib_eqn_fail_(p,"unknown equation symbol");
        p->token=p->symbol?258:257;p->end=p->at;return 1;
    }
    {static const struct{const char *text,*name;} aliases[]={ {"<->","iff"},{"=>","implies"},{"->","arrow"},{"<=","le"},{">=","ge"},{"!=","ne"},{"&&","and"},{"||","or"},{"*","cdot"},{"-","minus"},{"!","not"} };
     for(i=0;i<sizeof aliases/sizeof aliases[0];++i){n=strlen(aliases[i].text);if(n<=p->length-a&&!memcmp(p->source+a,aliases[i].text,n)){p->symbol=mulib_symbol_find(p->registry,aliases[i].name);p->at+=n;p->token=258;p->end=p->at;return 1;}}
    }
    p->symbol=mulib_symbol_match(p->registry,p->source+a,p->length-a,&n);
    if(p->symbol){p->at+=n;p->end=p->at;p->token=258;return 1;}
    if((unsigned char)p->source[a]>=128){uint32_t cp;if(!mulib_utf8_decode((const unsigned char *)p->source+a,p->length-a,&cp,&n))return mulib_eqn_fail_(p,"invalid equation UTF-8");p->at+=n;p->end=p->at;p->token=257;return 1;}
    return mulib_eqn_fail_(p,"unexpected equation token");
}
static int mulib_eqn_number_(const char *s,double *out){
    long double v=0,scale=1;int exponent=0,negative=0,frac=0;size_t i=0;
    while(s[i]&&s[i]!='e'&&s[i]!='E'){if(s[i]=='.')frac=1;else{v=v*10+(s[i]-'0');if(frac)scale*=10;}++i;}
    if(s[i]){++i;if(s[i]=='+'||s[i]=='-')negative=s[i++]=='-';while(s[i]){if(exponent>4096)return 0;exponent=exponent*10+s[i++]-'0';}}
    if(exponent>4096)return 0;
    v=v/scale*powl(10,negative?-exponent:exponent);*out=(double)v;return isfinite(*out);
}
static mulib_eqn_t *mulib_eqn_expr_(mulib_eqn_parser_ *,unsigned);
static mulib_eqn_t *mulib_eqn_expr_(mulib_eqn_parser_ *p,unsigned minimum){
    mulib_eqn_t *left=NULL,*node=NULL,*right;const mulib_symbol_t *op;
    if(++p->depth>64){(void)mulib_eqn_fail_(p,"equation nesting limit");goto fail;}
    if(p->token==258&&p->symbol&&(p->symbol->flags&MULIB_SYMBOL_PREFIX)){
        op=p->symbol;node=mulib_eqn_new_(p,MULIB_EQN_UNARY,op->name,strlen(op->name),op);if(!node||!mulib_eqn_next_(p))goto fail;
        left=mulib_eqn_expr_(p,!strcmp(op->name,"plus")||!strcmp(op->name,"minus")?70:op->precedence);if(!left)goto fail;node->height=left->height+1;node->child[node->count++]=left;left=node;node=NULL;
        if(left->height>64){(void)mulib_eqn_fail_(p,"equation tree depth limit");goto fail;}
    }else if(p->token=='('||p->token=='{'){
        int closing=p->token=='('?')':'}';if(!mulib_eqn_next_(p))goto fail;left=mulib_eqn_expr_(p,0);if(!left)goto fail;
        if(p->token!=closing){(void)mulib_eqn_fail_(p,"expected closing equation group");goto fail;}
        node=mulib_eqn_new_(p,MULIB_EQN_GROUP,"",0,NULL);if(!node)goto fail;node->height=left->height+1;node->child[node->count++]=left;left=node;node=NULL;if(!mulib_eqn_next_(p))goto fail;
        if(left->height>64){(void)mulib_eqn_fail_(p,"equation tree depth limit");goto fail;}
    }else if(p->token==256||p->token==257||(p->token==258&&p->symbol&&!p->symbol->flags)){
        int token=p->token;const char *text=p->symbol?p->symbol->name:p->source+p->start;size_t length=p->symbol?strlen(text):p->end-p->start;
        left=mulib_eqn_new_(p,token==256?MULIB_EQN_NUMBER:token==258?MULIB_EQN_SYMBOL:MULIB_EQN_IDENTIFIER,text,length,p->symbol);if(!left)goto fail;
        if(token==256&&!mulib_eqn_number_(left->text,&left->number)){(void)mulib_eqn_fail_(p,"equation number out of range");goto fail;}
        if(!mulib_eqn_next_(p))goto fail;
        if(p->token=='('&&token!=256){
            left->kind=MULIB_EQN_CALL;if(!mulib_eqn_next_(p))goto fail;
            if(p->token!=')')for(;;){if(left->count==16){(void)mulib_eqn_fail_(p,"equation call arity limit");goto fail;}right=mulib_eqn_expr_(p,0);if(!right)goto fail;left->child[left->count++]=right;if(right->height>=left->height)left->height=right->height+1;if(left->height>64){(void)mulib_eqn_fail_(p,"equation tree depth limit");goto fail;}if(p->token!=',')break;if(!mulib_eqn_next_(p))goto fail;}
            if(p->token!=')'){(void)mulib_eqn_fail_(p,"expected closing equation call");goto fail;}if(!mulib_eqn_next_(p))goto fail;
            if(((!strcmp(left->text,"sqrt")||!strcmp(left->text,"abs"))&&left->count!=1)||
                ((!strcmp(left->text,"frac")||!strcmp(left->text,"root")||!strcmp(left->text,"pow")||!strcmp(left->text,"forall")||!strcmp(left->text,"exists"))&&left->count!=2)||
                (!strcmp(left->text,"subsup")&&left->count!=3)){(void)mulib_eqn_fail_(p,"incorrect builtin equation arity");goto fail;}
        }
    }else{(void)mulib_eqn_fail_(p,"expected equation operand");goto fail;}
    while(p->token==258&&p->symbol&&(p->symbol->flags&MULIB_SYMBOL_INFIX)&&p->symbol->precedence>=minimum){
        op=p->symbol;node=mulib_eqn_new_(p,MULIB_EQN_BINARY,op->name,strlen(op->name),op);if(!node||!mulib_eqn_next_(p))goto fail;
        node->height=left->height+1;node->child[node->count++]=left;left=NULL;right=mulib_eqn_expr_(p,op->precedence+((op->flags&MULIB_SYMBOL_RIGHT)?0:1));if(!right)goto fail;
        node->child[node->count++]=right;if(right->height>=node->height)node->height=right->height+1;left=node;node=NULL;
        if(left->height>64){(void)mulib_eqn_fail_(p,"equation tree depth limit");goto fail;}
    }
    --p->depth;return left;
fail:
    --p->depth;mulib_eqn_destroy(left);mulib_eqn_destroy(node);return NULL;
}
mulib_eqn_t *mulib_eqn_parse(const mulib_symbol_registry_t *r,const char *s,size_t length,mulib_xml_error_t *e){
    mulib_eqn_parser_ p;mulib_eqn_t *n=NULL;memset(&p,0,sizeof p);p.source=s;p.length=length;p.registry=r;
    if(length>65536||!mulib_xml_valid_text(s,length)){(void)mulib_eqn_fail_(&p,"invalid/oversized equation source");goto done;}
    if(!mulib_eqn_next_(&p))goto done;
    if(!p.token)n=mulib_eqn_new_(&p,MULIB_EQN_GROUP,"",0,NULL);else n=mulib_eqn_expr_(&p,0);
    if(n&&p.token){(void)mulib_eqn_fail_(&p,"unexpected trailing equation token (implicit multiplication is unsupported)");mulib_eqn_destroy(n);n=NULL;}
done:
    if(e)*e=p.error;
    return n;
}
static int mulib_eqn_append_(mulib_xml_node_t *p,mulib_xml_node_t *c){if(c&&mulib_xml_append(p,c))return 1;mulib_xml_destroy(c);return 0;}
static mulib_xml_node_t *mulib_eqn_mml_number_(const char *text) {
    const char *exponent=strpbrk(text,"eE");
    mulib_xml_node_t *row=NULL,*power=NULL;
    char *mantissa;
    if(!exponent)return mulib_mathml_token("mn",text);
    mantissa=mulib_eqn_copy_(text,(size_t)(exponent-text));if(!mantissa)return NULL;
    row=mulib_mathml_element("mrow");power=mulib_mathml_element("msup");
    if(!row||!power||!mulib_eqn_append_(row,mulib_mathml_token("mn",mantissa))||
       !mulib_eqn_append_(row,mulib_symbol_mathml(mulib_symbol_find(NULL,"times")))||
       !mulib_eqn_append_(power,mulib_mathml_token("mn","10"))||
       !mulib_eqn_append_(power,mulib_mathml_token("mn",exponent+1))||!mulib_xml_append(row,power)) {
        mulib_xml_destroy(power);mulib_xml_destroy(row);free(mantissa);return NULL;
    }
    free(mantissa);return row;
}
static mulib_xml_node_t *mulib_eqn_mml_(const mulib_eqn_t *n,unsigned depth){
    mulib_xml_node_t *p=NULL;size_t i;const char *tag=NULL,*name=n->text;
    if(depth>64)return NULL;
    if(n->kind==MULIB_EQN_NUMBER)return mulib_eqn_mml_number_(name);
    if(n->kind==MULIB_EQN_IDENTIFIER)return mulib_mathml_token("mi",name);
    if(n->kind==MULIB_EQN_SYMBOL)return mulib_symbol_mathml(n->symbol);
    if(n->kind==MULIB_EQN_BINARY){if(!strcmp(name,"slash"))tag="mfrac";else if(!strcmp(name,"power"))tag="msup";else if(!strcmp(name,"subscript"))tag="msub";}
    if(n->kind==MULIB_EQN_CALL){if(!strcmp(name,"frac"))tag="mfrac";else if(!strcmp(name,"sqrt"))tag="msqrt";else if(!strcmp(name,"root"))tag="mroot";else if(!strcmp(name,"pow"))tag="msup";else if(!strcmp(name,"subsup"))tag="msubsup";}
    p=mulib_mathml_element(tag?tag:"mrow");if(!p)return NULL;
    if(tag){for(i=0;i<n->count;++i)if(!mulib_eqn_append_(p,mulib_eqn_mml_(n->child[i],depth+1)))goto fail;return p;}
    if(n->kind==MULIB_EQN_UNARY){if(!mulib_eqn_append_(p,mulib_symbol_mathml(n->symbol))||!mulib_eqn_append_(p,mulib_eqn_mml_(n->child[0],depth+1)))goto fail;return p;}
    if(n->kind==MULIB_EQN_CALL){if(!mulib_eqn_append_(p,n->symbol?mulib_symbol_mathml(n->symbol):mulib_mathml_token("mi",name)))goto fail;}
    if(n->count&&!mulib_eqn_append_(p,mulib_mathml_token("mo","(")))goto fail;
    for(i=0;i<n->count;++i){if(i&&!mulib_eqn_append_(p,n->kind==MULIB_EQN_BINARY?mulib_symbol_mathml(n->symbol):mulib_mathml_token("mo",",")))goto fail;if(!mulib_eqn_append_(p,mulib_eqn_mml_(n->child[i],depth+1)))goto fail;}
    if(n->count&&!mulib_eqn_append_(p,mulib_mathml_token("mo",")")))goto fail;
    return p;
fail:mulib_xml_destroy(p);return NULL;
}
mulib_xml_node_t *mulib_eqn_mathml(const mulib_eqn_t *n,int block,mulib_xml_error_t *e){
    mulib_xml_node_t *p=NULL;if(e){e->offset=0;e->message=NULL;}if(n)p=mulib_mathml_new(block);
    if(!p||!mulib_eqn_append_(p,mulib_eqn_mml_(n,0))){mulib_xml_destroy(p);if(e)e->message="invalid equation/MathML allocation failure";return NULL;}return p;
}
static int mulib_eqn_tex_(const mulib_eqn_t *n,mulib_strbuf_t *b,unsigned depth){
    size_t i;const char *name=n->text,*command=NULL;if(depth>64)return 0;
    if(n->kind==MULIB_EQN_NUMBER) {
        const char *exponent=strpbrk(name,"eE");
        if(!exponent)return mulib_strbuf_append(b,name);
        return mulib_strbuf_putc(b,'{')&&mulib_strbuf_append_n(b,name,(size_t)(exponent-name))&&
            mulib_strbuf_append(b,"\\times 10^{")&&mulib_strbuf_append(b,exponent+1)&&mulib_strbuf_append(b,"}}");
    }
    if(n->kind==MULIB_EQN_IDENTIFIER)return mulib_strbuf_append(b,"\\mathrm{")&&mulib_tex_escape(b,name,strlen(name))&&mulib_strbuf_putc(b,'}');
    if(n->kind==MULIB_EQN_SYMBOL)return mulib_strbuf_putc(b,'{')&&mulib_strbuf_append(b,n->symbol->latex)&&mulib_strbuf_putc(b,'}');
    if(n->kind==MULIB_EQN_UNARY)return mulib_strbuf_putc(b,'{')&&mulib_strbuf_append(b,n->symbol->latex)&&mulib_strbuf_putc(b,' ')&&mulib_eqn_tex_(n->child[0],b,depth+1)&&mulib_strbuf_putc(b,'}');
    if((n->kind==MULIB_EQN_BINARY&&!strcmp(name,"slash"))||(n->kind==MULIB_EQN_CALL&&!strcmp(name,"frac")))command="\\frac";
    else if(n->kind==MULIB_EQN_CALL&&!strcmp(name,"sqrt"))command="\\sqrt";
    if(command){if(!mulib_strbuf_append(b,command))return 0;for(i=0;i<n->count;++i)if(!mulib_strbuf_putc(b,'{')||!mulib_eqn_tex_(n->child[i],b,depth+1)||!mulib_strbuf_putc(b,'}'))return 0;return 1;}
    if(n->kind==MULIB_EQN_CALL&&!strcmp(name,"root"))return mulib_strbuf_append(b,"\\sqrt[")&&mulib_eqn_tex_(n->child[1],b,depth+1)&&mulib_strbuf_append(b,"]{")&&mulib_eqn_tex_(n->child[0],b,depth+1)&&mulib_strbuf_putc(b,'}');
    if((n->kind==MULIB_EQN_BINARY&&(!strcmp(name,"power")||!strcmp(name,"subscript")))||(n->kind==MULIB_EQN_CALL&&!strcmp(name,"pow")))return mulib_strbuf_putc(b,'{')&&mulib_eqn_tex_(n->child[0],b,depth+1)&&mulib_strbuf_append(b,!strcmp(name,"subscript")?"}_{":"}^{")&&mulib_eqn_tex_(n->child[1],b,depth+1)&&mulib_strbuf_putc(b,'}');
    if(n->kind==MULIB_EQN_CALL&&!strcmp(name,"subsup"))return mulib_strbuf_putc(b,'{')&&mulib_eqn_tex_(n->child[0],b,depth+1)&&mulib_strbuf_append(b,"}_{")&&mulib_eqn_tex_(n->child[1],b,depth+1)&&mulib_strbuf_append(b,"}^{")&&mulib_eqn_tex_(n->child[2],b,depth+1)&&mulib_strbuf_putc(b,'}');
    if(n->kind==MULIB_EQN_CALL){if(n->symbol){if(!mulib_strbuf_append(b,n->symbol->latex))return 0;}else if(!mulib_strbuf_append(b,"\\operatorname{")||!mulib_tex_escape(b,name,strlen(name))||!mulib_strbuf_putc(b,'}'))return 0;}
    if(n->count&&!mulib_strbuf_append(b,"\\left("))return 0;
    for(i=0;i<n->count;++i){if(i&&!(n->kind==MULIB_EQN_BINARY?mulib_strbuf_append(b,n->symbol->latex):mulib_strbuf_putc(b,',')))return 0;if(i&&!mulib_strbuf_putc(b,' '))return 0;if(!mulib_eqn_tex_(n->child[i],b,depth+1))return 0;}
    return !n->count||mulib_strbuf_append(b,"\\right)");
}
char *mulib_eqn_latex(const mulib_eqn_t *n,size_t *length,mulib_xml_error_t *e){mulib_strbuf_t b={0};if(length)*length=0;if(e){e->offset=0;e->message=NULL;}if(!n||!mulib_strbuf_append(&b,"")||!mulib_eqn_tex_(n,&b,0)){mulib_strbuf_free(&b);if(e)e->message="invalid equation/LaTeX allocation failure";return NULL;}if(length)*length=b.length;return b.data;}
static int mulib_eqn_eval_(const mulib_eqn_t *n,mulib_eqn_resolve_fn resolve,void *user,double *out,unsigned depth){
    double a=0,b=0,v=0;const char *s=n->text;if(depth>64)return 0;
    if(n->kind==MULIB_EQN_NUMBER)v=n->number;
    else if(n->kind==MULIB_EQN_SYMBOL&&!strcmp(s,"true"))v=1;
    else if(n->kind==MULIB_EQN_SYMBOL&&!strcmp(s,"false"))v=0;
    else if(n->kind==MULIB_EQN_SYMBOL&&!strcmp(s,"pi"))v=3.14159265358979323846;
    else if(n->kind==MULIB_EQN_IDENTIFIER||n->kind==MULIB_EQN_SYMBOL){if(!resolve||!resolve(user,s,&v))return 0;}
    else {
        if(!n->count||!mulib_eqn_eval_(n->child[0],resolve,user,&a,depth+1))return 0;
        if(n->kind==MULIB_EQN_GROUP){v=a;}
        else if(n->kind==MULIB_EQN_UNARY){if(!strcmp(s,"not"))v=!a;else if(!strcmp(s,"minus"))v=-a;else if(!strcmp(s,"plus"))v=a;else return 0;}
        else if(n->kind==MULIB_EQN_BINARY&&!strcmp(s,"and")&&!a)v=0;
        else if(n->kind==MULIB_EQN_BINARY&&!strcmp(s,"or")&&a)v=1;
        else if(n->kind==MULIB_EQN_BINARY&&(!strcmp(s,"implies")||!strcmp(s,"arrow"))&&!a)v=1;
        else if(n->kind==MULIB_EQN_CALL&&n->count==1){if(!strcmp(s,"sqrt")&&a>=0)v=sqrt(a);else if(!strcmp(s,"abs"))v=fabs(a);else if(!strcmp(s,"sin"))v=sin(a);else if(!strcmp(s,"cos"))v=cos(a);else if(!strcmp(s,"exp"))v=exp(a);else if(!strcmp(s,"ln")&&a>0)v=log(a);else return 0;}
        else {
            if(n->count!=2||!mulib_eqn_eval_(n->child[1],resolve,user,&b,depth+1))return 0;
            if(!strcmp(s,"plus"))v=a+b;else if(!strcmp(s,"minus"))v=a-b;else if(!strcmp(s,"times")||!strcmp(s,"cdot"))v=a*b;
            else if((!strcmp(s,"slash")||!strcmp(s,"divide")||!strcmp(s,"frac"))&&b!=0)v=a/b;
            else if(!strcmp(s,"power")||!strcmp(s,"pow"))v=pow(a,b);else if(!strcmp(s,"root")&&b!=0)v=pow(a,1/b);
            else if(!strcmp(s,"equal"))v=a==b;else if(!strcmp(s,"ne"))v=a!=b;else if(!strcmp(s,"lt"))v=a<b;else if(!strcmp(s,"gt"))v=a>b;else if(!strcmp(s,"le"))v=a<=b;else if(!strcmp(s,"ge"))v=a>=b;
            else if(!strcmp(s,"and"))v=a!=0&&b!=0;else if(!strcmp(s,"or"))v=a!=0||b!=0;else if(!strcmp(s,"xor"))v=(a!=0)!=(b!=0);else if(!strcmp(s,"implies")||!strcmp(s,"arrow"))v=a==0||b!=0;else if(!strcmp(s,"iff"))v=(a!=0)==(b!=0);else return 0;
        }
    }
    if(!isfinite(v))return 0;
    *out=v;return 1;
}
int mulib_eqn_eval(const mulib_eqn_t *n,mulib_eqn_resolve_fn fn,void *u,double *out,mulib_xml_error_t *e){double v;if(e){e->offset=0;e->message=NULL;}if(!n||!out||!mulib_eqn_eval_(n,fn,u,&v,0)){if(e)e->message="undefined, unsupported or nonfinite equation evaluation";return 0;}*out=v;return 1;}
#ifdef __cplusplus
}
#endif
#endif
