#pragma once
#include <cstdint>  // for uint32_t
#include <string_view>

namespace Config{
    inline constexpr uint32_t windowWidth     {1920};
    inline constexpr uint32_t windowHeight    {1080};
    inline constexpr std::string_view appTitle{"Mandelbrot Renderer"};
    inline constexpr int maxFramesInFlight    {2};
}
