/** @file mulib-distribcluster.h
 * @brief Allowlisted distributed container, OCI lifecycle and VMM worker actions.
 * Define MULIB_IMPL_DISTRIBCLUSTER and all dependencies once; see manual.
 */
#ifndef MULIB_DISTRIBCLUSTER_H
#define MULIB_DISTRIBCLUSTER_H
#include "mulib-distribcomp.h"
#include "mulib-container-runtime.h"
#include "mulib-ocicontainer.h"
#include "mulib-vmm.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Caller-owned registry, initialized once and externally synchronized. Resource
 * pointers/clock context are borrowed. No implicit resource allocation/launch. */
typedef struct mulib_distribcluster {
  struct {
    char id[64];
    unsigned kind;
    mulib_container *container_record;
    union { mulib_container_runtime *container; mulib_ocicontainer_t *oci; mulib_vmm_t *vmm; } as;
  } resources[32];
  size_t count;
  uint64_t (*clock_ms)(void *);
  void *clock_user;
} mulib_distribcluster_t;
/** Initialize an unused registry. Container actions require a monotonic clock. */
void mulib_distribcluster_init(mulib_distribcluster_t *, uint64_t (*clock_ms)(void *), void *user);
/** Bind a local prepared resource under a unique portable ID. Returns 0/error.
 * Registry never destroys borrowed resources. Same ID across kinds is rejected. */
int mulib_distribcluster_container(mulib_distribcluster_t *, const char *, mulib_container_runtime *);
int mulib_distribcluster_oci(mulib_distribcluster_t *, const char *, mulib_ocicontainer_t *);
int mulib_distribcluster_vmm(mulib_distribcluster_t *, const char *, mulib_vmm_t *);
/** Worker execute callback: payload is exactly a bound resource ID, not a path
 * or command. Actions: status/start/stop/close; container also pause/resume/step;
 * VMM also reset/step. OCI only changes its existing metadata lifecycle.
 * step submits one unit and is NOT idempotent across new attempts. */
int mulib_distribcluster_execute(void *, const char *kind, const char *action,
    const char *payload, size_t length, mulib_strbuf_t *output);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DISTRIBCLUSTER) && !defined(MULIB_DISTRIBCLUSTER_IMPLEMENTED)
