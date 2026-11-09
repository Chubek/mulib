/** @file mulib-symbol.h
 * @brief Dependency-free extensible mathematical/logic Unicode and TeX symbols.
 * Define MULIB_IMPL_SYMBOL plus MATHML, XML, STRBUF and UTF8.
 */
#ifndef MULIB_SYMBOL_H
#define MULIB_SYMBOL_H
#include "mulib-mathml.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_symbol_registry mulib_symbol_registry_t;
enum { MULIB_SYMBOL_PREFIX=1u, MULIB_SYMBOL_INFIX=2u, MULIB_SYMBOL_RIGHT=4u };
/** Borrowed immutable descriptor. token is mi/mo/mn; precedence 0 for atoms,
 * 1..90 for operators. Custom descriptors are invalidated by registry destroy. */
typedef struct mulib_symbol { const char *name,*utf8,*latex,*token; unsigned flags,precedence; } mulib_symbol_t;
/** @brief An empty extension registry with the builtin catalogue as fallback. NULL
 * registry is also a valid readonly builtin catalogue. No global mutation. */
mulib_symbol_registry_t *mulib_symbol_create(void);
/** @brief Destroy all custom descriptors; NULL safe. Builtins remain valid. */
void mulib_symbol_destroy(mulib_symbol_registry_t *);
/** @brief Copy a unique descriptor atomically; 1/0. Up to 1024 custom entries.
 * TeX representations are trusted author code, not escaped text. */
