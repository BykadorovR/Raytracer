# Skeletal animation and static models

This sample demonstrates skeletal animations and static models with different lighting models.

![](https://media.giphy.com/media/v1.Y2lkPTc5MGI3NjExaW52YnB2MTZoZjd4YnhicDl2bnAzZDE4amZva3p5aXJjYXRrZ3ZreCZlcD12MV9pbnRlcm5hbF9naWZfYnlfaWQmY3Q9Zw/INSIlY1MzKw2dlDOB4/giphy.gif)

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