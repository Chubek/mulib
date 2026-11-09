#define MULIB_IMPL_DOCSTRING
#define MULIB_IMPL_DSDOM
#define MULIB_IMPL_TAS
#define MULIB_IMPL_XTAS
#define MULIB_IMPL_PANDOCAST
#define MULIB_IMPL_GVIZZ
#define MULIB_IMPL_DYEGRAM
#define MULIB_IMPL_XML
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_CSS
#define MULIB_IMPL_JSON
#define MULIB_IMPL_YAML
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_TERMSYN
#define MULIB_IMPL_EDBUF
#define MULIB_IMPL_TERMSTYLE
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TERMINFO
#include "mulib-docstring.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mulib_docstril_t *compile(mulib_docstrings_t *c, const char *source, const char *format) {
    mulib_docstrings_error_t e;
    mulib_docstril_t *d = mulib_docstrings_compile(c,source,strlen(source),format,&e);
    if (!d) fprintf(stderr,"compile %s:%zu:%zu: %s (%d)\n",e.source,e.line,e.column,e.message,(int)e.code);
    assert(d && !e.code); return d;
}
static void rejected(mulib_docstrings_t *c, const char *source, mulib_docstrings_status_t status) {
    mulib_docstrings_error_t e;
    mulib_docstril_t *d = mulib_docstrings_compile(c,source,strlen(source),NULL,&e);
    if (d || e.code != status) fprintf(stderr,"expected %d, got %d: %s for %s\n",(int)status,(int)e.code,e.message,source);
    assert(!d && e.code == status && *e.message);
}
static int events(void *user, const mulib_xtas_event_t *event) {
    size_t *count = (size_t *)user;
    assert(event->node && event->depth <= MULIB_XML_DEPTH_LIMIT); ++*count; return 1;
}
static int cancel(void *user, const mulib_xtas_event_t *event) { (void)user; (void)event; return 0; }
static void streams(void) {
    mulib_docstril_t *d = mulib_docstril_create(), *copy;
    mulib_dsdom_t *p = mulib_dsdom_element("paragraph","A < B & C");
    mulib_tas_t *t;
    mulib_xml_error_t e;
    char *a, *b;
    size_t count = 0;
    assert(d && p && mulib_xml_set_attr(p,"class","a b") && mulib_xml_append(d,p));
    t = mulib_tas_from_dom(d); assert(t && mulib_tas_count(t) == 7);
    copy = mulib_tas_to_dom(t,&e); assert(copy && !e.message);
    a = mulib_xml_serialize(d,NULL); b = mulib_xml_serialize(copy,NULL); assert(a && b && !strcmp(a,b));
    assert(mulib_xtas_stream(d,events,&count) == 1 && count == 5);
    assert(mulib_xtas_stream(d,cancel,NULL) == 0 && mulib_xtas_stream(NULL,events,&count) == -1);
    free(a); free(b); mulib_xml_destroy(copy); mulib_tas_destroy(t);
    t = mulib_tas_create(); assert(t);
    assert(mulib_tas_push(t,MULIB_TAS_OPEN,"x",NULL));
    assert(mulib_tas_push(t,MULIB_TAS_ATTRIBUTE,"a","1") && mulib_tas_push(t,MULIB_TAS_ATTRIBUTE,"a","2"));
    assert(!mulib_tas_to_dom(t,&e) && e.message && e.offset == 2);
    assert(!mulib_tas_push(t,MULIB_TAS_TEXT,"wrong","text"));
    mulib_tas_destroy(t); mulib_xml_destroy(d);
    {
        mulib_dsdom_t *root = mulib_dsdom_element("x",NULL), *last = root, *child;
        unsigned i;
        assert(root);
        for (i = 1; i < MULIB_XML_DEPTH_LIMIT; ++i) {
            child = mulib_dsdom_element("x",NULL); assert(child && mulib_xml_append(last,child)); last = child;
        }
        copy = mulib_dsdom_clone(root); assert(copy); mulib_xml_destroy(copy);
        t = mulib_tas_from_dom(root); assert(t); copy = mulib_tas_to_dom(t,&e); assert(copy);
        mulib_xml_destroy(copy); mulib_tas_destroy(t);
        child = mulib_dsdom_element("x",NULL); assert(child && mulib_xml_append(last,child));
        assert(!mulib_dsdom_clone(root) && !mulib_tas_from_dom(root) && mulib_xtas_stream(root,events,&count) == -1);
        mulib_xml_destroy(root);
    }
}
static void language(void) {
    const char source[] =
        ".file test.c\n.define NAME MuLIB\n.define hello(who) **Hello**, ${who}!\n"
        ".macro box(value)\n> ${value}\n.endmacro\n"
        "# {{NAME}}\n.toc\n# {{NAME}}\n\n"
        "{{hello(World)}} __under__ *italic* ~~gone~~ ^up^ ~down~ `{{NAME}}` \\*literal*\n\n"
        "@span[class=\"accent\" title='a & b']{nested @strong{bold}}\n\n"
        "[site](https://example.org/) ![alt](picture.svg)\n\n"
        ".use box(quoted)\n"
        "- first\n- second\n\n1. ordered\n2. list\n\n"
        "| A | B |\n| --- | :---: |\n| `a|b` | @code{x|y} |\n\n"
        ".code c\nint answer = 42; // color\n.endcode\n"
        ".graph\nA: Parse\nB: Render\nA -> B: IR\nB -> B: repeat\n.endgraph\n"
        ".diagram\nClient -> Server: request & data\nServer -> Client: response\n.enddiagram\n"
        ".param arg documentation\n.index zebra\n.index alpha\n.indices\n"
        ".style css\nheading {color:navy} .accent {font-weight:bold}\n.endstyle\n";
    mulib_docstrings_t *c = mulib_docstrings_create(NULL);
    mulib_docstril_t *d = compile(c,source,NULL), *copy;
    mulib_docstrings_error_t e;
    mulib_xml_error_t xe;
    mulib_json_t *j;
    mulib_tas_t *t;
    char *xml = mulib_docstrings_render(d,"xml",NULL,&e), *html, *text;
    size_t length;
    assert(xml && strstr(xml,"<strong>Hello</strong>") && strstr(xml,"<code>{{NAME}}</code>"));
    assert(strstr(xml,"id=\"mulib\"") && strstr(xml,"id=\"mulib-2\"") && strstr(xml,"header=\"true\""));
    assert(strstr(xml,"class=\"type\"") && strstr(xml,"<svg") && !strstr(xml,"<stylesheet"));
    assert(strstr(xml,"style=\"color:navy;") && strstr(xml,"<underline>under</underline>"));
    html = mulib_docstrings_render(d,"html",&length,&e); assert(html && length == strlen(html));
    assert(strstr(html,"<table>") && strstr(html,"<th") && strstr(html,"<pre") && strstr(html,"<svg"));
    copy = mulib_xml_parse(xml,strlen(xml),&xe); assert(copy);
    assert(mulib_docstril_validate(mulib_xml_root(copy),0,0,&xe)); mulib_xml_destroy(copy);
    text = mulib_docstrings_render(d,"pandoc",NULL,&e); assert(text);
    j = mulib_json_parse(text,strlen(text),NULL); assert(j && mulib_json_get(j,"blocks"));
    assert(strstr(text,"\"Table\"") && strstr(text,"\"RawBlock\"") && strstr(text,"pandoc-api-version"));
    mulib_json_destroy(j); free(text); free(xml); free(html);
    t = mulib_tas_from_dom(d); copy = mulib_tas_to_dom(t,&xe); assert(copy); mulib_xml_destroy(copy); mulib_tas_destroy(t);
    mulib_xml_destroy(d);
    rejected(c,".unknown x",MULIB_DOCSTRINGS_SYNTAX);
    rejected(c,"@unknown{x}",MULIB_DOCSTRINGS_SYNTAX);
    rejected(c,".code c\nunclosed",MULIB_DOCSTRINGS_SYNTAX);
    rejected(c,".define loop {{loop}}\n{{loop}}",MULIB_DOCSTRINGS_LIMIT);
    rejected(c,".define f(x) ${x}\n{{f(a,b)}}",MULIB_DOCSTRINGS_SYNTAX);
    rejected(c,"{{missing}}",MULIB_DOCSTRINGS_SYNTAX);
    rejected(c,"# A {#same}\n# B {#same}",MULIB_DOCSTRINGS_INVALID);
    rejected(c,"| A | B |\n| one |",MULIB_DOCSTRINGS_SYNTAX);
    rejected(c,".include markdown file.md",MULIB_DOCSTRINGS_POLICY);
    rejected(c,"{{exec(nope)}}",MULIB_DOCSTRINGS_POLICY);
    rejected(c,"[bad](javascript:alert)",MULIB_DOCSTRINGS_SYNTAX);
    d = mulib_docstrings_compile(c,NULL,0,NULL,&e); assert(d && !e.code); mulib_xml_destroy(d);
    assert(!mulib_docstrings_compile(c,"a\0b",3,NULL,&e) && e.code == MULIB_DOCSTRINGS_INVALID);
    mulib_docstrings_destroy(c);
}
static int loader(void *user, const char *path, const char *format, mulib_strbuf_t *out) {
    (void)user; (void)format;
    if (!strcmp(path,"one.md")) return mulib_strbuf_append(out,"# Embedded Markdown\n\n**bold** and {{not_a_macro}}\n");
    if (!strcmp(path,"two.adoc")) return mulib_strbuf_append(out,"= Embedded AsciiDoc\n\n*bold*\n\n[source,c]\n----\nint x;\n----\n\n|===\n| A | B\n| 1 | 2\n|===\n");
    if (!strcmp(path,"cycle")) return mulib_strbuf_append(out,".include docstrings cycle");
    if (!strcmp(path,"broken")) return mulib_strbuf_append(out,".unknown");
    if (!strcmp(path,"style.yaml")) return mulib_strbuf_append(out,"heading:\n  color: green\ntoc link:\n  color: blue\n");
    return 0;
}
static int command(void *user, const char *text, mulib_strbuf_t *out) {
    (void)user; assert(!strcmp(text,"example, with args")); return mulib_strbuf_append(out,"literal <output> {{not_expanded}}");
}
static void embeddings(void) {
    mulib_docstrings_options_t o = {0};
    mulib_docstrings_t *c;
    mulib_docstril_t *d;
    mulib_docstrings_error_t e;
    char *text;
    o.load = loader; o.exec = command; c = mulib_docstrings_create(&o); assert(c);
    d = compile(c,".include markdown one.md\n.include asciidoc two.adoc\n.toc\n.stylesheet yaml style.yaml\n\n{{exec(\"example, with args\")}}",NULL);
    text = mulib_dsdom_text(d,NULL); assert(text && strstr(text,"Embedded Markdown") && strstr(text,"{{not_a_macro}}") && strstr(text,"{{not_expanded}}"));
    free(text);
    text = mulib_xml_serialize(d,NULL); assert(text && strstr(text,"style=\"color:green;\"") && strstr(text,"style=\"color:blue;\""));
    free(text); mulib_xml_destroy(d);
    rejected(c,".include docstrings cycle",MULIB_DOCSTRINGS_SYNTAX);
    rejected(c,".include markdown missing",MULIB_DOCSTRINGS_IO);
    assert(!mulib_docstrings_compile(c,".include docstrings broken",26,NULL,&e) && !strcmp(e.source,"broken"));
    mulib_docstrings_destroy(c);
}
static void lua(void) {
    const char extension[] =
        "assert(io == nil and os == nil and package == nil and debug == nil)\n"
        "assert(require('ldocstr') == ldocstr and require('ldstr-style') == ldocstr.style)\n"
        "ldocstr.element('kbd', function(content, attrs) attrs.inline='true'; return ldocstr.Element('kbd',content,attrs) end)\n"
        "ldocstr.directive('todo', function(text) return ldocstr.Para({ldocstr.Str('TODO: ' .. text)}) end)\n"
        "ldocstr.macro('upper', function(text) return ldocstr.Str(string.upper(text)) end)\n";
    mulib_docstrings_t *c = mulib_docstrings_create(NULL);
    mulib_docstrings_error_t e;
    mulib_docstril_t *d;
    char *text, *before, *after;
    mulib_mune_state_t *state;
    const char *script;
    assert(c && !mulib_docstrings_extend(c,extension,sizeof extension-1,&e));
    state = mulib_docstrings_lua(c,&e); assert(state); (void)mulib_mune_collect(state);
    d = compile(c,"# Title\n.toc\n\nPress @kbd[class=\"key\"]{Enter}. {{upper(hello)}} {{eval(6*7)}} {{eval('**literal**')}}\n.todo fix it\n",NULL);
    text = mulib_docstrings_render(d,"html",NULL,&e); assert(text && strstr(text,"<kbd") && strstr(text,"HELLO") && strstr(text,"42") && strstr(text,"**literal**")); free(text);
    script = "return { Str = function(el) if el.text == 'Enter' then return ldocstr.Str('Return') end end, Header = function(el) el.children={ldocstr.Str('Changed')} end }";
    assert(!mulib_docstrings_filter(c,&d,script,strlen(script),"walk",&e));
    text = mulib_dsdom_text(d,NULL); assert(text && strstr(text,"Return") && strstr(text,"Changed")); free(text);
    script = "return function(doc) local a=ldocstr.pandoc(doc); assert(a.blocks); assert(ldocstr.dom(doc).t == 'docstril'); assert(#ldocstr.events(doc)>0); assert(ldocstr.xml(doc):find('Changed')); doc.children[#doc.children+1]=ldocstr.Para('extra'); return doc end";
    assert(!mulib_docstrings_filter(c,&d,script,strlen(script),"document",&e));
    script = "return function(tokens) for _, t in ipairs(tokens) do if t.kind == 'text' and t.value == 'extra' then t.value = 'tokens' end end return tokens end";
    assert(!mulib_docstrings_filter(c,&d,script,strlen(script),"tokens",&e));
    script = "return function(events) for _, e in ipairs(events) do if e.kind == 'text' and e.text == 'tokens' then e.text = 'events' end end return events end";
    assert(!mulib_docstrings_filter(c,&d,script,strlen(script),"events",&e));
    before = mulib_xml_serialize(d,NULL);
    script = "return function(doc) doc.children={doc}; return doc end";
    assert(mulib_docstrings_filter(c,&d,script,strlen(script),"document",&e) != MULIB_DOCSTRINGS_OK);
    after = mulib_xml_serialize(d,NULL); assert(!strcmp(before,after)); free(after);
    script = "return {Str=function(el) error('filter failed') end}";
    assert(mulib_docstrings_filter(c,&d,script,strlen(script),NULL,&e) == MULIB_DOCSTRINGS_LUA);
    after = mulib_xml_serialize(d,NULL); assert(!strcmp(before,after)); free(before); free(after);
    script = "return ldocstr.style.filter({kbd={['font-weight']='bold'},['toc link']={color='blue'}})";
    assert(!mulib_docstrings_style(c,&d,script,strlen(script),"lua",&e));
    text = mulib_xml_serialize(d,NULL); assert(text && strstr(text,"style=\"color:blue;\"")); free(text);
    mulib_xml_destroy(d);
    d = compile(c,"return ldocstr.Doc({ldocstr.Header(1,'Lua'),ldocstr.Para('authored in Lua'),ldocstr.parse('embedded').children[1]})","lua");
    text = mulib_dsdom_text(d,NULL); assert(text && strstr(text,"authored in Lua") && strstr(text,"embedded")); free(text); mulib_xml_destroy(d);
    d = compile(c,".lua\nreturn ldocstr.Para('a Lua block')\n.endlua",NULL); mulib_xml_destroy(d);
    (void)mulib_mune_collect(state); mulib_docstrings_destroy(c);
}
static void hostile(void) {
    const char seed[] = "# Title\n.toc\n.define greet(x) @strong{${x}}\n{{greet(world)}} [link](https://example.org)\n| x | y |\n| --- | --- |\n| a | b |\n\n.graph\nA -> B: hi\n.endgraph\n";
    mulib_docstrings_options_t o = {0};
    mulib_docstrings_t *c;
    mulib_docstrings_error_t e;
    mulib_docstril_t *d;
    char bytes[sizeof seed];
    size_t i;
    o.steps = 10000; o.output_bytes = 32768; o.nodes = 1024;
    c = mulib_docstrings_create(&o); assert(c);
    for (i = 0; i < sizeof seed-1; ++i) {
        memcpy(bytes,seed,sizeof seed); bytes[i] = (char)(i%7 == 0 ? 0xff : i%3 == 0 ? '}' : '\\');
        d = mulib_docstrings_compile(c,bytes,sizeof seed-1,NULL,&e);
        assert(d ? e.code == MULIB_DOCSTRINGS_OK : e.code != MULIB_DOCSTRINGS_OK);
        mulib_xml_destroy(d);
        d = mulib_docstrings_compile(c,seed,i,NULL,&e);
        assert(d ? !e.code : e.code != MULIB_DOCSTRINGS_OK); mulib_xml_destroy(d);
    }
    mulib_docstrings_destroy(c);
}
static void styles_and_limits(void) {
    mulib_docstrings_t *c = mulib_docstrings_create(NULL);
    mulib_docstril_t *d = compile(c,"# Title\n\n@span[class=\"key\"]{styled}",NULL);
    mulib_docstrings_error_t e;
    mulib_docstrings_options_t o = {0};
    char *text, *before, *after;
    const char *style = "{\"heading\":{\"color\":\"red\"},\".key\":{\"font-weight\":\"bold\"}}";
    assert(!mulib_docstrings_style(c,&d,style,strlen(style),"json",&e));
    style = "paragraph:\n  margin: 4\n  padding: 2px\n";
    assert(!mulib_docstrings_style(c,&d,style,strlen(style),"yaml",&e));
    text = mulib_xml_serialize(d,NULL); assert(text && strstr(text,"font-weight:bold;") && strstr(text,"padding:2px;")); free(text);
    before = mulib_xml_serialize(d,NULL); style = "{\"*\":{\"color\":\"red;} bad {x:y\"}}";
    assert(mulib_docstrings_style(c,&d,style,strlen(style),"json",&e) == MULIB_DOCSTRINGS_SYNTAX);
    after = mulib_xml_serialize(d,NULL); assert(!strcmp(before,after)); free(before); free(after);
    mulib_xml_destroy(d); mulib_docstrings_destroy(c);
    o.nodes = 3; c = mulib_docstrings_create(&o); assert(c); rejected(c,"# A\n# B",MULIB_DOCSTRINGS_LIMIT); mulib_docstrings_destroy(c);
    memset(&o,0,sizeof o); o.source_bytes = 3; c = mulib_docstrings_create(&o); assert(c); rejected(c,"four",MULIB_DOCSTRINGS_LIMIT); mulib_docstrings_destroy(c);
    memset(&o,0,sizeof o); o.output_bytes = 16; c = mulib_docstrings_create(&o); assert(c); rejected(c,"long output here",MULIB_DOCSTRINGS_LIMIT); mulib_docstrings_destroy(c);
    memset(&o,0,sizeof o); o.depth = 65; assert(!mulib_docstrings_create(&o));
    memset(&o,0,sizeof o); o.lua.memory_bytes = 1;
    c = mulib_docstrings_create(&o); assert(c); rejected(c,"{{eval(1)}}",MULIB_DOCSTRINGS_MEMORY); mulib_docstrings_destroy(c);
}
static char *file_source(const char *directory, const char *name, size_t *length) {
    char path[4096]; FILE *file; long size; char *source;
    int n = snprintf(path,sizeof path,"%s/examples/%s",directory,name);
    assert(n > 0 && (size_t)n < sizeof path);
    file = fopen(path,"rb"); assert(file && !fseek(file,0,SEEK_END));
    size = ftell(file); assert(size >= 0 && size <= 1024*1024 && !fseek(file,0,SEEK_SET));
    source = (char *)malloc((size_t)size+1); assert(source);
    assert(fread(source,1,(size_t)size,file) == (size_t)size && !fclose(file));
    source[size] = 0; *length = (size_t)size; return source;
}
static void fixtures(const char *directory) {
    mulib_docstrings_t *c = mulib_docstrings_create(NULL);
    mulib_docstrings_error_t e;
    mulib_docstril_t *d;
    char *source, *text; size_t length;
    assert(c);
    source = file_source(directory,"keyboard.lua",&length);
    assert(!mulib_docstrings_extend(c,source,length,&e)); free(source);
    source = file_source(directory,"document.lua",&length);
    d = mulib_docstrings_compile(c,source,length,"lua",&e); free(source); assert(d);
    source = file_source(directory,"filter.lua",&length);
    assert(!mulib_docstrings_filter(c,&d,source,length,"walk",&e)); free(source);
    source = file_source(directory,"style.lua",&length);
    assert(!mulib_docstrings_style(c,&d,source,length,"lua",&e)); free(source);
    text = mulib_docstrings_render(d,"html",NULL,&e); assert(text && strstr(text,"Return") && strstr(text,"border:1px solid gray;"));
    free(text); mulib_xml_destroy(d); mulib_docstrings_destroy(c);
}
static void emit(const char *format) {
    const char source[] = "# Interoperable\n.toc\n\n**bold** *italic* __underline__ `code` [link](https://example.org)\n\n> quoted\n\n- one\n- two\n\n1. ordered\n\n| A | B |\n| --- | --- |\n| x | y |\n\n.code c\nint x = 42;\n.endcode\n.graph\nA -> B: IR\n.endgraph\n.diagram\nC -> S: message\n.enddiagram\n";
    mulib_docstrings_t *c = mulib_docstrings_create(NULL);
    mulib_docstril_t *d = compile(c,source,NULL);
    char *text = mulib_docstrings_render(d,format,NULL,NULL); assert(text); puts(text);
    free(text); mulib_xml_destroy(d); mulib_docstrings_destroy(c);
}
int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1],"--emit")) { emit(argv[2]); return 0; }
    streams(); language(); embeddings(); lua(); styles_and_limits(); hostile();
    if (argc == 3 && !strcmp(argv[1],"--fixtures")) fixtures(argv[2]);
    puts("docstring: OK"); return 0;
}
