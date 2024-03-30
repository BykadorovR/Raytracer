# Terrain

GPU generated terrain based on heightmap texture. Samples shows different material settings.

![](https://media.giphy.com/media/9kFtsK5QWa7Y8v3NEJ/giphy.gif)

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