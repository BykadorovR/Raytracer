# Terrain

GPU generated terrain based on heightmap texture. Samples shows different material settings.

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