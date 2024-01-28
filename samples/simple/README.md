# Simple primitives

This sample demonstrates different light models: simple coloring/texturing, Blinn-Phong, PBR using simple primitives: cube and sphere. Direct and point lights are used as light sources.

![](https://media.giphy.com/media/v1.Y2lkPTc5MGI3NjExdjBlOHVkdHgzMHp0azF4ajdnMDAzOTBwZHJrdXhrNWt3eGtueThzeSZlcD12MV9pbnRlcm5hbF9naWZfYnlfaWQmY3Q9Zw/4vdciHdGZ26bPBSjo6/giphy.gif)

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