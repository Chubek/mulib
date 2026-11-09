/* MuLIB test double, not an Android SDK header. */
#ifndef MULIB_TEST_ASSET_H
#define MULIB_TEST_ASSET_H
#include <stddef.h>
typedef struct AAssetManager AAssetManager;
typedef struct AAsset AAsset;
enum { AASSET_MODE_STREAMING = 2 };
AAsset *AAssetManager_open (AAssetManager *, const char *, int);
int AAsset_read (AAsset *, void *, size_t);
void AAsset_close (AAsset *);
#endif
