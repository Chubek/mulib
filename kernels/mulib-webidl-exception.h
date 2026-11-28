/* MuLIB browser staging addition: webidl-exception
 * Implemented subset: DOMException name lookup for selected legacy codes.
 * Remaining: Complete DOMException values, JS errors and realm prototypes.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEBIDL_EXCEPTION_H
#define MULIB_WEBIDL_EXCEPTION_H
#include "detail/mulib-browser-additions-common.h"

static inline const char *mulib_webidl_exception_name(unsigned code){switch(code){case 1:return "IndexSizeError";case 3:return "HierarchyRequestError";case 4:return "WrongDocumentError";case 5:return "InvalidCharacterError";case 8:return "NotFoundError";case 9:return "NotSupportedError";case 11:return "InvalidStateError";case 12:return "SyntaxError";case 14:return "NamespaceError";case 18:return "SecurityError";case 20:return "AbortError";case 22:return "QuotaExceededError";default:return NULL;}}
#endif /* MULIB_WEBIDL_EXCEPTION_H */
