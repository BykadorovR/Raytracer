# Shadow

Simple implementation of shadows for point and directional lights. Only objects with PBR or Phong materials can be affected by shadows. Objects with any material can cast shadows.

![](https://media.giphy.com/media/V3yJDZbdBNGoCsPl9Y/giphy.gif)

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