int mulib_symbol_define(mulib_symbol_registry_t *,const mulib_symbol_t *);
/** @brief Borrow an exact-name descriptor; accepts a leading backslash. */
const mulib_symbol_t *mulib_symbol_find(const mulib_symbol_registry_t *,const char *name);
/** @brief Borrow an exact Unicode-spelling descriptor; NULL when absent. */
const mulib_symbol_t *mulib_symbol_unicode(const mulib_symbol_registry_t *,const char *utf8);
/** @brief Longest Unicode-spelling prefix. Zero consumed on absence/invalid input. */
const mulib_symbol_t *mulib_symbol_match(const mulib_symbol_registry_t *,const char *,size_t,size_t *consumed);
/** @brief Number of builtins plus this registry's custom descriptors. */
size_t mulib_symbol_count(const mulib_symbol_registry_t *);
/** @brief Borrow a zero-based catalogue entry; NULL out of bounds. */
const mulib_symbol_t *mulib_symbol_at(const mulib_symbol_registry_t *,size_t);
/** @brief New owned Presentation MathML token, or NULL. */
mulib_xml_node_t *mulib_symbol_mathml(const mulib_symbol_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYMBOL) && !defined(MULIB_SYMBOL_IMPLEMENTED)
#define MULIB_SYMBOL_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SYMBOL_ATOM_(n,u,l) {n,u,l,"mi",0,0}
#define MULIB_SYMBOL_OP_(n,u,l,f,p) {n,u,l,"mo",f,p}
static const mulib_symbol_t mulib_symbol_builtins_[]={
    MULIB_SYMBOL_ATOM_("alpha","α","\\alpha"), MULIB_SYMBOL_ATOM_("beta","β","\\beta"),
    MULIB_SYMBOL_ATOM_("gamma","γ","\\gamma"), MULIB_SYMBOL_ATOM_("delta","δ","\\delta"),
    MULIB_SYMBOL_ATOM_("epsilon","ε","\\epsilon"), MULIB_SYMBOL_ATOM_("zeta","ζ","\\zeta"),
    MULIB_SYMBOL_ATOM_("eta","η","\\eta"), MULIB_SYMBOL_ATOM_("theta","θ","\\theta"),
    MULIB_SYMBOL_ATOM_("iota","ι","\\iota"), MULIB_SYMBOL_ATOM_("kappa","κ","\\kappa"),
    MULIB_SYMBOL_ATOM_("lambda","λ","\\lambda"), MULIB_SYMBOL_ATOM_("mu","μ","\\mu"),
    MULIB_SYMBOL_ATOM_("nu","ν","\\nu"), MULIB_SYMBOL_ATOM_("xi","ξ","\\xi"),
    MULIB_SYMBOL_ATOM_("omicron","ο","o"), MULIB_SYMBOL_ATOM_("pi","π","\\pi"),
    MULIB_SYMBOL_ATOM_("rho","ρ","\\rho"), MULIB_SYMBOL_ATOM_("sigma","σ","\\sigma"),
    MULIB_SYMBOL_ATOM_("tau","τ","\\tau"), MULIB_SYMBOL_ATOM_("upsilon","υ","\\upsilon"),
    MULIB_SYMBOL_ATOM_("phi","φ","\\phi"), MULIB_SYMBOL_ATOM_("chi","χ","\\chi"),
    MULIB_SYMBOL_ATOM_("psi","ψ","\\psi"), MULIB_SYMBOL_ATOM_("omega","ω","\\omega"),
    MULIB_SYMBOL_ATOM_("Gamma","Γ","\\Gamma"), MULIB_SYMBOL_ATOM_("Delta","Δ","\\Delta"),
    MULIB_SYMBOL_ATOM_("Theta","Θ","\\Theta"), MULIB_SYMBOL_ATOM_("Lambda","Λ","\\Lambda"),
    MULIB_SYMBOL_ATOM_("Xi","Ξ","\\Xi"), MULIB_SYMBOL_ATOM_("Pi","Π","\\Pi"),
    MULIB_SYMBOL_ATOM_("Sigma","Σ","\\Sigma"), MULIB_SYMBOL_ATOM_("Phi","Φ","\\Phi"),
    MULIB_SYMBOL_ATOM_("Psi","Ψ","\\Psi"), MULIB_SYMBOL_ATOM_("Omega","Ω","\\Omega"),
    MULIB_SYMBOL_ATOM_("infinity","∞","\\infty"), MULIB_SYMBOL_ATOM_("emptyset","∅","\\emptyset"),
    MULIB_SYMBOL_ATOM_("partial","∂","\\partial"), MULIB_SYMBOL_ATOM_("nabla","∇","\\nabla"),
    MULIB_SYMBOL_ATOM_("sum","∑","\\sum"), MULIB_SYMBOL_ATOM_("prod","∏","\\prod"),
    MULIB_SYMBOL_ATOM_("integral","∫","\\int"), MULIB_SYMBOL_ATOM_("hbar","ℏ","\\hbar"),
    MULIB_SYMBOL_ATOM_("real","ℝ","\\mathbb{R}"), MULIB_SYMBOL_ATOM_("integer","ℤ","\\mathbb{Z}"),
    MULIB_SYMBOL_ATOM_("natural","ℕ","\\mathbb{N}"), MULIB_SYMBOL_ATOM_("rational","ℚ","\\mathbb{Q}"),
    MULIB_SYMBOL_ATOM_("complex","ℂ","\\mathbb{C}"), MULIB_SYMBOL_ATOM_("true","⊤","\\top"),
    MULIB_SYMBOL_ATOM_("false","⊥","\\bot"), MULIB_SYMBOL_ATOM_("forall","∀","\\forall"),
    MULIB_SYMBOL_ATOM_("exists","∃","\\exists"), MULIB_SYMBOL_ATOM_("therefore","∴","\\therefore"),
    MULIB_SYMBOL_OP_("plus","+","+",3,50), MULIB_SYMBOL_OP_("minus","−","-",3,50),
    MULIB_SYMBOL_OP_("times","×","\\times",2,60), MULIB_SYMBOL_OP_("cdot","·","\\cdot",2,60),
    MULIB_SYMBOL_OP_("divide","÷","\\div",2,60), MULIB_SYMBOL_OP_("slash","/","/",2,60),
    MULIB_SYMBOL_OP_("power","^","^",6,80), MULIB_SYMBOL_OP_("subscript","_","_",6,80),
    MULIB_SYMBOL_OP_("equal","=","=",2,40), MULIB_SYMBOL_OP_("ne","≠","\\ne",2,40),
    MULIB_SYMBOL_OP_("lt","<","<",2,40), MULIB_SYMBOL_OP_("gt",">",">",2,40),
    MULIB_SYMBOL_OP_("le","≤","\\le",2,40), MULIB_SYMBOL_OP_("ge","≥","\\ge",2,40),
    MULIB_SYMBOL_OP_("approx","≈","\\approx",2,40), MULIB_SYMBOL_OP_("equiv","≡","\\equiv",2,40),
    MULIB_SYMBOL_OP_("in","∈","\\in",2,40), MULIB_SYMBOL_OP_("notin","∉","\\notin",2,40),
    MULIB_SYMBOL_OP_("subset","⊂","\\subset",2,40), MULIB_SYMBOL_OP_("subseteq","⊆","\\subseteq",2,40),
    MULIB_SYMBOL_OP_("supset","⊃","\\supset",2,40), MULIB_SYMBOL_OP_("supseteq","⊇","\\supseteq",2,40),
    MULIB_SYMBOL_OP_("union","∪","\\cup",2,45), MULIB_SYMBOL_OP_("intersection","∩","\\cap",2,46),
    MULIB_SYMBOL_OP_("and","∧","\\land",2,30), MULIB_SYMBOL_OP_("or","∨","\\lor",2,20),
    MULIB_SYMBOL_OP_("xor","⊕","\\oplus",2,25), MULIB_SYMBOL_OP_("not","¬","\\lnot",1,70),
    MULIB_SYMBOL_OP_("implies","⇒","\\Rightarrow",6,10), MULIB_SYMBOL_OP_("iff","⇔","\\Leftrightarrow",2,5),
    MULIB_SYMBOL_OP_("arrow","→","\\rightarrow",6,10), MULIB_SYMBOL_OP_("leftarrow","←","\\leftarrow",6,10),
    MULIB_SYMBOL_OP_("reversible","⇌","\\rightleftharpoons",2,10),
    MULIB_SYMBOL_OP_("plusminus","±","\\pm",3,50), MULIB_SYMBOL_OP_("tensor","⊗","\\otimes",2,60),
    MULIB_SYMBOL_OP_("models","⊨","\\models",2,8), MULIB_SYMBOL_OP_("proves","⊢","\\vdash",2,8)
};
#undef MULIB_SYMBOL_ATOM_
#undef MULIB_SYMBOL_OP_
struct mulib_symbol_registry { mulib_symbol_t **items; size_t count; };
size_t mulib_symbol_count(const mulib_symbol_registry_t *r) { return sizeof mulib_symbol_builtins_/sizeof mulib_symbol_builtins_[0]+(r?r->count:0); }
const mulib_symbol_t *mulib_symbol_at(const mulib_symbol_registry_t *r,size_t i) {
    size_t n=sizeof mulib_symbol_builtins_/sizeof mulib_symbol_builtins_[0];
    return i<n?&mulib_symbol_builtins_[i]:r&&i-n<r->count?r->items[i-n]:NULL;
}
mulib_symbol_registry_t *mulib_symbol_create(void) { return (mulib_symbol_registry_t *)calloc(1,sizeof(mulib_symbol_registry_t)); }
void mulib_symbol_destroy(mulib_symbol_registry_t *r) {
    size_t i; if(!r)return;
    for(i=0;i<r->count;++i) { mulib_symbol_t *s=r->items[i]; free((void *)s->name);free((void *)s->utf8);free((void *)s->latex);free((void *)s->token);free(s); }
    free(r->items);free(r);
}
const mulib_symbol_t *mulib_symbol_find(const mulib_symbol_registry_t *r,const char *name) {
    size_t i; if(!name)return NULL;if(*name=='\\')++name;
    for(i=0;i<mulib_symbol_count(r);++i) { const mulib_symbol_t *s=mulib_symbol_at(r,i);if(!strcmp(name,s->name))return s; }return NULL;
}
const mulib_symbol_t *mulib_symbol_unicode(const mulib_symbol_registry_t *r,const char *text) {
    size_t i;if(!text)return NULL;
    for(i=0;i<mulib_symbol_count(r);++i){const mulib_symbol_t *s=mulib_symbol_at(r,i);if(!strcmp(text,s->utf8))return s;}return NULL;
}
const mulib_symbol_t *mulib_symbol_match(const mulib_symbol_registry_t *r,const char *text,size_t n,size_t *used) {
    const mulib_symbol_t *best=NULL;size_t i,length=0;
    if(used)*used=0;
    if(!text)return NULL;
    for(i=0;i<mulib_symbol_count(r);++i){const mulib_symbol_t *s=mulib_symbol_at(r,i);size_t z=strlen(s->utf8);if(z>length&&z<=n&&!memcmp(text,s->utf8,z)){best=s;length=z;}}
    if(used)*used=length;
    return best;
}
static char *mulib_symbol_copy_(const char *s){size_t n=strlen(s);char *p=(char *)malloc(n+1);if(p)memcpy(p,s,n+1);return p;}
int mulib_symbol_define(mulib_symbol_registry_t *r,const mulib_symbol_t *s) {
    mulib_symbol_t *copy,**items;size_t i;
    if(!r||!s||r->count==1024||!s->name||!*s->name||strlen(s->name)>63||!s->utf8||!*s->utf8||strlen(s->utf8)>64||
        !s->latex||strlen(s->latex)>256||!s->token||(strcmp(s->token,"mi")&&strcmp(s->token,"mo")&&strcmp(s->token,"mn"))||
        !mulib_xml_valid_text(s->utf8,strlen(s->utf8))||!mulib_xml_valid_text(s->latex,strlen(s->latex))||
        (s->flags&~7u)||((s->flags&MULIB_SYMBOL_RIGHT)&&!(s->flags&MULIB_SYMBOL_INFIX))||((s->flags&3u)?(!s->precedence||s->precedence>90):s->precedence!=0)||mulib_symbol_find(r,s->name)||mulib_symbol_unicode(r,s->utf8))return 0;
    for(i=0;s->name[i];++i)if(!((s->name[i]>='A'&&s->name[i]<='Z')||(s->name[i]>='a'&&s->name[i]<='z')||s->name[i]=='_'||(i&&s->name[i]>='0'&&s->name[i]<='9')))return 0;
    copy=(mulib_symbol_t *)calloc(1,sizeof *copy);if(!copy)return 0;
    copy->name=mulib_symbol_copy_(s->name);copy->utf8=mulib_symbol_copy_(s->utf8);copy->latex=mulib_symbol_copy_(s->latex);copy->token=mulib_symbol_copy_(s->token);copy->flags=s->flags;copy->precedence=s->precedence;
    if(!copy->name||!copy->utf8||!copy->latex||!copy->token)goto fail;
    items=(mulib_symbol_t **)realloc(r->items,(r->count+1)*sizeof *items);if(!items)goto fail;
    r->items=items;r->items[r->count++]=copy;return 1;
fail:
    free((void *)copy->name);free((void *)copy->utf8);free((void *)copy->latex);free((void *)copy->token);free(copy);return 0;
}
mulib_xml_node_t *mulib_symbol_mathml(const mulib_symbol_t *s){return s&&s->token&&s->utf8?mulib_mathml_token(s->token,s->utf8):NULL;}
#ifdef __cplusplus
}
#endif
#endif
