#pragma once

#include <vector>
#include <string>
#include "iostream"
#ifdef __ANDROID__
#include <android/asset_manager.h>
#else
#include <fstream>
#endif
class Filesystem {
 private:
#ifdef __ANDROID__
  AAssetManager* _assetManager;
  template <class T>
  std::vector<T> _readFileAndroid(const std::string& filename) {
    AAssetDir* assetDir = AAssetManager_openDir(_assetManager, "shaders/shape");
    auto name = AAssetDir_getNextFileName(assetDir);
    while (name != nullptr) {
      std::cout << name << std::endl;
      name = AAssetDir_getNextFileName(assetDir);
    }

    AAsset* file = AAssetManager_open(_assetManager, filename.c_str(), AASSET_MODE_BUFFER);
    if (file == nullptr) throw std::runtime_error("failed to open file " + filename);
    size_t fileLength = AAsset_getLength(file);
    std::vector<T> fileContent(fileLength);
    AAsset_read(file, fileContent.data(), fileLength);
    AAsset_close(file);
    return fileContent;
  }
#else
  std::vector<char> _readFileDesktop(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open file " + filename);
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
  }
#endif
 public:
#ifdef __ANDROID__
  void setAssetManager(AAssetManager* assetManager);
#endif
  template <class T>
  std::vector<T> readFile(const std::string& filename) {
#ifdef __ANDROID__
    return _readFileAndroid<T>(filename);
#else
    return _readFileDesktop(filename);
#endif
  }
};
