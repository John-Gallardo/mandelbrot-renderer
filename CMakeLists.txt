cmake_minimum_required(VERSION 3.15)

project(main)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

set(SOURCE src/main.cpp src/App.cpp)
add_executable(${PROJECT_NAME} ${SOURCE})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw)
