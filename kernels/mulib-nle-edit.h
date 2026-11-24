/** @file mulib-nle-edit.h
 * Split, trim and atomic adjacent roll editing.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_NLE_EDIT_H
#define MULIB_NLE_EDIT_H
#include "mulib-nle-timeline.h"
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
static inline bool mulib_nle_edit_split(mulib_nle_timeline *t,uint64_t id,int64_t at,uint64_t new_id){size_t i=mulib_nle_timeline_find(t,id);if(i==SIZE_MAX||!new_id)return false;mulib_nle_item c=t->items[i];if(!mulib_nle_item_valid(c)||at<=c.start||at>=c.start+c.length)return false;int64_t d=at-c.start;mulib_nle_item tail=c;tail.id=new_id;tail.start=at;tail.length-=d;tail.source+=d;if(!mulib_nle_timeline_insert(t,tail))return false;t->items[i].length=d;return true;}
static inline bool mulib_nle_edit_trim(mulib_nle_item *c,int64_t left,int64_t right,int64_t asset_length){if(!c||!mulib_nle_item_valid(*c)||left<0||right<=left)return false;mulib_nle_item v=*c;int64_t off=left-c->start;if(!mulib_media_i64_add(c->source,off,&v.source))return false;v.start=left;v.length=right-left;if(!mulib_nle_item_valid(v)||v.source+v.length>asset_length)return false;*c=v;return true;}
static inline bool mulib_nle_edit_roll(mulib_nle_item *a,mulib_nle_item *b,int64_t cut,int64_t asset_a,int64_t asset_b){if(!a||!b||a==b||!mulib_nle_item_valid(*a)||!mulib_nle_item_valid(*b)||a->track!=b->track||a->start+a->length!=b->start)return false;mulib_nle_item x=*a,y=*b;if(!mulib_nle_edit_trim(&x,x.start,cut,asset_a)||!mulib_nle_edit_trim(&y,cut,y.start+y.length,asset_b))return false;*a=x;*b=y;return true;}
#ifdef __cplusplus
}
#endif
#endif
