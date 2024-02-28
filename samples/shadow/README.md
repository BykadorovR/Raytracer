# Shadow

Simple implementation of shadows for point and directional lights. Only objects with PBR or Phong materials can be affected by shadows. Objects with any material can cast shadows.

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