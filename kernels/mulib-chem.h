/** @file mulib-chem.h
 * @brief Chemical formula composition, approximate molar masses, notation and
 * exact one-dimensional stoichiometric balancing. No chemistry simulator.
 * Define MULIB_IMPL_CHEM plus SYMBOL/MATHML, TEX, FRACTION, XML, STRBUF, UTF8.
 */
#ifndef MULIB_CHEM_H
#define MULIB_CHEM_H
#include "mulib-symbol.h"
#include "mulib-tex.h"
#include "mulib-fraction.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_chem_formula mulib_chem_formula_t;
/** Approximate conventional g/mol values; unstable elements use a selected
 * mass number. Not isotope-abundance/uncertainty or safety reference data. */
typedef struct mulib_chem_element { const char *symbol; unsigned atomic_number; double molar_mass; } mulib_chem_element_t;
/** @brief Borrow element 1..118; NULL outside the catalogue. */
const mulib_chem_element_t *mulib_chem_element(unsigned atomic_number);
/** @brief Borrow an exact case-sensitive element-symbol match, or NULL. */
const mulib_chem_element_t *mulib_chem_find_element(const char *symbol);
/** @brief Formula grammar: element counts, nested ()/[], [13C] isotopes, component
 * coefficients, . or middle-dot hydrates, trailing ^2-/^+ charge and (s/l/g/aq)
 * phase. e^- is an electron. Bounds: 64 KiB, depth32, 4096 nodes, 10^12 atoms.
 * Isotope mass is approximated by its integer mass number, not exact mass. */
mulib_chem_formula_t *mulib_chem_parse(const char *,size_t,mulib_xml_error_t *);
/** @brief Destroy an owned formula; NULL safe. */
void mulib_chem_destroy(mulib_chem_formula_t *);
/** @brief Element atom count, including labelled isotopes; zero on invalid input. */
uint64_t mulib_chem_count(const mulib_chem_formula_t *,unsigned atomic_number);
/** @brief Approximate g/mol for the supplied species; zero for NULL/electron. */
double mulib_chem_molar_mass(const mulib_chem_formula_t *);
/** @brief Signed explicit species charge; zero for NULL/uncharged species. */
int mulib_chem_charge(const mulib_chem_formula_t *);
/** @brief Borrow phase s/l/g/aq or empty string; NULL for NULL. */
const char *mulib_chem_phase(const mulib_chem_formula_t *);
/** @brief Borrow the structured formula XML; mutation is not allowed. */
const mulib_xml_node_t *mulib_chem_tree(const mulib_chem_formula_t *);
/** @brief New owned MathML root with preserved formula notation. */
mulib_xml_node_t *mulib_chem_mathml(const mulib_chem_formula_t *,mulib_xml_error_t *);
/** @brief malloc-owned math-mode LaTeX; length excludes NUL and is zero on error. */
char *mulib_chem_latex(const mulib_chem_formula_t *,size_t *,mulib_xml_error_t *);
/** @brief Balance reactants followed by products, conserving elements and charge.
 * 1..8 species on each side, <=16 total. Requires nullity exactly one and a
 * strictly positive solution; returns reduced integer coefficients. Exact
 * checked rationals; ambiguous/impossible/isotopic/overflow cases fail with
 * output unchanged. Formula coefficients already present count as part of each
 * supplied species. No conditions, equilibrium or mechanistic inference. */
