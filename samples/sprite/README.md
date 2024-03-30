# Sprite

This sample demonstrates simple static sprites with and without alpha. To avoid alpha blending artifacts, sorting algorithm (from far to near) is used.

![](https://media.giphy.com/media/0tzpThYXGigkLVKleW/giphy.gif)

# How to install

## Build

- Download assets

  If assets were downloaded during engine setup, this step can be skipped then, otherwise:
  ```
  python download.py
  ```
- Build project
  ```
  mkdir build
  cd build
  cmake -DCMAKE_PREFIX_PATH=../../cmake ..
  cmake --build .
  ```