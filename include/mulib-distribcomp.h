/** @file mulib-distribcomp.h
 * @brief Lease-based MuMQ DAG orchestration, bounded workers and daemon control.
 * Define MULIB_IMPL_DISTRIBCOMP and the dependencies listed in the manual.
 */
#ifndef MULIB_DISTRIBCOMP_H
#define MULIB_DISTRIBCOMP_H
#include "mulib-distribtask.h"
#include "mulib-mumq-publisher.h"
#include "mulib-mumq-subscriber.h"
#include "mulib-daemon.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_distribcomp mulib_distribcomp_t;
typedef struct mulib_distribcomp_worker mulib_distribcomp_worker_t;
enum mulib_distribcomp_state {
  MULIB_DISTRIBCOMP_PENDING, MULIB_DISTRIBCOMP_RUNNING, MULIB_DISTRIBCOMP_SUCCEEDED,
  MULIB_DISTRIBCOMP_FAILED, MULIB_DISTRIBCOMP_BLOCKED, MULIB_DISTRIBCOMP_CANCELLED
};
/** Borrowed snapshot; output remains valid until the next result/destruction. */
typedef struct mulib_distribcomp_task_status {
  unsigned state, attempt;
  uint64_t deadline;
  int code;
  const char *output;
} mulib_distribcomp_task_status_t;
/** Allocate a coordinator borrowing an immutable plan and channel. run_id is
 * a unique caller-generated portable ID per orchestration, including restarts.
 * Creation queues a results subscription; tick waits for its SUBACK. */
mulib_distribcomp_t *mulib_distribcomp_create(const mulib_distribtask_plan_t *, mulib_mumq_t *, const char *run_id);
/** Free local state only; does not cancel remote work or close borrowed objects. */
void mulib_distribcomp_destroy(mulib_distribcomp_t *);
/** Advance leases, dependencies and dispatch, serializing tasks per node.
 * now_ms must be nondecreasing and leave room for one lease. Does not read.
 * Returns MuMQ status; AGAIN is backpressure, not a task failure. */
int mulib_distribcomp_tick(mulib_distribcomp_t *, uint64_t now_ms);
/** Consume a borrowed SUBACK/result; malformed input returns INVALID without
 * mutation. Stale/duplicate/unrelated results return AGAIN. */
int mulib_distribcomp_handle(mulib_distribcomp_t *, const mulib_mumq_message_t *);
/** tick plus at most one receive/handle; caller also advances the broker. */
int mulib_distribcomp_step(mulib_distribcomp_t *, uint64_t now_ms);
/** Copy one task status; return 0 or INVALID. */
int mulib_distribcomp_status(const mulib_distribcomp_t *, size_t, mulib_distribcomp_task_status_t *);
/** Mark every unfinished local task CANCELLED. Remote actions are not killed. */
int mulib_distribcomp_cancel(mulib_distribcomp_t *);
/** 0 unfinished, 1 all successful (including empty plan), -1 terminal failure. */
int mulib_distribcomp_finished(const mulib_distribcomp_t *);
/** Execute an approved backend action. Return 0 success or -255..255 error.
 * Append <=64 KiB NUL-free UTF-8 result to initialized output. Inputs are
 * borrowed only for the call. Must not reenter worker; retries require
 * idempotent actions. No action is implicitly implemented as a shell command. */
typedef int (*mulib_distribcomp_execute_fn)(void *user, const char *kind, const char *action,
    const char *payload, size_t length, mulib_strbuf_t *output);
typedef struct mulib_distribcomp_worker_options {
  mulib_distribcomp_execute_fn execute;
  void *user;
  int allow_mune, allow_daemon; /**< Explicit opt-ins to trusted host code/launch. */
} mulib_distribcomp_worker_options_t;
/** Borrow channel, copy IDs/options and queue node subscription. Every worker
 * is pinned to one run and node. Start only one worker for that node/run. */
mulib_distribcomp_worker_t *mulib_distribcomp_worker_create(mulib_mumq_t *, const char *cluster,
    const char *node, const char *run_id, const mulib_distribcomp_worker_options_t *);
/** Free cached results and terminate/reap any daemons this worker launched.
 * The borrowed channel is not closed. Call before destroying callback state. */
