/** @file mulib-project-history.h
 * Bounded command undo/redo history with branching truncation.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: In-memory callbacks only; no persistence or cleanup of discarded commands.
 */
#ifndef MULIB_PROJECT_HISTORY_H
#define MULIB_PROJECT_HISTORY_H
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
typedef struct {void *ctx;bool (*undo)(void*);bool (*redo)(void*);} mulib_project_history_command;
typedef struct {mulib_project_history_command *commands;size_t count,cursor,capacity;} mulib_project_history;
/* commands borrows capacity elements. Callbacks must not reenter/mutate h and
 * must leave application state unchanged when returning false. */
static inline bool mulib_project_history_valid(mulib_project_history *h){return h&&h->cursor<=h->count&&h->count<=h->capacity&&h->capacity<=SIZE_MAX/sizeof *h->commands&&(!h->capacity||h->commands);}
/* Record already-applied command; discarded redo commands belong to caller. */
static inline bool mulib_project_history_push(mulib_project_history *h,mulib_project_history_command c){if(!mulib_project_history_valid(h)||h->cursor>=h->capacity||!c.undo||!c.redo)return false;h->commands[h->cursor++]=c;h->count=h->cursor;return true;}
static inline bool mulib_project_history_undo(mulib_project_history *h){if(!mulib_project_history_valid(h)||!h->cursor)return false;mulib_project_history_command c=h->commands[h->cursor-1];if(!c.undo||!c.undo(c.ctx))return false;h->cursor--;return true;}
static inline bool mulib_project_history_redo(mulib_project_history *h){if(!mulib_project_history_valid(h)||h->cursor==h->count)return false;mulib_project_history_command c=h->commands[h->cursor];if(!c.redo||!c.redo(c.ctx))return false;h->cursor++;return true;}
#ifdef __cplusplus
}
#endif
#endif
