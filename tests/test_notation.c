#include "../examples/notation-common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EQN(r,s,e) mulib_eqn_parse(r,s,sizeof(s)-1,e)
#define JSON(s) mulib_json_parse(s,sizeof(s)-1,NULL)
static void tables(void){
    const char source[]="| A | B |\n| :--- | ---: |\n| alpha | beta |\n";
    mulib_tbl_t *t=mulib_tbl_parse(source,sizeof source-1,NULL,NULL),*copy;
    mulib_tbl_fields_t f={0};mulib_xml_error_t e;char *text;
    assert(t&&mulib_tbl_validate(t,&e)&&mulib_tbl_rows(t)==2&&mulib_tbl_columns(t)==2);
    assert(!strcmp(mulib_xml_attr(mulib_tbl_cell(t,1,1),"align"),"right"));
    text=mulib_tbl_render(t,"csv",NULL,&e);assert(text);copy=mulib_tbl_parse(text,strlen(text),"csv",&e);assert(copy&&mulib_tbl_rows(copy)==2);free(text);mulib_xml_destroy(copy);
    text=mulib_tbl_render(t,"latex",NULL,&e);assert(text&&strstr(text,"\\begin{tabular}{lr}"));free(text);
    text=mulib_tbl_render(t,"html",NULL,&e);assert(text&&strstr(text,"<th align=\"left\">A</th>"));free(text);mulib_xml_destroy(t);
    assert(mulib_tbl_split("| ``a|b`` | @span{x|y} |",sizeof("| ``a|b`` | @span{x|y} |")-1,&f,&e));assert(f.count==2);mulib_tbl_fields_free(&f);
    assert(!mulib_tbl_parse("| a | b |\n| one |",sizeof("| a | b |\n| one |")-1,NULL,&e));
}
static int variable(void *u,const char *name,double *out){(void)u;if(strcmp(name,"x"))return 0;*out=3;return 1;}
static void equations(void){
    mulib_eqn_t *n;mulib_xml_node_t *m;mulib_mathml_error_t me;mulib_xml_error_t e;double out=99;char *text;
    mulib_symbol_registry_t *r=mulib_symbol_create();mulib_symbol_t custom={"starlogic","⋆","\\star","mo",MULIB_SYMBOL_INFIX,35};
    assert(r&&mulib_symbol_count(r)>70&&mulib_symbol_define(r,&custom)&&!mulib_symbol_define(r,&custom));
    n=EQN(r,"sqrt(x^2) + frac(1,2) >= 3 and not false",&e);
    if(!n)fprintf(stderr,"eqn: %zu %s\n",e.offset,e.message);
    assert(n);
    assert(mulib_eqn_eval(n,variable,NULL,&out,&e)&&out==1);
    m=mulib_eqn_mathml(n,1,&e);assert(m&&mulib_mathml_validate(m,&me));mulib_xml_destroy(m);
    text=mulib_eqn_latex(n,NULL,&e);assert(text&&strstr(text,"\\frac"));free(text);mulib_eqn_destroy(n);
    n=EQN(r,"forall(x, P(x) -> Q(x))",&e);assert(n);m=mulib_eqn_mathml(n,0,&e);assert(m&&mulib_mathml_validate(m,&me));mulib_xml_destroy(m);assert(!mulib_eqn_eval(n,NULL,NULL,&out,&e)&&out==1);mulib_eqn_destroy(n);
    n=EQN(r,"x \\starlogic y",&e);assert(n&&mulib_eqn_symbol(n)==mulib_symbol_find(r,"starlogic"));mulib_eqn_destroy(n);
    assert(!mulib_eqn_parse(r,"frac(1)",7,&e)&&e.message);assert(!mulib_eqn_parse(r,"2x",2,&e));
    mulib_symbol_destroy(r);
}
static mulib_chem_formula_t *formula(const char *source){mulib_xml_error_t e;mulib_chem_formula_t *f=mulib_chem_parse(source,strlen(source),&e);if(!f)fprintf(stderr,"chem %s: %s\n",source,e.message);assert(f);return f;}
static void chemistry(void){
    mulib_chem_formula_t *f=formula("CuSO4·5H2O"),*isotope;mulib_xml_error_t e;mulib_mathml_error_t me;mulib_xml_node_t *m;char *text;uint64_t c[4]={99,99,99,99};
    assert(mulib_chem_count(f,1)==10&&mulib_chem_count(f,8)==9&&fabs(mulib_chem_molar_mass(f)-249.677)<.1);
    m=mulib_chem_mathml(f,&e);assert(m&&mulib_mathml_validate(m,&me));mulib_xml_destroy(m);text=mulib_chem_latex(f,NULL,&e);assert(text&&strstr(text,"\\cdot"));free(text);mulib_chem_destroy(f);
    f=formula("[Fe(CN)6]^3-(aq)");assert(mulib_chem_charge(f)==-3&&mulib_chem_count(f,6)==6&&!strcmp(mulib_chem_phase(f),"aq"));mulib_chem_destroy(f);
    isotope=formula("[13C]O2");assert(mulib_chem_count(isotope,6)==1&&fabs(mulib_chem_molar_mass(isotope)-44.998)<.001);mulib_chem_destroy(isotope);
    {mulib_chem_formula_t *a=formula("H2"),*b=formula("O2"),*d=formula("H2O");const mulib_chem_formula_t *all[]={a,b,d};assert(mulib_chem_balance(all,2,1,c,&e)&&c[0]==2&&c[1]==1&&c[2]==2);mulib_chem_destroy(a);mulib_chem_destroy(b);mulib_chem_destroy(d);}
    {mulib_chem_formula_t *a=formula("Fe^3+"),*b=formula("e^-"),*d=formula("Fe^2+");const mulib_chem_formula_t *all[]={a,b,d};assert(mulib_chem_balance(all,2,1,c,&e)&&c[0]==1&&c[1]==1&&c[2]==1);mulib_chem_destroy(a);mulib_chem_destroy(b);mulib_chem_destroy(d);}
    assert(!mulib_chem_parse("H0",2,&e)&&e.message);assert(!mulib_chem_parse("Xx2",3,&e));assert(!mulib_chem_parse("(OH",3,&e));
}
static void charts(void){
    const char *extension="assert(require('lcharts')==lcharts and os==nil);lcharts.register('novel',function(data,o) local s=lcharts.svg(200,200,'Novel');s:add(lcharts.circle(100,100,data[1].value,{fill='red'}));return s end)";
    mulib_chart_error_t e;mulib_chart_t *c=mulib_chart_create(NULL,&e);mulib_json_t *data=JSON("[{\"label\":\"A & B\",\"value\":30},{\"label\":\"C\",\"value\":70}]");mulib_xml_node_t *svg;char *text;size_t i;
    static const char *const kinds[]={"pie","donut","bar","line"};if(!c)fprintf(stderr,"chart: %s\n",e.message);assert(c);if(!data)data=mulib_json_parse("[30,70]",7,NULL);assert(data);
    for(i=0;i<4;++i){svg=mulib_chart_render(c,kinds[i],data,NULL,&e);if(!svg)fprintf(stderr,"chart %s: %s\n",kinds[i],e.message);assert(svg);text=mulib_xml_serialize(svg,NULL);assert(text);free(text);mulib_xml_destroy(svg);}
    mulib_json_destroy(data);data=JSON("[{\"value\":12}]");assert(data);assert(mulib_chart_extend(c,extension,strlen(extension),&e));svg=mulib_chart_render(c,"novel",data,NULL,&e);assert(svg);mulib_xml_destroy(svg);
    assert(!mulib_chart_render(c,"missing",data,NULL,&e)&&*e.message);mulib_json_destroy(data);
    {const char *cycle="lcharts.register('cycle',function() local x={tag='svg',children={}};x.children[1]=x;return x end)";assert(mulib_chart_extend(c,cycle,strlen(cycle),&e));}
    data=mulib_json_new(MULIB_JSON_ARRAY);assert(data);assert(!mulib_chart_render(c,"cycle",data,NULL,&e));mulib_json_destroy(data);mulib_chart_destroy(c);
}
static int native_font(void *u,const char *font,uint32_t cp,mulib_strbuf_t *out,double *advance){(void)u;assert(!strcmp(font,"Native")&&cp=='B');*advance=600;return mulib_strbuf_append(out,"M 0 0 L 500 0 L 250 700 Z");}
static void paleography(void){
    mulib_paleography_options_t o={0};mulib_paleography_error_t e;mulib_paleography_t *c;mulib_paleography_glyph_t g={"triangle","M 0 0 L 600 0 L 300 700 Z",0xe001,650};mulib_xml_node_t *svg;char *text;size_t n;
    const char *source="assert(require('lpaleo')==lpaleo);lpaleo.define{name='curve',codepoint=57346,advance=600,path='M 0 0 Q 300 900 600 0 Z'};lpaleo.script('imaginary',{direction='boustrophedon',mapping={a='triangle',b='curve'}});lpaleo.native('nativeB',66,'Native')";
    o.native=native_font;c=mulib_paleography_create(&o,&e);if(!c)fprintf(stderr,"paleo: %s\n",e.message);assert(c&&mulib_paleography_define(c,&g,&e)&&!mulib_paleography_define(c,&g,&e));assert(mulib_paleography_extend(c,source,strlen(source),&e));assert(mulib_paleography_count(c)==3);
    svg=mulib_paleography_svg(c,"ab\nba",5,"imaginary",NULL,&e);if(!svg)fprintf(stderr,"paleo svg: %s\n",e.message);assert(svg);text=mulib_xml_serialize(svg,NULL);assert(text&&strstr(text,"<path"));free(text);mulib_xml_destroy(svg);
    svg=mulib_paleography_font(c,&e);assert(svg);mulib_xml_destroy(svg);
    text=mulib_paleography_css(c,&n,&e);assert(text&&n==strlen(text)&&strstr(text,"base64")&&strstr(text,"#Paleo\")"));free(text);
    text=mulib_paleography_package(c,&n,&e);assert(text&&strstr(text,"\\pgfpathcurveto"));free(text);
    text=mulib_paleography_encoding(c,&n,&e);assert(text&&strstr(text,"/triangle"));free(text);
    text=mulib_paleography_latex(c,"ab",2,"imaginary",&n,&e);assert(text&&!strcmp(text,"\\Paleoglyph{triangle}\\Paleoglyph{curve}"));free(text);
    svg=mulib_paleography_svg(c,"th",2,"elder-futhark",NULL,&e);assert(svg);mulib_xml_destroy(svg);
    assert(!mulib_paleography_latex(c,"th",2,"elder-futhark",NULL,&e));mulib_paleography_destroy(c);
}
static void boundaries(void) {
    mulib_xml_error_t e;
    mulib_tbl_t *table=mulib_tbl_parse(NULL,0,NULL,&e);
    mulib_tbl_fields_t fields={0};
    mulib_eqn_t *eq;
    mulib_chem_formula_t *f;
    char *text;
    size_t i,length;
    double number=999;
    assert(table&&mulib_tbl_validate(table,&e));
    for(i=0;i<3;++i) { static const char *const formats[]={"csv","pipe","latex"};text=mulib_tbl_render(table,formats[i],&length,&e);assert(text&&!length&&!*text);free(text); }
    assert(mulib_tbl_split(NULL,0,&fields,&e)&&!fields.count);mulib_tbl_fields_free(&fields);
    mulib_xml_destroy(table);table=mulib_tbl_parse(NULL,0,"csv",&e);assert(table&&!mulib_tbl_rows(table));mulib_xml_destroy(table);
    {
        const char *const values[]={"a|b\\c","quoted \"value\"\nnext"};
        table=mulib_tbl_create();assert(table&&mulib_tbl_add_row(table,values,2,0,NULL));
        text=mulib_tbl_render(table,"csv",&length,&e);assert(text);
        {mulib_tbl_t *roundtrip=mulib_tbl_parse(text,length,"csv",&e);char *cell;assert(roundtrip);cell=mulib_dsdom_text(mulib_tbl_cell(roundtrip,0,1),NULL);assert(cell&&!strcmp(cell,values[1]));free(cell);mulib_xml_destroy(roundtrip);}free(text);
        assert(!mulib_tbl_render(table,"pipe",&length,&e)&&!length&&e.message);mulib_xml_destroy(table);
        table=mulib_tbl_create();assert(table&&mulib_tbl_add_row(table,values,1,0,NULL));text=mulib_tbl_render(table,"pipe",&length,&e);assert(text);
        {mulib_tbl_t *roundtrip=mulib_tbl_parse(text,length,"pipe",&e);char *cell;assert(roundtrip);cell=mulib_dsdom_text(mulib_tbl_cell(roundtrip,0,0),NULL);assert(cell&&!strcmp(cell,values[0]));free(cell);mulib_xml_destroy(roundtrip);}free(text);
        assert(mulib_xml_set_attr(mulib_tbl_cell(table,0,0),"colspan","2")&&!mulib_tbl_validate(table,&e));assert(!mulib_tbl_add_row(table,values,1,0,NULL));mulib_xml_destroy(table);
    }
    assert(!mulib_tbl_parse("| a |\n| --- |\n| --- |",sizeof("| a |\n| --- |\n| --- |")-1,NULL,&e));
    eq=mulib_eqn_parse(NULL,NULL,0,&e);assert(eq);text=mulib_eqn_latex(eq,&length,&e);assert(text&&!length);free(text);mulib_eqn_destroy(eq);
    eq=EQN(NULL,"false and missing or (false -> missing)",&e);assert(eq&&mulib_eqn_eval(eq,NULL,NULL,&number,&e)&&number==1);mulib_eqn_destroy(eq);
    eq=EQN(NULL,"1.25e+3 / 5",&e);assert(eq&&mulib_eqn_eval(eq,NULL,NULL,&number,&e)&&number==250);text=mulib_eqn_latex(eq,NULL,&e);assert(text&&strstr(text,"\\times 10^{+3}"));free(text);mulib_eqn_destroy(eq);
    eq=EQN(NULL,"1/0",&e);assert(eq&&!mulib_eqn_eval(eq,NULL,NULL,&number,&e)&&number==250);mulib_eqn_destroy(eq);
    assert(!EQN(NULL,"1e99999",&e)&&!EQN(NULL,"sqrt()",&e)&&!EQN(NULL,"\\unknown",&e));
    { char chain[258];for(i=0;i<128;++i){chain[2*i]='x';chain[2*i+1]='+';}chain[256]='x';chain[257]=0;assert(!mulib_eqn_parse(NULL,chain,257,&e)&&e.message); }
    f=formula("H1000000000000");assert(mulib_chem_count(f,1)==UINT64_C(1000000000000));mulib_chem_destroy(f);
    assert(!mulib_chem_parse("H1000000000001",14,&e));assert(!mulib_chem_parse("(H10000000)1000000",18,&e));
    assert(mulib_chem_element(118)&&!strcmp(mulib_chem_element(118)->symbol,"Og")&&!mulib_chem_element(0)&&!mulib_chem_find_element("Xx"));
    {
        static const char *const sources[]={"KMnO4","HCl","KCl","MnCl2","Cl2","H2O"};
        static const uint64_t expected[]={2,16,2,2,5,8};
        mulib_chem_formula_t *owned[6];const mulib_chem_formula_t *species[6];uint64_t result[6];
        for(i=0;i<6;++i)species[i]=owned[i]=formula(sources[i]);
        assert(mulib_chem_balance(species,2,4,result,&e));for(i=0;i<6;++i){assert(result[i]==expected[i]);mulib_chem_destroy(owned[i]);}
    }
    {
        mulib_chem_formula_t *a=formula("H2"),*b=formula("O2"),*c=formula("H2O"),*d=formula("H2O2");
        const mulib_chem_formula_t *species[]={a,b,c,d};uint64_t result[4]={99,99,99,99};
        assert(!mulib_chem_balance(species,2,2,result,&e)&&result[0]==99);
        assert(!mulib_chem_balance(species,1,1,result,&e)&&result[0]==99);
        mulib_chem_destroy(a);mulib_chem_destroy(b);mulib_chem_destroy(c);mulib_chem_destroy(d);
    }
    {
        const char *bad="lcharts.register('bad',function() return {tag='svg',attrs={onload='bad'}} end)";
        mulib_chart_error_t ce;mulib_chart_t *c=mulib_chart_create(NULL,&ce);mulib_json_t *data=JSON("[1]");mulib_xml_node_t *svg;
        assert(c&&data&&mulib_chart_extend(c,bad,strlen(bad),&ce)&&!mulib_chart_render(c,"bad",data,NULL,&ce));
        (void)mulib_mune_collect(mulib_chart_lua(c));svg=mulib_chart_render(c,"pie",data,NULL,&ce);assert(svg);text=mulib_xml_serialize(svg,NULL);assert(text&&strstr(text,"<circle")&&!strstr(text,"<path"));free(text);mulib_xml_destroy(svg);
        mulib_json_destroy(data);data=JSON("[{\"x\":1,\"y\":-2},{\"x\":3,\"y\":4}]");assert(data);svg=mulib_chart_render(c,"scatter",data,NULL,&ce);assert(svg);mulib_xml_destroy(svg);mulib_json_destroy(data);
        data=JSON("[-1,2]");assert(data&&!mulib_chart_render(c,"pie",data,NULL,&ce));svg=mulib_chart_render(c,"bar",data,NULL,&ce);assert(svg);mulib_chart_destroy(c);assert(mulib_xml_name(svg));mulib_xml_destroy(svg);mulib_json_destroy(data);
    }
    {
        mulib_paleography_t *p=mulib_paleography_create(NULL,NULL);mulib_paleography_glyph_t glyph={"invalid","M 0 0 L 1000000.1 0",0,600};mulib_xml_node_t *svg;
        assert(p&&!mulib_paleography_define(p,&glyph,NULL));glyph.path="m 0 0";assert(!mulib_paleography_define(p,&glyph,NULL));glyph.path="L 0 0";assert(!mulib_paleography_define(p,&glyph,NULL));glyph.path="";
        assert(mulib_paleography_define(p,&glyph,NULL)&&mulib_paleography_glyph(p,"invalid")->scalar==0xe000);
        assert(!mulib_paleography_latex(p,"invalid",7,"identity",NULL,NULL));
        svg=mulib_paleography_svg(p,NULL,0,NULL,NULL,NULL);assert(svg);mulib_xml_destroy(svg);
        for(i=1;i<256;++i){char name[32];(void)snprintf(name,sizeof name,"glyph%zu",i);glyph.name=name;glyph.scalar=(uint32_t)(0xe000+i);assert(mulib_paleography_define(p,&glyph,NULL));}
        assert(!mulib_paleography_encoding(p,&length,NULL)&&length==0);mulib_paleography_destroy(p);
    }
}
static void hostile(void) {
    static const char *const seeds[]={"| one | `a|b` |", "sqrt(x^2)+frac(1,2)","[Fe(CN)6]^3-"};
    size_t kind,i;char source[64];mulib_xml_error_t e;
    for(kind=0;kind<3;++kind)for(i=0;i<strlen(seeds[kind]);++i) {
        size_t n=strlen(seeds[kind]);memcpy(source,seeds[kind],n);source[i]=i%3==0?(char)0xff:i%3==1?'\\':'}';
        if(kind==0){mulib_tbl_fields_t f={0};(void)mulib_tbl_split(source,n,&f,&e);mulib_tbl_fields_free(&f);}
        else if(kind==1){mulib_eqn_t *eq=mulib_eqn_parse(NULL,source,n,&e);assert(eq?!e.message:e.message!=NULL);mulib_eqn_destroy(eq);eq=mulib_eqn_parse(NULL,seeds[kind],i,&e);mulib_eqn_destroy(eq);}
        else {mulib_chem_formula_t *f=mulib_chem_parse(source,n,&e);assert(f?!e.message:e.message!=NULL);mulib_chem_destroy(f);f=mulib_chem_parse(seeds[kind],i,&e);mulib_chem_destroy(f);}
    }
}
static mulib_paleography_t *export_paleo(int native) {
    mulib_paleography_options_t options={0};
    mulib_paleography_glyph_t triangle={"triangle","M 0.00001 0 L 600 0 L 300 700 Z",0xe001,650};
    const char source[]="lpaleo.define{name='curve',codepoint=57346,advance=650,path='M 0 0 Q 300 900 600 0 Z'};lpaleo.script('imaginary',{mapping={a='triangle',b='curve'}})";
    mulib_paleography_t *p;
    if(native)options.native_font="Latin Modern Roman";
    p=mulib_paleography_create(&options,NULL);assert(p&&mulib_paleography_define(p,&triangle,NULL)&&mulib_paleography_extend(p,source,sizeof source-1,NULL));
    return p;
}
static void emit(const char *format) {
    char *text=NULL;mulib_xml_node_t *node=NULL;
    if(!strncmp(format,"table-",6)) {
        const char source[]="| A & B | Value |\n| :--- | ---: |\n| a < b | 42 |\n";
        mulib_tbl_t *t=mulib_tbl_parse(source,sizeof source-1,NULL,NULL);assert(t);text=mulib_tbl_render(t,format+6,NULL,NULL);mulib_xml_destroy(t);
    } else if(!strncmp(format,"eqn-",4)) {
        mulib_eqn_t *eq=EQN(NULL,"sqrt(x^2)+frac(1,2) >= alpha",NULL);assert(eq);
        if(!strcmp(format+4,"mathml"))node=mulib_eqn_mathml(eq,1,NULL);else text=mulib_eqn_latex(eq,NULL,NULL);mulib_eqn_destroy(eq);
    } else if(!strncmp(format,"chem-",5)) {
        mulib_chem_formula_t *f=formula("[Fe(CN)6]^3-(aq)");if(!strcmp(format+5,"mathml"))node=mulib_chem_mathml(f,NULL);else text=mulib_chem_latex(f,NULL,NULL);mulib_chem_destroy(f);
    } else if(!strncmp(format,"chart-",6)) {
        mulib_chart_t *c=mulib_chart_create(NULL,NULL);mulib_json_t *data=JSON("[{\"label\":\"A & B\",\"value\":30},{\"label\":\"C\",\"value\":70}]");assert(c&&data);node=mulib_chart_render(c,format+6,data,NULL,NULL);mulib_chart_destroy(c);mulib_json_destroy(data);
    } else if(!strncmp(format,"paleo-",6)) {
        mulib_paleography_t *p=export_paleo(!strcmp(format+6,"native-package"));
        if(!strcmp(format+6,"svg"))node=mulib_paleography_svg(p,"ab",2,"imaginary",NULL,NULL);
        else if(!strcmp(format+6,"font"))node=mulib_paleography_font(p,NULL);
        else if(!strcmp(format+6,"css"))text=mulib_paleography_css(p,NULL,NULL);
        else if(strstr(format+6,"package"))text=mulib_paleography_package(p,NULL,NULL);
        else if(!strcmp(format+6,"encoding"))text=mulib_paleography_encoding(p,NULL,NULL);
        else if(!strcmp(format+6,"native-latex")) { mulib_paleography_destroy(p);p=export_paleo(1);text=mulib_paleography_latex(p,"abX",3,"imaginary",NULL,NULL); }
        else text=mulib_paleography_latex(p,"ab",2,"imaginary",NULL,NULL);
        mulib_paleography_destroy(p);
    }
    if(node){text=mulib_xml_serialize(node,NULL);mulib_xml_destroy(node);}assert(text);fputs(text,stdout);free(text);
}
int main(int argc,char **argv){
    if(argc==3&&!strcmp(argv[1],"--emit")){emit(argv[2]);return 0;}
    tables();equations();chemistry();charts();paleography();boundaries();hostile();puts("notation: OK");return 0;
}
