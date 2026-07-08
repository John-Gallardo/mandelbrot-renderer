#pragma once
#include <cstdint>  // for uint32_t
#include <string_view>

namespace Config{
    inline constexpr uint32_t windowWidth     {800};
    inline constexpr uint32_t windowHeight    {600};
    inline constexpr std::string_view appTitle{"Mandelbrot Renderer"};
}
