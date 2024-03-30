# IBL

This sample demonstrates IBL, based on skybox color values. Used model is rendered as PBR.

![](https://media.giphy.com/media/WK8R12t5jQfdinyTT6/giphy.gif)

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