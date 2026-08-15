#pragma once
#include <cstdint>  // for uint32_t
#include <string_view>

namespace Config{
    inline constexpr uint32_t windowWidth     {1000};
    inline constexpr uint32_t windowHeight    {1000};
    inline constexpr std::string_view appTitle{"Mandelbrot Renderer"};
    inline constexpr int maxFramesInFlight    {2};
    inline constexpr double moveConstant      {0.5};
    inline constexpr double zoomRate          {2};
}
