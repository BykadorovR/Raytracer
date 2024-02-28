# IBL

This sample demonstrates IBL, based on skybox color values. Used model is rendered as PBR.

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