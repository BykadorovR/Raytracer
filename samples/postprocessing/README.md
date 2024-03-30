# Postprocessing

Gamma, tone-mapping and bloom based on blur.

![](https://media.giphy.com/media/JJ4g8ol6s28fQfZlNn/giphy.gif)

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