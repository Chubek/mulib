/* MuLIB browser staging addition: web-opfs
 * Implemented subset: Reject unsafe path components for provider-relative filesystem operations.
 * Remaining: Origin-scoped filesystem, handles, sync access and symlink-safe broker.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_OPFS_H
#define MULIB_WEB_OPFS_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_opfs_component(muba_span s){if(!muba_span_ok(s)||!s.len||muba_eq(s,muba_str("."))||muba_eq(s,muba_str("..")))return 0;for(size_t i=0;i<s.len;i++)if(s.data[i]==0||s.data[i]=='/'||s.data[i]=='\\')return 0;return 1;}
#endif /* MULIB_WEB_OPFS_H */
