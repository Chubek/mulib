/* MuLIB browser staging addition: web-opfs
 * Implemented subset: Lexical component and bounded slash-delimited relative path checks.
 * Remaining: Origin-scoped filesystem, handles, sync access and symlink-safe broker.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_OPFS_H
#define MULIB_WEB_OPFS_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_opfs_component(muba_span s){if(!muba_span_ok(s)||!s.len||muba_eq(s,muba_str("."))||muba_eq(s,muba_str("..")))return 0;for(size_t i=0;i<s.len;i++)if(s.data[i]==0||s.data[i]=='/'||s.data[i]=='\\')return 0;return 1;}

/* Byte length (including separators) and component count have literal
 * limits. No decoding, normalization, filesystem access or authorization. */
static inline int mulib_web_opfs_relative_path(muba_span path, size_t max_bytes,
    size_t max_components)
{
    if (!path.data || !path.len || path.len > max_bytes || !max_components)
        return 0;
    size_t start = 0;
    for (size_t i = 0; i < path.len; i++) {
        if (path.data[i] == ':')
            return 0;
        if (path.data[i] == '/') {
            muba_span component = {path.data + start, i - start};
            if (!mulib_web_opfs_component(component) || !--max_components)
                return 0;
            start = i + 1;
        }
    }
    muba_span tail = {path.data + start, path.len - start};
    return mulib_web_opfs_component(tail);
}
#endif /* MULIB_WEB_OPFS_H */
