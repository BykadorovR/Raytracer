# IBL

This sample demonstrates IBL, based on skybox color values. Used model is rendered as PBR.

![](https://media.giphy.com/media/v1.Y2lkPTc5MGI3NjExYWV0bTN6cGxwdTh0ZWtzMTc5aGdsaDNneGJqN3Y5dXMwamJucWl2dyZlcD12MV9pbnRlcm5hbF9naWZfYnlfaWQmY3Q9Zw/dILtvAQv2xpuy9SYWA/giphy.gif)

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