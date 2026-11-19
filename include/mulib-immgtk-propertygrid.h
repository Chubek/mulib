/* MuLIB graphical extension: immgtk-propertygrid. C11, header-only. */
#ifndef MULIB_IMMGTK_PROPERTYGRID_H
#define MULIB_IMMGTK_PROPERTYGRID_H
#include "mulib-immgtk-native.h"
typedef enum{MIG_PROP_BOOL,MIG_PROP_NUMBER,MIG_PROP_COLOR}mig_prop_kind;
typedef struct{const char *name;mig_prop_kind kind;void *value;double lo,hi;}mig_prop;
static inline int mulib_immgtk_propertygrid(mulib_immgtk_native_t *g,const mig_prop *p,size_t n,mgx_rect r){int changed=0;for(size_t i=0;i<n&&i<(size_t)r.h/24;i++){if(!p[i].name||!p[i].value||!mig_push_id(g,p[i].name))continue;mgx_rect label={r.x,r.y+(int)i*24,r.w/2,24},value={r.x+r.w/2,label.y,r.w-r.w/2,24};mig_label(g,label,p[i].name);if(p[i].kind==MIG_PROP_BOOL)changed|=mig_checkbox(g,"value","",value,p[i].value);else if(p[i].kind==MIG_PROP_NUMBER)changed|=mig_slider(g,"value",value,p[i].value,p[i].lo,p[i].hi);else mig_rect(g,value,*(uint32_t*)p[i].value);mig_pop_id(g);}return changed;}
#endif
