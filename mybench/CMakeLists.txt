cmake_minimum_required (VERSION 3.10)
project(mybench)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cachelib/cmake/")
# set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../opt/cachelib/lib/cmake/")
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../opt/cachelib/")

message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

# set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# add_subdirectory(../cachelib cachelib)

find_package(cachelib CONFIG REQUIRED)
find_package(fmt)

# add_library (mybenchlib reader.c)

add_executable(mybench main.cpp reader.cpp bench.cpp cache.cpp)
target_link_libraries(mybench cachelib fmt::fmt) 
