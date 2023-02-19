cmake_minimum_required (VERSION 3.10)
project(mybench)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

find_package(cachelib CONFIG REQUIRED)
find_package(fmt)

# add_library (mybenchlib reader.c)

add_executable(mybench main.cpp reader.c cache.cpp)
target_link_libraries(mybench cachelib fmt::fmt) 
