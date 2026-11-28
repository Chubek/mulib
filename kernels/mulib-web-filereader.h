/* MuLIB browser staging addition: web-filereader
 * Implemented subset: Synchronous blob-to-owned-buffer read.
 * Remaining: Asynchronous FileReader state, events, data URLs and abort.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_FILEREADER_H
#define MULIB_WEB_FILEREADER_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-web-blob.h"
static inline int mulib_web_filereader_bytes(const mulib_web_blob_t *b,muba_buf *out){if(!b||!out||out==&b->bytes)return 0;muba_buf t={0};if(!muba_append(&t,(muba_span){b->bytes.data,b->bytes.len}))return 0;return muba_commit(out,&t);}
#endif /* MULIB_WEB_FILEREADER_H */
