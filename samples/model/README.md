# Skeletal animation and static models

This sample demonstrates skeletal animations and static models with different lighting models.

![](https://media.giphy.com/media/INSIlY1MzKw2dlDOB4/giphy.gif)

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