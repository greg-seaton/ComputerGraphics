# Computer Graphics Engine

A software renderer and ray tracer developed in C++ as part of a Computer Graphics project. The project explores the core techniques used in modern rendering pipelines, including rasterisation, lighting models, camera transformations, texture mapping, and physically-inspired ray tracing.

## Features

### Rasterisation

* Line and triangle rasterisation
* Colour interpolation across surfaces
* Texture mapping using barycentric coordinates
* Depth buffering and occlusion handling

### Camera System

* 3D camera positioning and movement
* Orbit camera controls
* Look-at camera orientation
* Perspective projection

### Ray Tracing

* Ray-triangle intersection testing
* Shadow ray generation
* Recursive reflections
* Multiple light source support

### Shading and Materials

* Flat shading
* Gouraud shading
* Phong illumination model
* Metallic and reflective materials
* Angle-of-incidence lighting
* Specular highlights
* Normal maps

### Environment Rendering

* Skybox rendering
* Environment mapping

## Technical Highlights

* Implemented custom vector and matrix mathematics for geometric transformations.
* Developed a complete rendering pipeline from 3D model loading through to final image generation.
* Implemented both rasterisation-based and ray-traced rendering approaches for comparison.
* Built support for OBJ geometry and material file parsing.
* Developed interpolation systems for colour, depth, texture coordinates, and lighting values.

## Technologies

* C++
* SDL2
* OBJ/MTL asset pipeline

## Demo

### Reflections
![Reflections](renderFinalP1.gif)

### Camera Movement
![Camera Movement](renderFinalP2.gif)

### Shading & Materials
![Shading](renderFinalP3.gif)
