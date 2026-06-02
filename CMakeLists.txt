cmake_minimum_required(VERSION 3.16)
project(CameraWSLVision VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV COMPONENTS core imgproc highgui videoio imgcodecs QUIET)
if(NOT OpenCV_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(OPENCV4 QUIET opencv4)
    if(NOT OPENCV4_FOUND)
        pkg_check_modules(OPENCV4 QUIET opencv)
    endif()
    if(OPENCV4_FOUND)
        message(STATUS "Found OpenCV via pkg-config: ${OPENCV4_VERSION}")
        set(OpenCV_FOUND TRUE)
        set(OpenCV_INCLUDE_DIRS ${OPENCV4_INCLUDE_DIRS})
        set(OpenCV_LIBS ${OPENCV4_LIBRARIES})
        add_compile_options(${OPENCV4_CFLAGS_OTHER})
        link_directories(${OPENCV4_LIBRARY_DIRS})
    endif()
endif()

if(NOT OpenCV_FOUND)
    message(FATAL_ERROR
        "OpenCV not found. Install OpenCV development files or set OpenCV_DIR/CMAKE_PREFIX_PATH.\n"
        "On Ubuntu/Debian: sudo apt install libopencv-dev pkg-config\n"
        "If OpenCV is installed in a custom location, set OpenCV_DIR or CMAKE_PREFIX_PATH to the directory containing OpenCVConfig.cmake.")
endif()

add_executable(CameraApp src/main.cpp)

target_include_directories(CameraApp PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(CameraApp PRIVATE ${OpenCV_LIBS})

target_compile_options(CameraApp PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /Zc:__cplusplus>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)
