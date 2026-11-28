#ifndef ON1X_SDK_AST_VISITOR_H
#define ON1X_SDK_AST_VISITOR_H

#include "On1xSDK-AST.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_ASTVisitOrder;
enum {
    ON1X_SDK_AST_PREORDER = 1,
    ON1X_SDK_AST_POSTORDER = 2
};

/*
 * Called synchronously with borrowed values. Returning a known non-OK status
 * stops traversal; an unknown numeric status is converted to ON1X_SDK_ERROR.
 */
typedef On1xSDK_Status (*On1xSDK_ASTVisitorFn)(
    void *userdata,
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode node,
    size_t depth);

On1xSDK_Status on1x_sdk_ast_visit(
    const On1xSDK_AST *ast,
    On1xSDK_ASTNode root,
    On1xSDK_ASTVisitOrder order,
    On1xSDK_ASTVisitorFn visitor,
    void *userdata);

#ifdef __cplusplus
}
#endif

#endif
