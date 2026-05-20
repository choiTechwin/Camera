cmake_minimum_required(VERSION 3.16)

project(Camera VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(OpenCV REQUIRED)

add_executable(CameraApp
    src/main.cpp
)

target_include_directories(CameraApp PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(CameraApp PRIVATE
    ${OpenCV_LIBS}
)

target_compile_options(CameraApp PRIVATE
    -Wall
    -Wextra
)

target_compile_options(CameraApp PRIVATE
    $<$<CONFIG:Debug>:-O0;-g>
    $<$<CONFIG:Release>:-O3>
)
