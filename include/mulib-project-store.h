/** @file mulib-project-store.h
 * Transactional provider interface with lifecycle guards.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Provider supplies durability and recovery, e.g. existing KVDB.
 */
#ifndef MULIB_PROJECT_STORE_H
#define MULIB_PROJECT_STORE_H
#include "mulib-media-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {void *ctx;mulib_media_status (*begin)(void*);mulib_media_status (*put)(void*,mulib_media_bytes,mulib_media_bytes);mulib_media_status (*commit)(void*);void (*rollback)(void*);bool active;} mulib_project_store;
static inline mulib_media_status mulib_project_store_begin(mulib_project_store *s){if(!s||s->active||!s->begin||!s->rollback)return MULIB_MEDIA_INVALID;mulib_media_status r=s->begin(s->ctx);if(r==MULIB_MEDIA_OK)s->active=true;return r;}
static inline mulib_media_status mulib_project_store_put(mulib_project_store *s,mulib_media_bytes k,mulib_media_bytes v){return !s||!s->active||!s->put||!k.size||!mulib_media_span_valid(k)||!mulib_media_span_valid(v)?MULIB_MEDIA_INVALID:s->put(s->ctx,k,v);}
static inline mulib_media_status mulib_project_store_commit(mulib_project_store *s){if(!s||!s->active||!s->commit)return MULIB_MEDIA_INVALID;mulib_media_status r=s->commit(s->ctx);if(r==MULIB_MEDIA_OK)s->active=false;return r;}
/* Invalid/incomplete providers are ignored; never call through NULL. Mark the
 * transaction inactive before invoking rollback to prevent duplicate rollback. */
static inline void mulib_project_store_rollback(mulib_project_store *s){if(s&&s->active&&s->rollback){s->active=false;s->rollback(s->ctx);}}
#ifdef __cplusplus
}
#endif
#endif
