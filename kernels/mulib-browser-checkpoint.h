/* MuLIB browser staging addition: browser-checkpoint
 * Implemented subset: Capture journal sequence, document and frame identifiers.
 * Remaining: Atomic DOM/style/layout snapshot, quiescence and retained payload.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_CHECKPOINT_H
#define MULIB_BROWSER_CHECKPOINT_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-browser-journal.h"
typedef struct {uint64_t sequence,document,frame;} mulib_browser_checkpoint_t;
static inline int mulib_browser_checkpoint_capture(const mulib_browser_journal_t *j,uint64_t document,uint64_t frame,mulib_browser_checkpoint_t *out){if(!j||!document||!out)return 0;*out=(mulib_browser_checkpoint_t){j->seq,document,frame};return 1;}
#endif /* MULIB_BROWSER_CHECKPOINT_H */