#define MULIB_DISTRIBCLUSTER_IMPLEMENTED
#include <stdio.h>
#include <string.h>
void mulib_distribcluster_init(mulib_distribcluster_t *c, uint64_t (*clock)(void *), void *u) {
  if (c) { memset(c,0,sizeof *c); c->clock_ms=clock; c->clock_user=u; }
}
static int mulib_distribcluster_bind_(mulib_distribcluster_t *c, const char *id, unsigned kind,
    mulib_container_runtime *container, mulib_ocicontainer_t *oci, mulib_vmm_t *vmm) {
  size_t n;
  if (!c || c->count>=32 || !mulib_distribtask_identifier(id) || (!container&&!oci&&!vmm)) return -1;
  for (n=0;n<c->count;++n) if (!strcmp(c->resources[n].id,id)) return -1;
  n=c->count++; strcpy(c->resources[n].id,id); c->resources[n].kind=kind;
  if (kind==1) { c->resources[n].as.container=container; c->resources[n].container_record=container->container; }
  else if (kind==2) c->resources[n].as.oci=oci;
  else c->resources[n].as.vmm=vmm;
  return 0;
}
int mulib_distribcluster_container(mulib_distribcluster_t *c, const char *id, mulib_container_runtime *p) {
  if (!p || !p->prepared || !p->container) return -1;
  return mulib_distribcluster_bind_(c,id,1,p,NULL,NULL);
}
int mulib_distribcluster_oci(mulib_distribcluster_t *c, const char *id, mulib_ocicontainer_t *p) {
  return mulib_distribcluster_bind_(c,id,2,NULL,p,NULL);
}
int mulib_distribcluster_vmm(mulib_distribcluster_t *c, const char *id, mulib_vmm_t *p) {
  return mulib_distribcluster_bind_(c,id,3,NULL,NULL,p);
}
int mulib_distribcluster_execute(void *user, const char *kind, const char *action,
    const char *payload, size_t length, mulib_strbuf_t *out) {
  mulib_distribcluster_t *c=(mulib_distribcluster_t *)user;
  size_t i; unsigned k; int rc=-1, state=0; char text[32];
  if (!c || !kind || !action || !payload || !out || !length || length>63 || memchr(payload,0,length)) return -1;
  k=!strcmp(kind,"container")?1:!strcmp(kind,"ocicontainer")?2:!strcmp(kind,"vmm")?3:0;
  if (!k) return -1;
  for (i=0;i<c->count;++i) if (c->resources[i].kind==k && strlen(c->resources[i].id)==length
      && !memcmp(c->resources[i].id,payload,length)) break;
  if (i==c->count) return -1;
  if (k==1) {
    mulib_container_runtime *r=c->resources[i].as.container;
    mulib_container *record=c->resources[i].container_record;
    uint64_t now;
    if (!record || !c->clock_ms) return -1;
    now=c->clock_ms(c->clock_user);
    if (!strcmp(action,"status")) rc=0;
    else if (!r->prepared) rc=!strcmp(action,"close")?0:-1;
    else if (!strcmp(action,"start")) rc=r->container->state==MULIB_CONTAINER_RUNNING?0:mulib_container_runtime_start(r,now);
    else if (!strcmp(action,"stop")) rc=r->container->state==MULIB_CONTAINER_EXITED
      || r->container->state==MULIB_CONTAINER_DELETED?0:mulib_container_runtime_signal(r,15);
    else if (!strcmp(action,"pause")) rc=r->container->state==MULIB_CONTAINER_PAUSED?0:mulib_container_runtime_pause(r,1,now);
    else if (!strcmp(action,"resume")) rc=r->container->state==MULIB_CONTAINER_RUNNING?0:mulib_container_runtime_pause(r,0,now);
    else if (!strcmp(action,"close")) rc=mulib_container_runtime_close(r,now);
    else if (!strcmp(action,"step")) {
      rc=mulib_container_runtime_step(r,1,now);
      if (rc==MULIB_CT_DONE || rc==MULIB_CT_AGAIN) rc=0;
    }
    state=(int)record->state;
  } else if (k==2) {
    mulib_ocicontainer_t *r=c->resources[i].as.oci; int next=0;
    if (!strcmp(action,"status")) rc=0;
    else {
      if (!strcmp(action,"start")) next=MULIB_OCICONTAINER_RUNNING;
      else if (!strcmp(action,"stop")) next=MULIB_OCICONTAINER_STOPPED;
      else if (!strcmp(action,"close")) next=MULIB_OCICONTAINER_DELETED;
      if (next) rc=r->state==next?0:mulib_ocicontainer_transition(r,next,0)?0:-1;
    }
    state=r->state;
  } else {
    mulib_vmm_t *r=c->resources[i].as.vmm;
    if (!strcmp(action,"status")) rc=0;
    else if (!strcmp(action,"start")) rc=r->state==MULIB_VMM_RUNNING?0:mulib_vmm_start(r);
    else if (!strcmp(action,"stop")) rc=r->state==MULIB_VMM_READY?0:mulib_vmm_stop(r);
    else if (!strcmp(action,"close")) rc=mulib_vmm_close(r);
    else if (!strcmp(action,"reset")) rc=mulib_vmm_reset(r);
    else if (!strcmp(action,"step")) { mulib_vmm_exit_t exit; rc=mulib_vmm_step(r,&exit); }
    state=(int)r->state;
  }
  (void)snprintf(text,sizeof text,"%d",state);
  if (!mulib_strbuf_append(out,text)) return -3;
  return rc;
}
#endif
