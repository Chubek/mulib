/* MuLIB browser staging addition: web-permissions-policy
 * Implemented subset: Explicit feature mask intersection and permission check.
 * Remaining: Policy parsing, origin allowlists, inherited policies and enforcement.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_PERMISSIONS_POLICY_H
#define MULIB_WEB_PERMISSIONS_POLICY_H
#include "detail/mulib-browser-additions-common.h"

static inline uint64_t mulib_web_permissions_policy_inherit(uint64_t parent,uint64_t own){return parent&own;}
static inline int mulib_web_permissions_policy_allows(uint64_t mask,unsigned feature){return feature<64&&((mask>>feature)&1u);}
#endif /* MULIB_WEB_PERMISSIONS_POLICY_H */
