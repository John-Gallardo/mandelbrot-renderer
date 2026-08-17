# Mandelbrot Renderer
A Vulkan-based Mandelbrot renderer using the primitive graphics pipeline (vertex & fragment shaders). I use C++23, Vulkan-HPP's RAII bindings, GLFW for windowing, and Slang for the shaders. Note that only Linux was tested.

# Features
- Movement using WASD, and zooming using + and -
- Window resizing and automatic aspect ratio correction
  
# Video Demo
- TODO

# Build Instructions
1. Vulkan, GLFW, and a compiler supporting C++23 are required to be installed
2. Go to the root directory & run:
```
cmake -Bbuild && cmake --build build && ./build/main
```

# References
- [Khronos Vulkan Tutorial](https://docs.vulkan.org/tutorial/latest/00_Introduction.html)
- [Learn OpenGL](https://learnopengl.com/) (for GLFW usage)
