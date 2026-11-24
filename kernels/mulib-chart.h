/** @file mulib-chart.h
 * @brief Extensible Lua-driven SVG charts, with automatically loaded lcharts.
 * Define MULIB_IMPL_CHART plus MUNE, JSON, XML, STRBUF and their dependencies.
 */
#ifndef MULIB_CHART_H
#define MULIB_CHART_H
#include "mulib-mune.h"
#include "mulib-json.h"
#include "mulib-xml.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_chart mulib_chart_t;
typedef struct mulib_chart_error {size_t offset;char message[256];} mulib_chart_error_t;
/** @brief Independent restricted Mune state. lcharts is installed automatically.
 * Runtime limits select Mune defaults when NULL/zero. No filesystem loading. */
mulib_chart_t *mulib_chart_create(const mulib_mune_runtime_options_t *,mulib_chart_error_t *);
/** @brief Destroy context/state; NULL safe, no-op during an active operation. */
void mulib_chart_destroy(mulib_chart_t *);
/** @brief Register/replace chart kinds with lcharts.register(name,function(data,opts)).
 * Source <=1 MiB. Returns 1/0. Lua side effects survive script errors. */
int mulib_chart_extend(mulib_chart_t *,const char *,size_t,mulib_chart_error_t *);
/** @brief Render builtin pie/donut/bar/line/scatter or an arbitrary Lua plugin.
 * data/options are borrowed JSON; options NULL means {}. Output is an owned SVG
 * element. NULL on error. Bounds: 100000 JSON/markup nodes, depth64, 8 MiB SVG.
 * Lua chart constructors use {tag,attrs,children} nodes. No Docstrings coupling. */
mulib_xml_node_t *mulib_chart_render(mulib_chart_t *,const char *kind,const mulib_json_t *data,const mulib_json_t *options,mulib_chart_error_t *);
/** @brief Borrow the state for explicit host bindings; never destroy it yourself. */
mulib_mune_state_t *mulib_chart_lua(mulib_chart_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CHART) && !defined(MULIB_CHART_IMPLEMENTED)
#define MULIB_CHART_IMPLEMENTED
#include "mulib-mune-markup.inc"
#include "mulib-chart-data.inc"
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_chart {mulib_mune_state_t *state;int busy;};
static int mulib_chart_error_(mulib_chart_error_t *e,size_t offset,const char *message){if(e){e->offset=offset;(void)snprintf(e->message,sizeof e->message,"%s",message?message:"");}return 0;}
mulib_chart_t *mulib_chart_create(const mulib_mune_runtime_options_t *options,mulib_chart_error_t *e){
    mulib_chart_t *c=(mulib_chart_t *)calloc(1,sizeof *c);mulib_mune_error_t me={0};(void)mulib_chart_error_(e,0,NULL);
    if(!c)goto fail;
    c->state=mulib_munexml_state_(options);if(!c->state)goto fail;
    if(!mulib_munexml_bootstrap_(c->state,mulib_chart_source_,sizeof mulib_chart_source_/sizeof mulib_chart_source_[0],"lcharts",&me)){(void)mulib_chart_error_(e,me.offset,me.message?me.message:"lcharts bootstrap failure");mulib_chart_destroy(c);return NULL;}return c;
fail:(void)mulib_chart_error_(e,0,"chart/Mune allocation failure");mulib_chart_destroy(c);return NULL;
}
void mulib_chart_destroy(mulib_chart_t *c){if(c&&!c->busy){c->busy=1;mulib_mune_state_destroy(c->state);free(c);}}
mulib_mune_state_t *mulib_chart_lua(mulib_chart_t *c){return c&&!c->busy?c->state:NULL;}
int mulib_chart_extend(mulib_chart_t *c,const char *source,size_t length,mulib_chart_error_t *e){
    mulib_mune_result_t *r=NULL;mulib_mune_error_t me;int ok;(void)mulib_chart_error_(e,0,NULL);
    if(!c||c->busy||length>1024*1024||!mulib_xml_valid_text(source,length))return mulib_chart_error_(e,0,"invalid/reentrant/oversized chart extension");
    c->busy=1;ok=!mulib_mune_run_source(c->state,source,length,&r,&me);if(!ok)(void)mulib_chart_error_(e,me.offset,me.message);mulib_mune_result_destroy(r);c->busy=0;return ok;
}
mulib_xml_node_t *mulib_chart_render(mulib_chart_t *c,const char *kind,const mulib_json_t *data,const mulib_json_t *options,mulib_chart_error_t *e){
    mulib_mune_value_t module,fn,args[3];mulib_mune_result_t *roots=NULL,*result=NULL;mulib_mune_error_t me;mulib_xml_node_t *svg=NULL;size_t budget=100000;
    (void)mulib_chart_error_(e,0,NULL);if(!c||c->busy||!kind||!*kind||strlen(kind)>63||!data)return (void)mulib_chart_error_(e,0,"invalid/reentrant chart arguments"),NULL;
    c->busy=1;roots=mulib_mune_result_create(c->state);if(!roots)goto memory;
    if(mulib_mune_make_string(c->state,kind,strlen(kind),&args[0])||!mulib_mune_result_push(roots,args[0])||
        !mulib_munexml_json_(c->state,data,&args[1],0,&budget)||!mulib_mune_result_push(roots,args[1]))goto memory;
    if(options){if(!mulib_munexml_json_(c->state,options,&args[2],0,&budget))goto memory;}else if(mulib_mune_make_table(c->state,&args[2]))goto memory;
    if(!mulib_mune_result_push(roots,args[2])||mulib_mune_get_global(c->state,"lcharts",&module)||!mulib_munexml_get_(c->state,module,"render",&fn))goto memory;
    if(mulib_mune_call(c->state,fn,args,3,&result,&me)){(void)mulib_chart_error_(e,me.offset,me.message);goto done;}
    svg=mulib_munexml_svg_(c->state,mulib_mune_result_get(result,0));if(!svg)(void)mulib_chart_error_(e,0,"invalid/cyclic/oversized SVG plugin result or allocation failure");goto done;
memory:(void)mulib_chart_error_(e,0,"chart JSON bridge allocation/depth/node limit");
done:mulib_mune_result_destroy(result);mulib_mune_result_destroy(roots);c->busy=0;return svg;
}
#ifdef __cplusplus
}
#endif
#endif