int mulib_chem_balance(const mulib_chem_formula_t *const *,size_t reactants,size_t products,uint64_t *coefficients,mulib_xml_error_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CHEM) && !defined(MULIB_CHEM_IMPLEMENTED)
#define MULIB_CHEM_IMPLEMENTED
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static const mulib_chem_element_t mulib_chem_elements_[]={
 {"H",1,1.008},{"He",2,4.002602},{"Li",3,6.94},{"Be",4,9.0121831},{"B",5,10.81},{"C",6,12.011},{"N",7,14.007},{"O",8,15.999},{"F",9,18.998403163},{"Ne",10,20.1797},
 {"Na",11,22.98976928},{"Mg",12,24.305},{"Al",13,26.9815385},{"Si",14,28.085},{"P",15,30.973761998},{"S",16,32.06},{"Cl",17,35.45},{"Ar",18,39.948},{"K",19,39.0983},{"Ca",20,40.078},
 {"Sc",21,44.955908},{"Ti",22,47.867},{"V",23,50.9415},{"Cr",24,51.9961},{"Mn",25,54.938044},{"Fe",26,55.845},{"Co",27,58.933194},{"Ni",28,58.6934},{"Cu",29,63.546},{"Zn",30,65.38},
 {"Ga",31,69.723},{"Ge",32,72.630},{"As",33,74.921595},{"Se",34,78.971},{"Br",35,79.904},{"Kr",36,83.798},{"Rb",37,85.4678},{"Sr",38,87.62},{"Y",39,88.90584},{"Zr",40,91.224},
 {"Nb",41,92.90637},{"Mo",42,95.95},{"Tc",43,98},{"Ru",44,101.07},{"Rh",45,102.90550},{"Pd",46,106.42},{"Ag",47,107.8682},{"Cd",48,112.414},{"In",49,114.818},{"Sn",50,118.710},
 {"Sb",51,121.760},{"Te",52,127.60},{"I",53,126.90447},{"Xe",54,131.293},{"Cs",55,132.90545196},{"Ba",56,137.327},{"La",57,138.90547},{"Ce",58,140.116},{"Pr",59,140.90766},{"Nd",60,144.242},
 {"Pm",61,145},{"Sm",62,150.36},{"Eu",63,151.964},{"Gd",64,157.25},{"Tb",65,158.92535},{"Dy",66,162.500},{"Ho",67,164.93033},{"Er",68,167.259},{"Tm",69,168.93422},{"Yb",70,173.045},
 {"Lu",71,174.9668},{"Hf",72,178.49},{"Ta",73,180.94788},{"W",74,183.84},{"Re",75,186.207},{"Os",76,190.23},{"Ir",77,192.217},{"Pt",78,195.084},{"Au",79,196.966569},{"Hg",80,200.592},
 {"Tl",81,204.38},{"Pb",82,207.2},{"Bi",83,208.98040},{"Po",84,209},{"At",85,210},{"Rn",86,222},{"Fr",87,223},{"Ra",88,226},{"Ac",89,227},{"Th",90,232.0377},
 {"Pa",91,231.03588},{"U",92,238.02891},{"Np",93,237},{"Pu",94,244},{"Am",95,243},{"Cm",96,247},{"Bk",97,247},{"Cf",98,251},{"Es",99,252},{"Fm",100,257},
 {"Md",101,258},{"No",102,259},{"Lr",103,266},{"Rf",104,267},{"Db",105,268},{"Sg",106,269},{"Bh",107,270},{"Hs",108,277},{"Mt",109,278},{"Ds",110,281},
 {"Rg",111,282},{"Cn",112,285},{"Nh",113,286},{"Fl",114,289},{"Mc",115,290},{"Lv",116,293},{"Ts",117,294},{"Og",118,294}
};
struct mulib_chem_formula { mulib_xml_node_t *tree;uint64_t counts[118];double mass;int charge,isotopic;char phase[3]; };
typedef struct mulib_chem_parser_ {const char *s;size_t length,at,nodes,depth;mulib_xml_error_t error;int isotopic;} mulib_chem_parser_;
typedef struct mulib_chem_composition_ {uint64_t counts[118];double mass;} mulib_chem_composition_;
const mulib_chem_element_t *mulib_chem_element(unsigned z){return z>=1&&z<=118?&mulib_chem_elements_[z-1]:NULL;}
const mulib_chem_element_t *mulib_chem_find_element(const char *s){size_t i;if(!s)return NULL;for(i=0;i<118;++i)if(!strcmp(s,mulib_chem_elements_[i].symbol))return &mulib_chem_elements_[i];return NULL;}
static int mulib_chem_fail_(mulib_chem_parser_ *p,const char *m){if(!p->error.message){p->error.offset=p->at;p->error.message=m;}return 0;}
static int mulib_chem_number_(mulib_chem_parser_ *p,uint64_t *out){uint64_t n=0;size_t start=p->at;while(p->at<p->length&&p->s[p->at]>='0'&&p->s[p->at]<='9'){unsigned d=(unsigned)(p->s[p->at++]-'0');if(n>(UINT64_C(1000000000000)-d)/10)return mulib_chem_fail_(p,"chemical count limit");n=n*10+d;}if(p->at==start)n=1;else if(!n)return mulib_chem_fail_(p,"zero chemical count");*out=n;return 1;}
static int mulib_chem_combine_(mulib_chem_parser_ *p,mulib_chem_composition_ *a,const mulib_chem_composition_ *b,uint64_t factor){size_t i;uint64_t total=0;for(i=0;i<118;++i){if(b->counts[i]>(UINT64_C(1000000000000)-a->counts[i])/factor)return mulib_chem_fail_(p,"chemical atom count overflow/limit");a->counts[i]+=b->counts[i]*factor;if(a->counts[i]>UINT64_C(1000000000000)-total)return mulib_chem_fail_(p,"chemical total atom limit");total+=a->counts[i];}a->mass+=b->mass*(double)factor;return isfinite(a->mass)||mulib_chem_fail_(p,"chemical mass overflow");}
static mulib_xml_node_t *mulib_chem_node_(mulib_chem_parser_ *p,const char *name){mulib_xml_node_t *n;if(++p->nodes>4096){(void)mulib_chem_fail_(p,"chemical node limit");return NULL;}n=mulib_xml_new(MULIB_XML_ELEMENT,name);if(!n)(void)mulib_chem_fail_(p,"chemical allocation failure");return n;}
static int mulib_chem_attrnum_(mulib_xml_node_t *n,const char *key,uint64_t v){char b[32];(void)snprintf(b,sizeof b,"%llu",(unsigned long long)v);return mulib_xml_set_attr(n,key,b);}
static int mulib_chem_group_(mulib_chem_parser_ *p,mulib_xml_node_t *parent,mulib_chem_composition_ *out,char closing){
    size_t terms=0;if(++p->depth>32){--p->depth;return mulib_chem_fail_(p,"chemical grouping depth");}
    while(p->at<p->length){
        char c=p->s[p->at];mulib_xml_node_t *n=NULL;mulib_chem_composition_ part={{0},0};uint64_t count=1,isotope=0;const mulib_chem_element_t *element=NULL;
        if(c==closing||(!closing&&(c=='.'||(p->length-p->at>=2&&!memcmp(p->s+p->at,"·",2)))))break;
        if(c=='('||(c=='['&&!(p->at+1<p->length&&p->s[p->at+1]>='0'&&p->s[p->at+1]<='9'))){
            ++p->at;n=mulib_chem_node_(p,"group");if(!n)goto fail;
            if(!mulib_xml_set_attr(n,"bracket",c=='('?"()":"[]")||!mulib_chem_group_(p,n,&part,c=='('?')':']')){mulib_xml_destroy(n);goto fail;}
            if(p->at==p->length||p->s[p->at]!=(c=='('?')':']')){mulib_xml_destroy(n);(void)mulib_chem_fail_(p,"unclosed chemical group");goto fail;}++p->at;
        }else{
            char symbol[3];size_t j=0;
            if(c=='['){++p->at;if(!mulib_chem_number_(p,&isotope)||isotope>400){(void)mulib_chem_fail_(p,"invalid isotope mass number");goto fail;}p->isotopic=1;}
            if(p->at==p->length||p->s[p->at]<'A'||p->s[p->at]>'Z'){(void)mulib_chem_fail_(p,"expected chemical element");goto fail;}
            symbol[j++]=p->s[p->at++];if(p->at<p->length&&p->s[p->at]>='a'&&p->s[p->at]<='z')symbol[j++]=p->s[p->at++];symbol[j]=0;
            element=mulib_chem_find_element(symbol);if(!element){(void)mulib_chem_fail_(p,"unknown chemical element");goto fail;}
            if(isotope){if(isotope<element->atomic_number||p->at==p->length||p->s[p->at++]!=']'){(void)mulib_chem_fail_(p,"invalid isotope notation");goto fail;}}
            n=mulib_chem_node_(p,"element");if(!n)goto fail;
            if(!mulib_xml_set_attr(n,"symbol",symbol)||(isotope&&!mulib_chem_attrnum_(n,"isotope",isotope))){mulib_xml_destroy(n);goto memory;}
            part.counts[element->atomic_number-1]=1;part.mass=isotope?(double)isotope:element->molar_mass;
        }
        if(!mulib_chem_number_(p,&count)||!mulib_chem_combine_(p,out,&part,count)){mulib_xml_destroy(n);goto fail;}
        if(!mulib_chem_attrnum_(n,"count",count)||!mulib_xml_append(parent,n)){mulib_xml_destroy(n);goto memory;}
        ++terms;
    }
    --p->depth;return terms?1:mulib_chem_fail_(p,"empty chemical group/component");
memory:(void)mulib_chem_fail_(p,"chemical allocation failure");
fail:--p->depth;return 0;
}
mulib_chem_formula_t *mulib_chem_parse(const char *s,size_t length,mulib_xml_error_t *error){
    mulib_chem_formula_t *f=NULL;mulib_chem_parser_ p;mulib_chem_composition_ total={{0},0};size_t i;memset(&p,0,sizeof p);p.s=s;p.length=length;
    if(!length||length>65536||!mulib_xml_valid_text(s,length)){(void)mulib_chem_fail_(&p,"invalid/empty chemical source");goto fail;}
    f=(mulib_chem_formula_t *)calloc(1,sizeof *f);if(!f)goto memory;f->tree=mulib_chem_node_(&p,"formula");if(!f->tree)goto fail;
    {static const char *const phases[]={"(aq)","(s)","(l)","(g)"};for(i=0;i<4;++i){size_t n=strlen(phases[i]);if(p.length>=n&&!memcmp(s+p.length-n,phases[i],n)){memcpy(f->phase,phases[i]+1,n-2);p.length-=n;break;}}}
    for(i=0;i<p.length;++i)if(s[i]=='^'){
        uint64_t charge=1;size_t end=p.length;p.at=i+1;
        if(!mulib_chem_number_(&p,&charge)||charge>100||p.at+1!=end||(s[p.at]!='+'&&s[p.at]!='-')){(void)mulib_chem_fail_(&p,"invalid chemical charge");goto fail;}
        f->charge=(int)charge*(s[p.at]=='-'?-1:1);p.length=i;p.at=0;break;
    }
    if(p.length==1&&s[0]=='e'&&f->charge==-1){if(!mulib_xml_set_attr(f->tree,"electron","true"))goto memory;}
    else while(p.at<p.length){
        uint64_t factor;mulib_xml_node_t *component;mulib_chem_composition_ part={{0},0};
        if(!mulib_chem_number_(&p,&factor))goto fail;
        component=mulib_chem_node_(&p,"component");if(!component)goto fail;
        if(!mulib_chem_group_(&p,component,&part,0)||!mulib_chem_combine_(&p,&total,&part,factor)){mulib_xml_destroy(component);goto fail;}
        if(!mulib_chem_attrnum_(component,"count",factor)||!mulib_xml_append(f->tree,component)){mulib_xml_destroy(component);goto memory;}
        if(p.at<p.length){p.at+=s[p.at]=='.'?1:2;if(p.at==p.length){(void)mulib_chem_fail_(&p,"trailing hydrate separator");goto fail;}}
    }
    if(!p.length){(void)mulib_chem_fail_(&p,"missing chemical formula");goto fail;}
    memcpy(f->counts,total.counts,sizeof f->counts);f->mass=total.mass;f->isotopic=p.isotopic;
    { char charge[16];(void)snprintf(charge,sizeof charge,"%d",f->charge);if(!mulib_xml_set_attr(f->tree,"charge",charge)||(*f->phase&&!mulib_xml_set_attr(f->tree,"phase",f->phase)))goto memory; }
    if(error)*error=p.error;
    return f;
memory:(void)mulib_chem_fail_(&p,"chemical allocation failure");
fail:if(!p.error.message)(void)mulib_chem_fail_(&p,"chemical parse/allocation failure");if(error)*error=p.error;mulib_chem_destroy(f);return NULL;
}
void mulib_chem_destroy(mulib_chem_formula_t *f){if(f){mulib_xml_destroy(f->tree);free(f);}}
uint64_t mulib_chem_count(const mulib_chem_formula_t *f,unsigned z){return f&&z>=1&&z<=118?f->counts[z-1]:0;}
double mulib_chem_molar_mass(const mulib_chem_formula_t *f){return f?f->mass:0;}
int mulib_chem_charge(const mulib_chem_formula_t *f){return f?f->charge:0;}
const char *mulib_chem_phase(const mulib_chem_formula_t *f){return f?f->phase:NULL;}
const mulib_xml_node_t *mulib_chem_tree(const mulib_chem_formula_t *f){return f?f->tree:NULL;}
static int mulib_chem_append_(mulib_xml_node_t *p,mulib_xml_node_t *c){if(c&&mulib_xml_append(p,c))return 1;mulib_xml_destroy(c);return 0;}
static mulib_xml_node_t *mulib_chem_mml_(const mulib_xml_node_t *n){
    const char *name=mulib_xml_name(n),*count=mulib_xml_attr(n,"count"),*isotope=mulib_xml_attr(n,"isotope"),*bracket=mulib_xml_attr(n,"bracket");
    const mulib_xml_node_t *c;mulib_xml_node_t *row=mulib_mathml_element("mrow"),*script=NULL;
    if(!row)return NULL;
    if(!strcmp(name,"element")){
        if(isotope){script=mulib_mathml_element("msup");if(!script||!mulib_chem_append_(script,mulib_mathml_element("mrow"))||!mulib_chem_append_(script,mulib_mathml_token("mn",isotope))){mulib_xml_destroy(script);goto fail;}if(!mulib_chem_append_(row,script))goto fail;script=NULL;}
        {mulib_xml_node_t *atom=mulib_mathml_token("mi",mulib_xml_attr(n,"symbol"));if(!atom)goto fail;if(!mulib_xml_set_attr(atom,"mathvariant","normal")){mulib_xml_destroy(atom);goto fail;}if(!mulib_chem_append_(row,atom))goto fail;}
    }else{
        if(!strcmp(name,"component")&&count&&strcmp(count,"1")&&!mulib_chem_append_(row,mulib_mathml_token("mn",count)))goto fail;
        if(bracket&&!mulib_chem_append_(row,mulib_mathml_token("mo",bracket[0]=='('?"(":"[")))goto fail;
        for(c=mulib_xml_first(n);c;c=mulib_xml_next(c))if(!mulib_chem_append_(row,mulib_chem_mml_(c)))goto fail;
        if(bracket&&!mulib_chem_append_(row,mulib_mathml_token("mo",bracket[0]=='('? ")" : "]")))goto fail;
    }
    if(count&&strcmp(count,"1")&&strcmp(name,"component")){
        script=mulib_mathml_element("msub");if(!script)goto fail;
        if(!mulib_xml_append(script,row)){mulib_xml_destroy(script);goto fail;}
        if(!mulib_chem_append_(script,mulib_mathml_token("mn",count))){mulib_xml_destroy(script);return NULL;}return script;
    }
    return row;
fail:mulib_xml_destroy(row);return NULL;
}
mulib_xml_node_t *mulib_chem_mathml(const mulib_chem_formula_t *f,mulib_xml_error_t *e){
    mulib_xml_node_t *math=NULL,*row=NULL,*power;const mulib_xml_node_t *c;char charge[32];size_t i=0;
    if(e){e->offset=0;e->message=NULL;}if(!f)goto fail;math=mulib_mathml_new(0);row=mulib_mathml_element("mrow");if(!math||!row)goto fail;
    if(mulib_xml_attr(f->tree,"electron")&&!mulib_chem_append_(row,mulib_mathml_token("mi","e")))goto fail;
    for(c=mulib_xml_first(f->tree);c;c=mulib_xml_next(c),++i){if(i&&!mulib_chem_append_(row,mulib_symbol_mathml(mulib_symbol_find(NULL,"cdot"))))goto fail;if(!mulib_chem_append_(row,mulib_chem_mml_(c)))goto fail;}
    if(f->charge){if(abs(f->charge)==1)(void)snprintf(charge,sizeof charge,"%c",f->charge<0?'-':'+');else(void)snprintf(charge,sizeof charge,"%d%c",abs(f->charge),f->charge<0?'-':'+');power=mulib_mathml_element("msup");if(!power)goto fail;if(!mulib_xml_append(power,row)){mulib_xml_destroy(power);goto fail;}row=power;if(!mulib_chem_append_(row,mulib_mathml_token("mtext",charge)))goto fail;}
    if(!mulib_xml_append(math,row))goto fail;
    row=NULL;
    if(*f->phase){char phase[8];(void)snprintf(phase,sizeof phase,"(%s)",f->phase);if(!mulib_chem_append_(math,mulib_mathml_token("mtext",phase)))goto fail;}
    return math;
fail:mulib_xml_destroy(row);mulib_xml_destroy(math);if(e)e->message="chemical MathML allocation/argument failure";return NULL;
}
static int mulib_chem_tex_(const mulib_xml_node_t *n,mulib_strbuf_t *b){
    const char *name=mulib_xml_name(n),*count=mulib_xml_attr(n,"count"),*isotope=mulib_xml_attr(n,"isotope"),*bracket=mulib_xml_attr(n,"bracket");const mulib_xml_node_t *c;
    if(!strcmp(name,"element")){if(isotope&&(!mulib_strbuf_append(b,"{}^{")||!mulib_strbuf_append(b,isotope)||!mulib_strbuf_putc(b,'}')))return 0;if(!mulib_strbuf_append(b,"\\mathrm{")||!mulib_strbuf_append(b,mulib_xml_attr(n,"symbol"))||!mulib_strbuf_putc(b,'}'))return 0;}
    else{if(!strcmp(name,"component")&&count&&strcmp(count,"1")&&!mulib_strbuf_append(b,count))return 0;if(bracket&&!mulib_strbuf_putc(b,bracket[0]))return 0;for(c=mulib_xml_first(n);c;c=mulib_xml_next(c))if(!mulib_chem_tex_(c,b))return 0;if(bracket&&!mulib_strbuf_putc(b,bracket[1]))return 0;}
    return !count||!strcmp(count,"1")||!strcmp(name,"component")||(mulib_strbuf_append(b,"_{")&&mulib_strbuf_append(b,count)&&mulib_strbuf_putc(b,'}'));
}
char *mulib_chem_latex(const mulib_chem_formula_t *f,size_t *length,mulib_xml_error_t *e){
    mulib_strbuf_t b={0};const mulib_xml_node_t *c;size_t i=0;char charge[32];if(length)*length=0;if(e){e->offset=0;e->message=NULL;}if(!f||!mulib_strbuf_append(&b,""))goto fail;
    if(mulib_xml_attr(f->tree,"electron")&&!mulib_strbuf_append(&b,"\\mathrm{e}"))goto fail;
    for(c=mulib_xml_first(f->tree);c;c=mulib_xml_next(c),++i){if(i&&!mulib_strbuf_append(&b,"\\cdot "))goto fail;if(!mulib_chem_tex_(c,&b))goto fail;}
    if(f->charge){if(abs(f->charge)==1)(void)snprintf(charge,sizeof charge,"%c",f->charge<0?'-':'+');else(void)snprintf(charge,sizeof charge,"%d%c",abs(f->charge),f->charge<0?'-':'+');if(!mulib_strbuf_append(&b,"^{")||!mulib_strbuf_append(&b,charge)||!mulib_strbuf_putc(&b,'}'))goto fail;}
    if(*f->phase&&(!mulib_strbuf_append(&b,"\\,(\\mathrm{")||!mulib_strbuf_append(&b,f->phase)||!mulib_strbuf_append(&b,"})")))goto fail;
    if(length)*length=b.length;
    return b.data;
fail:mulib_strbuf_free(&b);if(e)e->message="chemical LaTeX allocation/argument failure";return NULL;
}
static uint64_t mulib_chem_gcd_(uint64_t a,uint64_t b){while(b){uint64_t r=a%b;a=b;b=r;}return a;}
static int mulib_chem_mul_(mulib_fraction_t a,mulib_fraction_t b,mulib_fraction_t *out){
    uint64_t aa,bb,g,h;int64_t an,bn,ad,bd;
    if(a.num==INT64_MIN||b.num==INT64_MIN||a.den<=0||b.den<=0)return 0;
    aa=a.num<0?(uint64_t)(-a.num):(uint64_t)a.num;bb=b.num<0?(uint64_t)(-b.num):(uint64_t)b.num;g=mulib_chem_gcd_(aa,(uint64_t)b.den);h=mulib_chem_gcd_(bb,(uint64_t)a.den);an=a.num/(int64_t)g;bn=b.num/(int64_t)h;ad=a.den/(int64_t)h;bd=b.den/(int64_t)g;
    aa=an<0?(uint64_t)(-an):(uint64_t)an;bb=bn<0?(uint64_t)(-bn):(uint64_t)bn;
    if((bb&&aa>(uint64_t)INT64_MAX/bb)||ad>INT64_MAX/bd)return 0;
    return mulib_fraction_make(an*bn,ad*bd,out);
}
int mulib_chem_balance(const mulib_chem_formula_t *const *f,size_t left,size_t right,uint64_t *out,mulib_xml_error_t *e){
    mulib_fraction_t (*a)[16]=NULL,x[16],pivot,factor,term;size_t n=left+right,row=0,col,i,j,k,pivots[16];uint64_t values[16],lcm=1,g=0;const char *message="invalid/ambiguous/unbalanceable chemical reaction";
    if(e){e->offset=0;e->message=NULL;}if(!f||!out||!left||!right||left>8||right>8||n>16)goto fail;
    a=(mulib_fraction_t (*)[16])calloc(119,sizeof *a);if(!a){message="balance allocation failure";goto fail;}
    for(i=0;i<n;++i){if(!f[i]||f[i]->isotopic){message="isotope-specific balancing is unsupported";goto fail;}for(j=0;j<119;++j){a[j][i].den=1;a[j][i].num=j==118?f[i]->charge:(int64_t)f[i]->counts[j];if(i>=left)a[j][i].num=-a[j][i].num;}}
    for(col=0;col<n&&row<119;++col){
        for(k=row;k<119&&!a[k][col].num;++k){}if(k==119)continue;
        for(j=0;j<n;++j){term=a[k][j];a[k][j]=a[row][j];a[row][j]=term;}
        pivot=a[row][col];if(!mulib_fraction_make(pivot.den,pivot.num,&factor))goto overflow;
        for(j=col;j<n;++j)if(!mulib_chem_mul_(a[row][j],factor,&a[row][j]))goto overflow;
        for(i=0;i<119;++i)if(i!=row&&a[i][col].num){factor=a[i][col];if(factor.num==INT64_MIN)goto overflow;factor.num=-factor.num;for(j=col;j<n;++j)if(!mulib_chem_mul_(factor,a[row][j],&term)||!mulib_fraction_add(a[i][j],term,&a[i][j]))goto overflow;}
        pivots[row++]=col;
    }
    if(row!=n-1)goto fail;
    for(i=0;i<n;++i){x[i].num=0;x[i].den=1;}
    for(col=0;col<n;++col){for(i=0;i<row&&pivots[i]!=col;++i){}if(i==row)break;}
    if(col==n)goto fail;
    x[col].num=1;
    for(i=0;i<row;++i){x[pivots[i]]=a[i][col];if(x[pivots[i]].num==INT64_MIN)goto overflow;x[pivots[i]].num=-x[pivots[i]].num;}
    for(i=0;i<n;++i){uint64_t d=(uint64_t)x[i].den,h=mulib_chem_gcd_(lcm,d);if(x[i].num<=0)goto fail;if(lcm/h>(uint64_t)INT64_MAX/d)goto overflow;lcm=lcm/h*d;}
    for(i=0;i<n;++i){uint64_t scale=lcm/(uint64_t)x[i].den;if((uint64_t)x[i].num>(uint64_t)INT64_MAX/scale)goto overflow;values[i]=(uint64_t)x[i].num*scale;g=mulib_chem_gcd_(g,values[i]);}
    for(i=0;i<n;++i)out[i]=values[i]/g;
    free(a);return 1;
overflow:message="exact stoichiometry arithmetic overflow";
fail:free(a);if(e)e->message=message;return 0;
}
#ifdef __cplusplus
}
#endif
#endif
