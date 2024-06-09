# Geodata

Shows interaction between terrain and playable entity. Terrain height information is used to place entity correctly (center of object is used).

![](https://media.giphy.com/media/J3qycwARqb4CvRzHPz/giphy.gif)

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