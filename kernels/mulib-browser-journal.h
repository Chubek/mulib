/* MuLIB browser staging addition: browser-journal
 * Implemented subset: Owned bounded event journal with monotonic sequences and copied payloads.
 * Remaining: Retention/compaction, multi-thread delivery, schema and snapshot integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_JOURNAL_H
#define MULIB_BROWSER_JOURNAL_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t seq,document,node;unsigned channel,kind;muba_buf payload;} mulib_browser_journal_event;
typedef struct {mulib_browser_journal_event *v;size_t len,cap,bytes,max_bytes;uint64_t seq;} mulib_browser_journal_t;
static inline int mulib_browser_journal_append(mulib_browser_journal_t *j,unsigned channel,unsigned kind,uint64_t document,uint64_t node,muba_span payload){if(!j||!channel||j->len>=j->cap||j->seq==UINT64_MAX||j->bytes>j->max_bytes||payload.len>j->max_bytes-j->bytes)return 0;mulib_browser_journal_event e={0};if(!muba_append(&e.payload,payload))return 0;e.seq=++j->seq;e.channel=channel;e.kind=kind;e.document=document;e.node=node;j->v[j->len++]=e;j->bytes+=payload.len;return 1;}
static inline void mulib_browser_journal_clear(mulib_browser_journal_t *j){if(j){for(size_t i=0;i<j->len;i++)muba_free(&j->v[i].payload);j->len=0;j->bytes=0;}}
#endif /* MULIB_BROWSER_JOURNAL_H */