void mulib_distribcomp_worker_destroy(mulib_distribcomp_worker_t *);
/** Flush/retry results, then receive and execute at most one task. Execution
 * is synchronous and bounded by the selected backend. Duplicate attempts
 * resend cached results; older attempts never execute again within this
 * worker lifetime. Cache: 256 task IDs, latest attempt/result per ID. */
int mulib_distribcomp_worker_step(mulib_distribcomp_worker_t *);
/** Handle an already received message. AGAIN means unrelated/stale or pending
 * result backpressure; caller must retry a request if a prior result is pending. */
int mulib_distribcomp_worker_handle(mulib_distribcomp_worker_t *, const mulib_mumq_message_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DISTRIBCOMP) && !defined(MULIB_DISTRIBCOMP_IMPLEMENTED)
#define MULIB_DISTRIBCOMP_IMPLEMENTED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct mulib_distribcomp_record_ {
  mulib_distribcomp_task_status_t status;
  char *output;
} mulib_distribcomp_record_;
struct mulib_distribcomp {
  const mulib_distribtask_plan_t *plan;
  mulib_mumq_t *channel;
  char run[64], topic[256];
  int ready;
  uint64_t now;
  mulib_distribcomp_record_ records[256];
};
typedef struct mulib_distribcomp_cache_ {
  char id[64];
  unsigned attempt;
  unsigned char digest[32];
  char *reply;
} mulib_distribcomp_cache_;
struct mulib_distribcomp_worker {
  mulib_mumq_t *channel;
  mulib_distribcomp_worker_options_t options;
  char cluster[64], node[64], run[64], topic[256], results[256];
  int ready;
  size_t count;
  mulib_distribcomp_cache_ cache[256];
  char *pending;
  struct { char name[128]; mulib_process_t child; } daemons[32];
};
static const char *mulib_distribcomp_text_(const mulib_json_t *j, const char *key, size_t *length) {
  size_t n; const char *s=mulib_json_text(mulib_json_get(j,key),&n);
  if (!s || memchr(s,0,n)) return NULL;
  if (length) *length=n;
  return s;
}
static int mulib_distribcomp_int_(const mulib_json_t *j, const char *key, int low, int high, int *out) {
  const mulib_json_t *v=mulib_json_get(j,key); double d;
  if (mulib_json_typeof(v)!=MULIB_JSON_NUMBER) return 0;
  d=mulib_json_numeric(v);
  if (!(d>=low && d<=high) || d!=(double)(int)d) return 0;
  *out=(int)d; return 1;
}
static int mulib_distribcomp_set_(mulib_json_t *o, const char *k, mulib_json_t *v) {
  if (!v) return 0;
  if (!mulib_json_set(o,k,v)) { mulib_json_destroy(v); return 0; } return 1;
}
static mulib_json_t *mulib_distribcomp_envelope_(const char *run, const char *id, const char *node, unsigned attempt) {
  mulib_json_t *j=mulib_json_new(MULIB_JSON_OBJECT);
  if (!j) return NULL;
  if (!mulib_distribcomp_set_(j,"v",mulib_json_number(1))
      || !mulib_distribcomp_set_(j,"run",mulib_json_string(run))
      || !mulib_distribcomp_set_(j,"task",mulib_json_string(id))
      || !mulib_distribcomp_set_(j,"node",mulib_json_string(node))
      || !mulib_distribcomp_set_(j,"attempt",mulib_json_number(attempt))) {
    mulib_json_destroy(j); return NULL;
  }
  return j;
}
mulib_distribcomp_t *mulib_distribcomp_create(const mulib_distribtask_plan_t *p, mulib_mumq_t *q, const char *run) {
  mulib_distribcomp_t *c;
  if (!p || !q || !mulib_distribtask_identifier(run)) return NULL;
  c=(mulib_distribcomp_t *)calloc(1,sizeof *c); if (!c) return NULL;
  c->plan=p; c->channel=q; strcpy(c->run,run);
  (void)snprintf(c->topic,sizeof c->topic,"cluster/%s/result/%s",mulib_distribtask_cluster(p),run);
  if (mulib_mumq_subscribe(q,c->topic)) { free(c); return NULL; } return c;
}
void mulib_distribcomp_destroy(mulib_distribcomp_t *c) {
  if (c) { for (size_t i=0;i<256;++i) free(c->records[i].output); free(c); }
}
int mulib_distribcomp_status(const mulib_distribcomp_t *c, size_t i, mulib_distribcomp_task_status_t *s) {
  if (!c || !s || i>=mulib_distribtask_count(c->plan)) return -1;
  *s=c->records[i].status; s->output=c->records[i].output; return 0;
}
int mulib_distribcomp_finished(const mulib_distribcomp_t *c) {
  int failed=0;
  if (!c) return -1;
  for (size_t i=0;i<mulib_distribtask_count(c->plan);++i) {
    unsigned s=c->records[i].status.state;
    if (s==MULIB_DISTRIBCOMP_PENDING || s==MULIB_DISTRIBCOMP_RUNNING) return 0;
    if (s!=MULIB_DISTRIBCOMP_SUCCEEDED) failed=1;
  }
  return failed?-1:1;
}
int mulib_distribcomp_cancel(mulib_distribcomp_t *c) {
  if (!c) return -1;
  for (size_t i=0;i<mulib_distribtask_count(c->plan);++i)
    if (c->records[i].status.state<2) c->records[i].status.state=MULIB_DISTRIBCOMP_CANCELLED;
  return 0;
}
int mulib_distribcomp_tick(mulib_distribcomp_t *c, uint64_t now) {
  size_t count; uint64_t lease; int rc;
  if (!c || now<c->now || now>UINT64_MAX-(lease=mulib_distribtask_lease(c->plan))) return -1;
  c->now=now; count=mulib_distribtask_count(c->plan);
  if (count>256) return -1;
  rc=mulib_mumq_flush(c->channel); if (rc<0) return rc;
  for (size_t i=0;i<count;++i) {
    mulib_distribcomp_task_status_t *s=&c->records[i].status;
    if (s->state==MULIB_DISTRIBCOMP_RUNNING && now>=s->deadline) {
      s->code=-254;
      s->state=s->attempt>=mulib_distribtask_attempts(c->plan)?MULIB_DISTRIBCOMP_FAILED:MULIB_DISTRIBCOMP_PENDING;
    }
  }
  /* Propagate blocked dependencies to a fixed point, irrespective of order. */
  for (size_t pass=0;pass<count;++pass) {
    int changed=0;
    for (size_t i=0;i<count;++i) if (c->records[i].status.state==MULIB_DISTRIBCOMP_PENDING) {
      const mulib_distribtask_t *t=mulib_distribtask_at(c->plan,i);
      for (size_t j=0;j<t->dependency_count;++j) if (c->records[t->dependencies[j]].status.state>=MULIB_DISTRIBCOMP_FAILED) {
        c->records[i].status.state=MULIB_DISTRIBCOMP_BLOCKED; changed=1; break;
      }
    }
    if (!changed) break;
  }
  if (!c->ready) return 1;
  for (size_t i=0;i<count;++i) {
    const mulib_distribtask_t *t=mulib_distribtask_at(c->plan,i);
    mulib_distribcomp_task_status_t *s=&c->records[i].status;
    size_t j; mulib_json_t *request; char topic[256], *text;
    if (!t) return -1;
    if (s->state!=MULIB_DISTRIBCOMP_PENDING) continue;
    for (j=0;j<t->dependency_count;++j)
      if (c->records[t->dependencies[j]].status.state!=MULIB_DISTRIBCOMP_SUCCEEDED) break;
    if (j<t->dependency_count) continue;
    for (j=0;j<count;++j) {
      const mulib_distribtask_t *other=mulib_distribtask_at(c->plan,j);
      if (!other) return -1;
      if (c->records[j].status.state==MULIB_DISTRIBCOMP_RUNNING && !strcmp(t->node,other->node)) break;
    }
    if (j<count) continue;
    request=mulib_distribcomp_envelope_(c->run,t->id,t->node,s->attempt+1);
    if (!request) return -3;
    if (!mulib_distribcomp_set_(request,"kind",mulib_json_string(t->kind))
        || !mulib_distribcomp_set_(request,"action",mulib_json_string(t->action))
        || !mulib_distribcomp_set_(request,"payload",mulib_json_string_n(t->payload,t->length))) {
      mulib_json_destroy(request); return -3;
    }
    text=mulib_json_stringify(request,0); mulib_json_destroy(request); if (!text) return -3;
    (void)snprintf(topic,sizeof topic,"cluster/%s/node/%s",mulib_distribtask_cluster(c->plan),t->node);
    rc=mulib_mumq_publish(c->channel,topic,text,strlen(text)); free(text);
    if (rc) return rc;
    ++s->attempt; s->deadline=now+lease; s->state=MULIB_DISTRIBCOMP_RUNNING;
  }
  return mulib_mumq_flush(c->channel);
}
int mulib_distribcomp_handle(mulib_distribcomp_t *c, const mulib_mumq_message_t *m) {
  mulib_json_t *j; const char *run, *id, *node, *output;
  size_t n=0, index; int version, attempt, code, rc=-1;
  if (!c || !m) return -1;
  if (mulib_mumq_subscribed(m,c->topic)) { c->ready=1; return 0; }
  if (m->type!=MULIB_MUMQ_PUBLISH || strcmp(m->topic,c->topic)) return 1;
  if ((!m->data&&m->length) || m->length>MULIB_MUMQ_MAX_PAYLOAD) return -1;
  j=mulib_json_parse((const char *)m->data,m->length,NULL); if (!j) return -1;
  run=mulib_distribcomp_text_(j,"run",NULL); id=mulib_distribcomp_text_(j,"task",NULL);
  node=mulib_distribcomp_text_(j,"node",NULL); output=mulib_distribcomp_text_(j,"output",&n);
  if (!run || !id || !node || !output || n>65536
      || !mulib_distribcomp_int_(j,"v",1,1,&version)
      || !mulib_distribcomp_int_(j,"attempt",1,100,&attempt)
      || !mulib_distribcomp_int_(j,"code",-255,255,&code)) goto done;
  rc=1;
  if (strcmp(run,c->run)) goto done;
  for (index=0;index<mulib_distribtask_count(c->plan);++index)
    if (!strcmp(id,mulib_distribtask_at(c->plan,index)->id)) break;
  if (index<mulib_distribtask_count(c->plan)) {
    mulib_distribcomp_record_ *r=&c->records[index];
    char *copy;
    if (r->status.state!=MULIB_DISTRIBCOMP_RUNNING || r->status.attempt!=(unsigned)attempt
        || strcmp(node,mulib_distribtask_at(c->plan,index)->node) || c->now>=r->status.deadline) goto done;
    copy=(char *)malloc(n+1); if (!copy) { rc=-3; goto done; }
    memcpy(copy,output,n+1); free(r->output); r->output=copy; r->status.code=code;
    r->status.state=!code?MULIB_DISTRIBCOMP_SUCCEEDED:
      r->status.attempt>=mulib_distribtask_attempts(c->plan)?MULIB_DISTRIBCOMP_FAILED:MULIB_DISTRIBCOMP_PENDING;
    rc=0;
  }
done:
  mulib_json_destroy(j); return rc;
}
int mulib_distribcomp_step(mulib_distribcomp_t *c, uint64_t now) {
  mulib_mumq_message_t m={0}; int rc=mulib_distribcomp_tick(c,now);
  if (rc<0) return rc;
  rc=mulib_mumq_receive(c->channel,&m);
  if (!rc) rc=mulib_distribcomp_handle(c,&m);
  mulib_mumq_message_free(&m); return rc;
}
mulib_distribcomp_worker_t *mulib_distribcomp_worker_create(mulib_mumq_t *q, const char *cluster,
    const char *node, const char *run, const mulib_distribcomp_worker_options_t *o) {
  mulib_distribcomp_worker_t *w;
  if (!q || !mulib_distribtask_identifier(cluster) || !mulib_distribtask_identifier(node)
      || !mulib_distribtask_identifier(run)) return NULL;
  w=(mulib_distribcomp_worker_t *)calloc(1,sizeof *w); if (!w) return NULL;
  w->channel=q; if (o) w->options=*o;
  strcpy(w->cluster,cluster); strcpy(w->node,node); strcpy(w->run,run);
  (void)snprintf(w->topic,sizeof w->topic,"cluster/%s/node/%s",cluster,node);
  (void)snprintf(w->results,sizeof w->results,"cluster/%s/result/%s",cluster,run);
  if (mulib_mumq_subscribe(q,w->topic)) { free(w); return NULL; } return w;
}
void mulib_distribcomp_worker_destroy(mulib_distribcomp_worker_t *w) {
  if (!w) return;
  for (size_t i=0;i<w->count;++i) free(w->cache[i].reply);
  for (size_t i=0;i<32;++i) if (w->daemons[i].child.pid) {
    mulib_process_status_t status;
    (void)mulib_process_terminate(&w->daemons[i].child,&status);
  }
  free(w);
}
static int mulib_distribcomp_mune_(const char *p, size_t n, mulib_strbuf_t *out) {
  mulib_mune_state_t *s=mulib_mune_state_create(NULL);
  mulib_mune_result_t *r=NULL; mulib_mune_error_t e={MULIB_MUNE_OK,0,""}; int rc=-1;
  if (!s) return -3;
  if (mulib_mune_run_source(s,p,n,&r,&e)) (void)mulib_strbuf_append(out,e.message?e.message:"Mune error");
  else {
    mulib_mune_value_t v=mulib_mune_result_get(r,0); size_t len;
    const char *text=mulib_mune_value_string(v,&len); char number[64];
    if (text) rc=len<=65536 && !memchr(text,0,len) && mulib_strbuf_append_n(out,text,len)?0:-1;
    else if (v.type==MULIB_MUNE_NIL) rc=0;
    else if (v.type==MULIB_MUNE_INTEGER || v.type==MULIB_MUNE_NUMBER) {
      if (v.type==MULIB_MUNE_INTEGER) (void)snprintf(number,sizeof number,"%lld",(long long)v.as.integer);
      else (void)snprintf(number,sizeof number,"%.17g",v.as.number);
      rc=mulib_strbuf_append(out,number)?0:-3;
    } else if (v.type==MULIB_MUNE_BOOLEAN) rc=mulib_strbuf_append(out,v.as.boolean?"true":"false")?0:-3;
  }
  mulib_mune_result_destroy(r); mulib_mune_state_destroy(s); return rc;
}
static int mulib_distribcomp_daemon_(mulib_distribcomp_worker_t *w, const char *action,
                                    const char *p, size_t n, mulib_strbuf_t *out) {
  if (!strcmp(action,"start")) {
    mulib_daemon_error_t e; mulib_daemon_t *d=mulib_daemon_load(p,n,&e); size_t slot, empty=32; int rc;
    if (!d) { (void)mulib_strbuf_append(out,e.message); return -1; }
    for (slot=0;slot<32;++slot) {
      if (!w->daemons[slot].child.pid && empty==32) empty=slot;
      if (!strcmp(w->daemons[slot].name,mulib_daemon_name(d))) break;
    }
    if (slot==32) slot=empty;
    if (slot==32) { mulib_daemon_destroy(d); return -1; }
    if (w->daemons[slot].child.pid) rc=0;
    else {
      rc=mulib_daemon_launch(d,NULL,NULL,&w->daemons[slot].child);
      if (!rc) strcpy(w->daemons[slot].name,mulib_daemon_name(d));
    }
    mulib_daemon_destroy(d); return rc;
  }
  if (!strcmp(action,"stop")) {
    for (size_t i=0;i<32;++i) if (strlen(w->daemons[i].name)==n && !memcmp(w->daemons[i].name,p,n))
      return w->daemons[i].child.pid?mulib_daemon_stop(&w->daemons[i].child):0;
    return 0; /* Already absent is idempotent. */
  }
  return -1;
}
static int mulib_distribcomp_pending_(mulib_distribcomp_worker_t *w) {
  int rc;
  if (!w->pending) return 0;
  rc=mulib_mumq_publish(w->channel,w->results,w->pending,strlen(w->pending));
  if (!rc) w->pending=NULL;
  return rc;
}
int mulib_distribcomp_worker_handle(mulib_distribcomp_worker_t *w, const mulib_mumq_message_t *m) {
  mulib_json_t *j=NULL, *reply=NULL;
  const char *run, *id, *node, *kind, *action, *payload;
  size_t n=0, index; int v, attempt, code, rc=-1;
  unsigned char digest[32]; mulib_strbuf_t output;
  mulib_distribcomp_cache_ *entry;
  if (!w || !m) return -1;
  if (mulib_mumq_subscribed(m,w->topic)) { w->ready=1; return 0; }
  if (!w->ready || w->pending || m->type!=MULIB_MUMQ_PUBLISH || strcmp(m->topic,w->topic)) return 1;
  if ((!m->data&&m->length) || m->length>MULIB_MUMQ_MAX_PAYLOAD) return -1;
  j=mulib_json_parse((const char *)m->data,m->length,NULL); if (!j) return -1;
  run=mulib_distribcomp_text_(j,"run",NULL); id=mulib_distribcomp_text_(j,"task",NULL);
  node=mulib_distribcomp_text_(j,"node",NULL); kind=mulib_distribcomp_text_(j,"kind",NULL);
  action=mulib_distribcomp_text_(j,"action",NULL); payload=mulib_distribcomp_text_(j,"payload",&n);
  if (!run || !mulib_distribtask_identifier(id) || !node || !kind || !action || !payload || n>65536
      || strlen(action)>=32 || !mulib_distribtask_identifier(action)
      || (strcmp(kind,"mune") && strcmp(kind,"daemon") && strcmp(kind,"container")
          && strcmp(kind,"ocicontainer") && strcmp(kind,"vmm"))
      || !mulib_distribcomp_int_(j,"v",1,1,&v) || !mulib_distribcomp_int_(j,"attempt",1,100,&attempt)) goto done;
  rc=1; if (strcmp(run,w->run) || strcmp(node,w->node)) goto done;
  (void)mulib_authcrypto_hash(MULIB_AUTHCRYPTO_SHA256,m->data,m->length,digest,32);
  for (index=0;index<w->count;++index) if (!strcmp(id,w->cache[index].id)) break;
  if (index<w->count) {
    entry=&w->cache[index];
    if ((unsigned)attempt<entry->attempt) goto done;
    if ((unsigned)attempt==entry->attempt) {
      if (memcmp(entry->digest,digest,32)) { rc=-1; goto done; }
      if (!entry->reply) { rc=-3; goto done; }
      w->pending=entry->reply; rc=mulib_distribcomp_pending_(w); goto done;
    }
  } else if (index==256) { rc=-1; goto done; }
  entry=&w->cache[index];
  /* Reserve response allocations before executing an action with side effects.
   * Serialization can still fail after execution: that is an explicit error,
   * and the attempt stays marked to prevent unsafe automatic reexecution. */
  reply=mulib_distribcomp_envelope_(run,id,node,(unsigned)attempt);
  if (!reply) { rc=-3; goto done; }
  if (index==w->count) { strcpy(entry->id,id); ++w->count; }
  free(entry->reply); entry->reply=NULL; entry->attempt=(unsigned)attempt; memcpy(entry->digest,digest,32);
  mulib_strbuf_init(&output);
  if (!strcmp(kind,"mune") && w->options.allow_mune && !strcmp(action,"run"))
    code=mulib_distribcomp_mune_(payload,n,&output);
  else if (!strcmp(kind,"daemon") && w->options.allow_daemon)
    code=mulib_distribcomp_daemon_(w,action,payload,n,&output);
  else if (w->options.execute) code=w->options.execute(w->options.user,kind,action,payload,n,&output);
  else code=-253;
  if (code < -255 || code > 255) code=-255;
  if (output.length>65536 || (output.length && (memchr(output.data,0,output.length)
      || !mulib_utf8_validate((const unsigned char *)output.data,output.length)))) {
    code=-255; mulib_strbuf_free(&output); mulib_strbuf_init(&output);
  }
  if (!mulib_distribcomp_set_(reply,"code",mulib_json_number(code))
      || !mulib_distribcomp_set_(reply,"output",mulib_json_string_n(output.data?output.data:"",output.length))) rc=-3;
  else {
    entry->reply=mulib_json_stringify(reply,0);
    if (!entry->reply) rc=-3;
    else { w->pending=entry->reply; rc=mulib_distribcomp_pending_(w); if (rc==1) rc=0; }
  }
  mulib_strbuf_free(&output);
done:
  mulib_json_destroy(reply); mulib_json_destroy(j); return rc;
}
int mulib_distribcomp_worker_step(mulib_distribcomp_worker_t *w) {
  mulib_mumq_message_t m={0}; int rc;
  if (!w) return -1;
  for (size_t i=0;i<32;++i) if (w->daemons[i].child.pid) {
    mulib_process_status_t s;
    (void)mulib_process_wait(&w->daemons[i].child,&s,1);
  }
  rc=mulib_mumq_flush(w->channel); if (rc<0) return rc;
  rc=mulib_distribcomp_pending_(w); if (rc) return rc;
  rc=mulib_mumq_receive(w->channel,&m);
  if (!rc) rc=mulib_distribcomp_worker_handle(w,&m);
  mulib_mumq_message_free(&m); return rc;
}
#endif
