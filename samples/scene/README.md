# Scene

Complex scene, main playground.

![](https://media.giphy.com/media/g1u76El75GTR5xp6zI/giphy.gif)

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