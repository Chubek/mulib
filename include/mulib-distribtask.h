/** @file mulib-distribtask.h
 * @brief Owned DAG tasks compiled from Mune plus required JSON/YAML configuration.
 * Define MULIB_IMPL_DISTRIBTASK with MUNE, JSON, YAML and their dependencies.
 */
#ifndef MULIB_DISTRIBTASK_H
#define MULIB_DISTRIBTASK_H
#include "mulib-mune.h"
#include "mulib-yaml.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_distribtask_plan mulib_distribtask_plan_t;
typedef struct mulib_distribtask_error {
  int code;
  size_t offset;
  char message[256];
} mulib_distribtask_error_t;
enum { MULIB_DISTRIBTASK_JSON=0, MULIB_DISTRIBTASK_YAML=1 };
/** Immutable borrowed definition. Dependency indices refer to the same plan. */
typedef struct mulib_distribtask {
  char id[64], node[64], kind[16], action[32];
  char *payload;
  size_t length, dependency_count;
  unsigned dependencies[256];
} mulib_distribtask_t;
/** Compile atomically. Configuration (1 MiB max) requires cluster:string and
 * nodes:array of 1..32 unique IDs. Optional attempts (1..100, default 3),
 * lease_ms (1..86400000, default 30000). Mune source/profile each <=1 MiB.
 * Profile executes first. Both see distrib_config and host functions
 * distrib_task(id,node,kind,action,payload), distrib_depends(id,prerequisite).
 * Limits: 256 tasks, 1024 edges, 64 KiB payload, acyclic graph. IDs <=63 bytes.
 * These are trusted Mune programs, NOT a security sandbox. */
mulib_distribtask_plan_t *mulib_distribtask_load(const char *config, size_t config_length,
    int format, const char *source, size_t length, const char *profile, size_t profile_length,
    mulib_distribtask_error_t *error);
/** File equivalent; profile_path may be NULL. Extensions are not interpreted. */
mulib_distribtask_plan_t *mulib_distribtask_load_file(const char *config_path, int format,
    const char *task_path, const char *profile_path, mulib_distribtask_error_t *error);
