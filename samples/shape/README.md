# Shape primitives

This sample demonstrates different lighting models: simple coloring/texturing, Blinn-Phong, PBR using simple primitives: cube and sphere. Direct and point lights are used as light sources.

![](https://media.giphy.com/media/WNxu1YsdBWca4A5jKn/giphy.gif)
![](https://media.giphy.com/media/Hw72BHfihIqkTKr9z4/giphy.gif)

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