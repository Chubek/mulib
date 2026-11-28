/* MuLIB browser staging addition: mime-sniff
 * Implemented subset: Signature detection for PNG, GIF, JPEG, PDF and WebP.
 * Remaining: WHATWG sniffing contexts, text/binary detection and supplied type policy.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MIME_SNIFF_H
#define MULIB_MIME_SNIFF_H
#include "detail/mulib-browser-additions-common.h"

static inline const char *mulib_mime_sniff_detect(muba_span s){if(!muba_span_ok(s))return NULL;if(s.len>=8&&!memcmp(s.data,"\x89PNG\r\n\x1a\n",8))return "image/png";if(s.len>=6&&(!memcmp(s.data,"GIF87a",6)||!memcmp(s.data,"GIF89a",6)))return "image/gif";if(s.len>=3&&s.data[0]==255&&s.data[1]==216&&s.data[2]==255)return "image/jpeg";if(s.len>=5&&!memcmp(s.data,"%PDF-",5))return "application/pdf";if(s.len>=12&&!memcmp(s.data,"RIFF",4)&&!memcmp(s.data+8,"WEBP",4))return "image/webp";return NULL;}
#endif /* MULIB_MIME_SNIFF_H */
