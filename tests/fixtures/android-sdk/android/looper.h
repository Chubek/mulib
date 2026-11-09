/* MuLIB test double, not an Android SDK header. */
#ifndef MULIB_TEST_LOOPER_H
#define MULIB_TEST_LOOPER_H
typedef struct ALooper ALooper;
typedef int (*ALooper_callbackFunc) (int, int, void *);
enum { ALOOPER_PREPARE_ALLOW_NON_CALLBACKS = 1, ALOOPER_POLL_ERROR = -4 };
ALooper *ALooper_prepare (int);
int ALooper_addFd (ALooper *, int, int, int, ALooper_callbackFunc, void *);
int ALooper_removeFd (ALooper *, int);
int ALooper_pollOnce (int, int *, int *, void **);
#endif
