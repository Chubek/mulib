#define MULIB_IMPL_COMPILER_CACHE
#define MULIB_IMPL_COMPILER_DRIVER
#define MULIB_IMPL_COMPILER_TEST
#define MULIB_IMPL_PASSMANAGER
#define MULIB_IMPL_DIAGNOSTIC
#define MULIB_IMPL_IMPORTGRAPH
#define MULIB_IMPL_MODULE_RESOLVER
#define MULIB_IMPL_SOURCE_MANAGER
#define MULIB_IMPL_SOURCE_MAP
#define MULIB_IMPL_STACKMAP
#include "mulib-compiler-cache.h"
#include "mulib-compiler-driver.h"
#include "mulib-compiler-test.h"
#include "mulib-passmanager.h"
#include "mulib-diagnostic.h"
#include "mulib-importgraph.h"
#include "mulib-module-resolver.h"
#include "mulib-source-manager.h"
#include "mulib-source-map.h"
#include "mulib-stackmap.h"
#include <assert.h>

static int stage(void *program,void *context) {++*(int *)program;return *(int *)context;}
int main(void) {
    mulib_compiler_cache_t cache={0};char bytes[]={'a',0,'z'};const void *data;size_t length;
    assert(mulib_compiler_cache_put(&cache,7,bytes,3));bytes[0]='b';
    assert(mulib_compiler_cache_get(&cache,7,&data,&length)&&length==3&&((const char *)data)[0]=='a');
    assert(mulib_compiler_cache_put(&cache,7,data,length)&&cache.count==1);
    assert(mulib_compiler_cache_put(&cache,7,NULL,0));
    assert(mulib_compiler_cache_get(&cache,7,&data,&length)&&length==0);
    assert(!mulib_compiler_cache_get(&cache,8,&data,&length));mulib_compiler_cache_free(&cache);
    mulib_compiler_driver_t driver={0};int good=1,bad=0,program=0;size_t failed;
    assert(mulib_compiler_driver_add(&driver,"first",stage,&good));
    assert(mulib_compiler_driver_add(&driver,"fail",stage,&bad));
    assert(mulib_compiler_driver_add(&driver,"last",stage,&good));
    assert(!mulib_compiler_driver_run(&driver,&program,&failed)&&failed==1&&program==2);
    bad=1;assert(mulib_compiler_driver_run(&driver,&program,&failed)&&failed==3&&program==5);
    mulib_compiler_driver_free(&driver);
    mulib_passmanager_t passes={0};passes.valid_analyses=7;bad=0;program=0;
    assert(mulib_passmanager_add(&passes,stage,&good,3)&&mulib_passmanager_add(&passes,stage,&bad,1));
    assert(!mulib_passmanager_run(&passes,&program,&failed)&&failed==1&&program==2&&passes.valid_analyses==1);
    mulib_passmanager_free(&passes);
    mulib_compiler_test_t checks={0};
    assert(mulib_compiler_test_check(&checks,1,"success")&&mulib_compiler_test_ok(&checks));
    assert(!mulib_compiler_test_check(&checks,0,"failure")&&!mulib_compiler_test_ok(&checks));
    assert(checks.passed==1&&checks.failed==1&&!strcmp(checks.last_failure,"failure"));
    checks.passed=SIZE_MAX;assert(!mulib_compiler_test_check(&checks,1,"overflow")&&checks.passed==SIZE_MAX);
    mulib_diagnostic_list_t diag={0};char message[]="error";
    assert(mulib_diagnostic_add(&diag,12,MULIB_DIAG_ERROR,0,1,2,message));message[0]='x';
    assert(diag.errors==1&&!strcmp(diag.items[0].message,"error"));
    assert(!mulib_diagnostic_add(&diag,0,(mulib_diagnostic_severity_t)-1,0,0,0,"bad"));
    assert(!mulib_diagnostic_add(&diag,0,MULIB_DIAG_NOTE,0,SIZE_MAX,1,"bad")&&diag.count==1);
    mulib_diagnostic_free(&diag);
    mulib_importgraph_t imports={0};imports.nodes=4;size_t order[4];
    assert(mulib_importgraph_add(&imports,2,0)&&mulib_importgraph_add(&imports,0,3));
    assert(mulib_importgraph_add(&imports,2,0)&&imports.count==2);
    assert(mulib_importgraph_order(&imports,order,4)&&order[0]==1&&order[1]==2&&order[2]==0&&order[3]==3);
    assert(mulib_importgraph_add(&imports,3,2));order[0]=77;
    assert(!mulib_importgraph_order(&imports,order,4)&&order[0]==77);mulib_importgraph_free(&imports);
    mulib_module_resolver_t modules={0};char path[]="src/a.mu";
    assert(mulib_module_resolver_add(&modules,"a",path));path[0]='x';
    assert(!strcmp(mulib_module_resolver_get(&modules,"a"),"src/a.mu"));
    assert(!mulib_module_resolver_add(&modules,"a","new")&&!mulib_module_resolver_get(&modules,"A"));
    mulib_module_resolver_free(&modules);
    mulib_source_manager_t source={0};size_t line,column;
    assert(mulib_source_manager_set(&source,"a\0b\r\n\n",6));
    assert(source.lines==3&&source.length==6);
    assert(mulib_source_manager_position(&source,4,&line,&column)&&line==1&&column==5);
    assert(mulib_source_manager_position(&source,5,&line,&column)&&line==2&&column==1);
    assert(mulib_source_manager_position(&source,6,&line,&column)&&line==3&&column==1);
    assert(!mulib_source_manager_position(&source,7,&line,&column));
    assert(mulib_source_manager_set(&source,source.text+2,3)&&source.length==3&&source.text[0]=='b');
    assert(!mulib_source_manager_set(&source,NULL,1)&&source.length==3);
    assert(mulib_source_manager_set(&source,NULL,0)&&source.lines==1);
    mulib_source_manager_free(&source);
    mulib_source_map_t map={0};mulib_source_map_entry_t entry;
    assert(mulib_source_map_add(&map,10,3,4,100)&&mulib_source_map_add(&map,20,1,5,0));
    assert(mulib_source_map_find(&map,12,&entry)&&entry.original==102&&entry.source_id==4&&entry.length==1);
    assert(!mulib_source_map_find(&map,13,&entry)&&!mulib_source_map_find(&map,9,&entry));
    assert(!mulib_source_map_add(&map,20,2,0,0)&&!mulib_source_map_add(&map,SIZE_MAX,1,0,0));
    mulib_source_map_free(&map);
    mulib_stackmap_t stack={0};mulib_stackmap_entry_t point;
    assert(mulib_stackmap_add(&stack,0,1,2)&&mulib_stackmap_add(&stack,10,4,8));
    assert(!mulib_stackmap_add(&stack,10,0,0)&&!mulib_stackmap_add(&stack,9,0,0));
    assert(mulib_stackmap_find(&stack,10,&point)&&point.pointer_registers==4&&point.pointer_spills==8);
    assert(!mulib_stackmap_find(&stack,11,&point));mulib_stackmap_free(&stack);
    return 0;
}
