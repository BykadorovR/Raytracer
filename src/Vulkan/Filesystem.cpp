#include "Filesystem.h"

#ifdef __ANDROID__
void Filesystem::setAssetManager(AAssetManager* assetManager) { _assetManager = assetManager; }
#endif