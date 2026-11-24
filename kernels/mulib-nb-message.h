/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_MESSAGE_H
#define MULIB_NB_MESSAGE_H
#include "mulib-wm-core.h"
typedef enum {MULIB_NB_MSG_EXECUTE=1,MULIB_NB_MSG_STDOUT,MULIB_NB_MSG_STDERR,MULIB_NB_MSG_DISPLAY,MULIB_NB_MSG_RESULT,MULIB_NB_MSG_ERROR,MULIB_NB_MSG_DONE,MULIB_NB_MSG_INTERRUPT} mulib_nb_message_kind;
typedef struct {uint64_t request,cell,sequence;mulib_nb_message_kind kind;const unsigned char *data;size_t length;} mulib_nb_message;
static inline void mulib_nb_msg_put(unsigned char *p,uint64_t x){for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(x>>(8*i));}
static inline uint64_t mulib_nb_msg_get(const unsigned char *p){uint64_t x=0;for(unsigned i=0;i<8;i++)x|=(uint64_t)p[i]<<(8*i);return x;}
static inline int mulib_nb_message_encode(const mulib_nb_message *m,unsigned char **out,size_t *n){if(!m||!out||!n||!m->request||!m->cell||m->kind<1||m->kind>MULIB_NB_MSG_INTERRUPT||m->length>SIZE_MAX-40||(!m->data&&m->length))return 0;unsigned char *p=(unsigned char*)malloc(40+m->length);if(!p)return 0;memcpy(p,"NBM1",4);memset(p+4,0,4);p[4]=(unsigned char)m->kind;mulib_nb_msg_put(p+8,m->request);mulib_nb_msg_put(p+16,m->cell);mulib_nb_msg_put(p+24,m->sequence);mulib_nb_msg_put(p+32,m->length);if(m->length)memcpy(p+40,m->data,m->length);*out=p;*n=40+m->length;return 1;}
static inline int mulib_nb_message_decode(const void *bytes,size_t n,size_t limit,mulib_nb_message *out){const unsigned char *p=(const unsigned char*)bytes;if(!p||!out||n<40||memcmp(p,"NBM1",4)||p[5]||p[6]||p[7]||p[4]<1||p[4]>MULIB_NB_MSG_INTERRUPT)return 0;uint64_t a=mulib_nb_msg_get(p+8),b=mulib_nb_msg_get(p+16),z=mulib_nb_msg_get(p+32);if(!a||!b||z!=n-40||z>limit)return 0;*out=(mulib_nb_message){a,b,mulib_nb_msg_get(p+24),(mulib_nb_message_kind)p[4],p+40,(size_t)z};return 1;}
#endif
