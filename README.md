# Vulkan Engine

[![ClangFormat](https://github.com/BykadorovR/Raytracer/actions/workflows/clang-format-check.yml/badge.svg)](https://github.com/BykadorovR/Raytracer/actions/workflows/clang-format-check.yml)

# What's this project about

This project is a graphics engine utilizing the Vulkan API, offering a comprehensive set of features to facilitate the development of visually compelling applications. The engine includes support for terrain rendering, various lighting models, particle systems, skeletal animation and more.

# Structure

- **dev/master** branch contains classic rasterization engine
- **raytracing** branch contains Path Tracer with importance sampling using compute shader
- https://github.com/BykadorovR/VulkanEngine/tree/master/samples set of samples which demonstates how to use the engine

# Architecture

[Draw.io diagrams](https://drive.google.com/file/d/1eI9kWqMdEsKQL0aQhGGtAJed9jmwzZk7/view?usp=sharing)

# How to build

Currently Windows is the only tested and supported OS.

- Clone the project
- mkdir build && cd build && cmake ..
- Build the project
- Call python compile.py path/to/vulkan/compiler/glslc.exe to compile shaders
- Call python download.py to download assets
