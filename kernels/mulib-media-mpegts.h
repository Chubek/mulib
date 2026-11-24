/** @file mulib-media-mpegts.h
 * 188-byte transport packet header and adaptation-field parsing.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No PES assembly, PAT/PMT or muxing.
 */
#ifndef MULIB_MEDIA_MPEGTS_H
#define MULIB_MEDIA_MPEGTS_H
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
typedef struct {uint16_t pid;uint8_t continuity;bool start,error,discontinuity;mulib_media_bytes payload;} mulib_media_mpegts_packet;
static inline bool mulib_media_mpegts_parse(const uint8_t *p,size_t n,mulib_media_mpegts_packet *o){if(!p||!o||n<188||p[0]!=0x47)return false;unsigned af=(p[3]>>4)&3;if(!af)return false;size_t pos=4;bool disc=false;if(af&2){unsigned len=p[pos++];if((af==2&&len!=183)||(af==3&&len>182))return false;if(len)disc=(p[pos]&0x80)!=0;pos+=len;}o->pid=(uint16_t)(((p[1]&31)<<8)|p[2]);o->start=(p[1]&64)!=0;o->error=(p[1]&128)!=0;o->continuity=p[3]&15;o->discontinuity=disc;o->payload.data=p+pos;o->payload.size=(af&1)?188-pos:0;return true;}
#ifdef __cplusplus
}
#endif
#endif