/** Destroy all owned definitions/configuration. NULL safe. */
void mulib_distribtask_destroy(mulib_distribtask_plan_t *);
/** Borrow metadata and definitions, valid until destruction. */
const char *mulib_distribtask_cluster(const mulib_distribtask_plan_t *);
size_t mulib_distribtask_count(const mulib_distribtask_plan_t *);
const mulib_distribtask_t *mulib_distribtask_at(const mulib_distribtask_plan_t *, size_t index);
const mulib_json_t *mulib_distribtask_config(const mulib_distribtask_plan_t *);
unsigned mulib_distribtask_attempts(const mulib_distribtask_plan_t *);
uint64_t mulib_distribtask_lease(const mulib_distribtask_plan_t *);
/** Validate a portable ID (ASCII alnum/underscore/dot/hyphen, not . or ..). */
int mulib_distribtask_identifier(const char *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DISTRIBTASK) && !defined(MULIB_DISTRIBTASK_IMPLEMENTED)
#define MULIB_DISTRIBTASK_IMPLEMENTED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct mulib_distribtask_plan {
  char cluster[64], nodes[32][64];
  size_t node_count, count;
  unsigned attempts;
  uint64_t lease;
  mulib_json_t *config;
  mulib_distribtask_t tasks[256];
};
typedef struct mulib_distribtask_build_ {
  mulib_distribtask_plan_t *plan;
  size_t edges;
  char from[1024][64], to[1024][64];
} mulib_distribtask_build_;
static int mulib_distribtask_error_(mulib_distribtask_error_t *e, int code, size_t at, const char *s) {
  if (e) { e->code=code; e->offset=at; (void)snprintf(e->message,sizeof e->message,"%s",s?s:""); }
  return code;
}
int mulib_distribtask_identifier(const char *s) {
  size_t i;
  if (!s || !*s || !strcmp(s,".") || !strcmp(s,"..")) return 0;
  for (i=0;s[i];++i) {
    unsigned char c=(unsigned char)s[i];
    if (i==63 || !((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9')
      || c=='_' || c=='-' || c=='.')) return 0;
  }
  return 1;
}
static const char *mulib_distribtask_text_(const mulib_json_t *v) {
  size_t n; const char *s=mulib_json_text(v,&n);
  return s && !memchr(s,0,n) ? s : NULL;
}
static int mulib_distribtask_number_(const mulib_json_t *v, unsigned def, unsigned max, unsigned *out) {
  double d;
  if (!v) { *out=def; return 1; }
  if (mulib_json_typeof(v)!=MULIB_JSON_NUMBER) return 0;
  d=mulib_json_numeric(v);
  if (!(d>=1 && d<=max) || d!=(double)(unsigned)d) return 0;
  *out=(unsigned)d; return 1;
}
void mulib_distribtask_destroy(mulib_distribtask_plan_t *p) {
  if (p) { for (size_t i=0;i<p->count;++i) free(p->tasks[i].payload); mulib_json_destroy(p->config); free(p); }
}
const char *mulib_distribtask_cluster(const mulib_distribtask_plan_t *p) { return p?p->cluster:NULL; }
size_t mulib_distribtask_count(const mulib_distribtask_plan_t *p) { return p?p->count:0; }
const mulib_distribtask_t *mulib_distribtask_at(const mulib_distribtask_plan_t *p, size_t i) { return p&&i<p->count?&p->tasks[i]:NULL; }
const mulib_json_t *mulib_distribtask_config(const mulib_distribtask_plan_t *p) { return p?p->config:NULL; }
unsigned mulib_distribtask_attempts(const mulib_distribtask_plan_t *p) { return p?p->attempts:0; }
uint64_t mulib_distribtask_lease(const mulib_distribtask_plan_t *p) { return p?p->lease:0; }
static int mulib_distribtask_find_(mulib_distribtask_plan_t *p, const char *id) {
  for (size_t i=0;i<p->count;++i) if (!strcmp(p->tasks[i].id,id)) return (int)i;
  return -1;
}
static mulib_mune_status_t mulib_distribtask_native_(mulib_mune_state_t *s, const mulib_mune_value_t *a,
    size_t n, mulib_mune_result_t *out, void *user) {
  mulib_distribtask_build_ *b=(mulib_distribtask_build_ *)user;
  const char *v[5]; size_t lengths[5];
  (void)s; (void)out;
  if (n!=2 && n!=5) return MULIB_MUNE_RUNTIME;
  for (size_t i=0;i<n;++i) {
    v[i]=mulib_mune_value_string(a[i],&lengths[i]);
    if (!v[i] || memchr(v[i],0,lengths[i])) return MULIB_MUNE_RUNTIME;
  }
  if (!mulib_distribtask_identifier(v[0]) || !mulib_distribtask_identifier(v[1])) return MULIB_MUNE_RUNTIME;
  if (n==2) {
    if (b->edges==1024) return MULIB_MUNE_LIMIT;
    strcpy(b->from[b->edges],v[0]); strcpy(b->to[b->edges++],v[1]);
  } else {
    mulib_distribtask_t *t;
    size_t node;
    if (b->plan->count==256 || lengths[4]>65536) return MULIB_MUNE_LIMIT;
    if (!mulib_utf8_validate((const unsigned char *)v[4],lengths[4])) return MULIB_MUNE_RUNTIME;
    if (mulib_distribtask_find_(b->plan,v[0])>=0 || lengths[3]>=32
        || !mulib_distribtask_identifier(v[3]) || (strcmp(v[2],"mune") && strcmp(v[2],"daemon")
          && strcmp(v[2],"container") && strcmp(v[2],"ocicontainer") && strcmp(v[2],"vmm"))) return MULIB_MUNE_RUNTIME;
    for (node=0;node<b->plan->node_count;++node) if (!strcmp(v[1],b->plan->nodes[node])) break;
    if (node==b->plan->node_count) return MULIB_MUNE_RUNTIME;
    t=&b->plan->tasks[b->plan->count]; t->payload=(char *)malloc(lengths[4]+1);
    if (!t->payload) return MULIB_MUNE_MEMORY;
    strcpy(t->id,v[0]); strcpy(t->node,v[1]); strcpy(t->kind,v[2]); strcpy(t->action,v[3]);
    memcpy(t->payload,v[4],lengths[4]+1); t->length=lengths[4]; ++b->plan->count;
  }
  return MULIB_MUNE_OK;
}
static mulib_mune_status_t mulib_distribtask_add_(mulib_mune_state_t *s, const mulib_mune_value_t *a,
    size_t n, mulib_mune_result_t *out, void *user) {
  return n==5?mulib_distribtask_native_(s,a,n,out,user):MULIB_MUNE_RUNTIME;
}
static mulib_mune_status_t mulib_distribtask_depend_(mulib_mune_state_t *s, const mulib_mune_value_t *a,
    size_t n, mulib_mune_result_t *out, void *user) {
  return n==2?mulib_distribtask_native_(s,a,n,out,user):MULIB_MUNE_RUNTIME;
}
/* Keep every temporary conversion value rooted until the global is installed. */
static int mulib_distribtask_value_(mulib_mune_state_t *s, const mulib_json_t *j,
                                   mulib_mune_result_t *roots, mulib_mune_value_t *v, unsigned depth) {
  mulib_json_type type=mulib_json_typeof(j);
  if (depth>128) return 0;
  switch (type) {
  case MULIB_JSON_NULL: *v=mulib_mune_nil(); break;
  case MULIB_JSON_BOOL: *v=mulib_mune_boolean(mulib_json_boolean(j)); break;
  case MULIB_JSON_NUMBER: *v=mulib_mune_number(mulib_json_numeric(j)); break;
  case MULIB_JSON_STRING: {
    size_t n; const char *text=mulib_json_text(j,&n);
    if (mulib_mune_make_string(s,text,n,v)) return 0;
    break;
  }
  default:
    if (mulib_mune_make_table(s,v) || !mulib_mune_result_push(roots,*v)) return 0;
    for (size_t i=0;i<mulib_json_size(j);++i) {
      mulib_mune_value_t key, val;
      if (type==MULIB_JSON_ARRAY) key=mulib_mune_integer((int64_t)i+1);
      else {
        size_t n; const char *text=mulib_json_key(j,i,&n);
        if (mulib_mune_make_string(s,text,n,&key) || !mulib_mune_result_push(roots,key)) return 0;
      }
      if (!mulib_distribtask_value_(s,mulib_json_at(j,i),roots,&val,depth+1)
          || mulib_mune_raw_set(s,*v,key,val)) return 0;
    }
  }
  return mulib_mune_result_push(roots,*v);
}
static int mulib_distribtask_graph_(mulib_distribtask_build_ *b) {
  unsigned char done[256]={0}; size_t completed=0;
  for (size_t i=0;i<b->edges;++i) {
    int a=mulib_distribtask_find_(b->plan,b->from[i]), d=mulib_distribtask_find_(b->plan,b->to[i]);
    mulib_distribtask_t *t;
    if (a<0 || d<0 || a==d) return 0;
    t=&b->plan->tasks[a];
    for (size_t j=0;j<t->dependency_count;++j) if (t->dependencies[j]==(unsigned)d) return 0;
    t->dependencies[t->dependency_count++]=(unsigned)d;
  }
  for (;;) {
    size_t before=completed;
    for (size_t i=0;i<b->plan->count;++i) if (!done[i]) {
      const mulib_distribtask_t *t=&b->plan->tasks[i]; size_t j;
      for (j=0;j<t->dependency_count;++j) if (!done[t->dependencies[j]]) break;
      if (j==t->dependency_count) { done[i]=1; ++completed; }
    }
    if (completed==b->plan->count) return 1;
    if (completed==before) return 0;
  }
}
mulib_distribtask_plan_t *mulib_distribtask_load(const char *config, size_t cn, int format,
    const char *src, size_t n, const char *profile, size_t pn, mulib_distribtask_error_t *error) {
  mulib_distribtask_plan_t *p=NULL;
  mulib_distribtask_build_ *b=NULL;
  mulib_mune_state_t *s=NULL;
  mulib_mune_result_t *roots=NULL, *r=NULL;
  mulib_mune_error_t me={MULIB_MUNE_OK,0,""};
  mulib_json_error_t je={0}; mulib_yaml_error_t ye={0};
  mulib_json_t *nodes; mulib_mune_value_t value;
  const char *cluster; unsigned lease;
  int code=-1; const char *message="invalid configuration or input"; size_t at=0;
  if (!config || !cn || cn>1048576 || (!src&&n) || (!profile&&pn) || n>1048576 || pn>1048576
      || (format!=0 && format!=1)) goto fail;
  p=(mulib_distribtask_plan_t *)calloc(1,sizeof *p);
  b=(mulib_distribtask_build_ *)calloc(1,sizeof *b);
  if (!p || !b) { code=-2; message="allocation failed"; goto fail; }
  b->plan=p;
  p->config=format ? mulib_yaml_parse(config,cn,&ye) : mulib_json_parse(config,cn,&je);
  if (!p->config) { message=format?ye.message:je.message; at=format?ye.offset:je.offset; goto fail; }
  cluster=mulib_distribtask_text_(mulib_json_get(p->config,"cluster"));
  nodes=mulib_json_get(p->config,"nodes");
  if (!mulib_distribtask_identifier(cluster) || mulib_json_typeof(nodes)!=MULIB_JSON_ARRAY
      || !(p->node_count=mulib_json_size(nodes)) || p->node_count>32
      || !mulib_distribtask_number_(mulib_json_get(p->config,"attempts"),3,100,&p->attempts)
      || !mulib_distribtask_number_(mulib_json_get(p->config,"lease_ms"),30000,86400000,&lease)) goto fail;
  p->lease=lease; strcpy(p->cluster,cluster);
  for (size_t i=0;i<p->node_count;++i) {
    const char *node=mulib_distribtask_text_(mulib_json_at(nodes,i));
    if (!mulib_distribtask_identifier(node)) goto fail;
    for (size_t j=0;j<i;++j) if (!strcmp(node,p->nodes[j])) goto fail;
    strcpy(p->nodes[i],node);
  }
  s=mulib_mune_state_create(NULL);
  if (!s || !(roots=mulib_mune_result_create(s)) || !mulib_distribtask_value_(s,p->config,roots,&value,0)
      || mulib_mune_set_global(s,"distrib_config",value)
      || mulib_mune_bind(s,"distrib_task",mulib_distribtask_add_,b)
      || mulib_mune_bind(s,"distrib_depends",mulib_distribtask_depend_,b)) {
    code=-2; message="Mune allocation failed"; goto fail;
  }
  if (pn && mulib_mune_run_source(s,profile,pn,&r,&me)) goto script_fail;
  mulib_mune_result_destroy(r); r=NULL;
  if (mulib_mune_run_source(s,src,n,&r,&me)) goto script_fail;
  if (!mulib_distribtask_graph_(b)) { message="missing/duplicate dependency or cyclic graph"; goto fail; }
  mulib_mune_result_destroy(r); mulib_mune_result_destroy(roots); mulib_mune_state_destroy(s); free(b);
  mulib_distribtask_error_(error,0,0,""); return p;
script_fail:
  code=-3; message=me.message; at=me.offset;
fail:
  mulib_distribtask_error_(error,code,at,message);
  mulib_mune_result_destroy(r); mulib_mune_result_destroy(roots); mulib_mune_state_destroy(s);
  free(b); mulib_distribtask_destroy(p); return NULL;
}
static char *mulib_distribtask_read_(const char *path, size_t *n) {
  FILE *f; char *p; size_t size;
  if (!path || !(f=fopen(path,"rb"))) return NULL;
  p=(char *)malloc(1048577);
  if (!p) { fclose(f); return NULL; }
  size=fread(p,1,1048577,f);
  if (ferror(f) || size>1048576) { fclose(f); free(p); return NULL; }
  fclose(f); *n=size; return p;
}
mulib_distribtask_plan_t *mulib_distribtask_load_file(const char *cp, int format, const char *tp,
    const char *pp, mulib_distribtask_error_t *e) {
  size_t cn=0, tn=0, pn=0;
  char *c=mulib_distribtask_read_(cp,&cn), *t=mulib_distribtask_read_(tp,&tn);
  char *p=pp?mulib_distribtask_read_(pp,&pn):NULL;
  mulib_distribtask_plan_t *out=NULL;
  if (!c || !t || (pp&&!p)) mulib_distribtask_error_(e,-4,0,"cannot read bounded configuration/task/profile file");
  else out=mulib_distribtask_load(c,cn,format,t,tn,p,pn,e);
  free(c); free(t); free(p); return out;
}
#endif
