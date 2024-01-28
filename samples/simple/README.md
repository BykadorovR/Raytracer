# Simple primitives

This sample demonstrates different light models: simple coloring/texturing, Blinn-Phong, PBR using simple primitives: cube and sphere. Direct and point lights are used as light sources.

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