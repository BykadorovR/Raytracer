# Sprite

This sample demonstrates simple static sprites with and without alpha. To avoid alpha blending artifacts, sorting algorithm (from far to near) is used.

# How to install

## Build

- Download assets
```
python download.py
```
- Build project
```
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=../../cmake ..
cmake --build .