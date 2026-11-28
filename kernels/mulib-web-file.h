/* MuLIB browser staging addition: web-file
 * Implemented subset: Owned file name, last-modified metadata and blob bytes.
 * Remaining: FileList, native file providers and JS wrappers.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_FILE_H
#define MULIB_WEB_FILE_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-web-blob.h"
typedef struct {mulib_web_blob_t blob;muba_buf name;int64_t modified;} mulib_web_file_t;
static inline int mulib_web_file_init(mulib_web_file_t *f,muba_span name,muba_span bytes,int64_t modified){if(!f||f->name.data||f->blob.bytes.data)return 0;mulib_web_file_t x={0};if(!muba_append(&x.name,name)||!mulib_web_blob_init(&x.blob,bytes)){muba_free(&x.name);mulib_web_blob_free(&x.blob);return 0;}x.modified=modified;*f=x;return 1;}
static inline void mulib_web_file_free(mulib_web_file_t *f){if(f){muba_free(&f->name);mulib_web_blob_free(&f->blob);}}
#endif /* MULIB_WEB_FILE_H */
