cmake_minimum_required (VERSION 3.10)
project(mybench)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option (FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." TRUE)

if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options (-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_compile_options (-fcolor-diagnostics)
    endif ()
endif ()


set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cachelib/cmake/")
# set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../opt/cachelib/lib/cmake/")
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../opt/cachelib/")

message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

# set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# add_subdirectory(../cachelib cachelib)

find_package(cachelib CONFIG REQUIRED)
find_package(fmt)

# add_library (mybenchlib reader.c)
add_executable(strictlru main.cpp reader.cpp bench.cpp benchMT.cpp cache.cpp)
target_compile_definitions(strictlru PRIVATE USE_STRICTLRU)
target_link_libraries(strictlru cachelib fmt::fmt atomic) 

add_executable(lru main.cpp reader.cpp bench.cpp benchMT.cpp cache.cpp)
target_compile_definitions(lru PRIVATE USE_LRU)
target_link_libraries(lru cachelib fmt::fmt atomic) 

add_executable(twoq main.cpp reader.cpp bench.cpp benchMT.cpp cache.cpp)
target_compile_definitions(twoq PRIVATE USE_TWOQ)
target_link_libraries(twoq cachelib fmt::fmt atomic) 

add_executable(tinylfu main.cpp reader.cpp bench.cpp benchMT.cpp cache.cpp)
target_compile_definitions(tinylfu PRIVATE USE_TINYLFU)
target_link_libraries(tinylfu cachelib fmt::fmt atomic)

add_executable(clock main.cpp reader.cpp bench.cpp benchMT.cpp cache.cpp)
target_compile_definitions(clock PRIVATE USE_CLOCK)
target_link_libraries(clock cachelib fmt::fmt atomic)

add_executable(sieve main.cpp reader.cpp bench.cpp benchMT.cpp cache.cpp)
target_compile_definitions(sieve PRIVATE USE_SIEVE)
target_link_libraries(sieve cachelib fmt::fmt atomic)

add_executable(sievebuffered main.cpp reader.cpp bench.cpp benchMT.cpp cache.cpp)
target_compile_definitions(sievebuffered PRIVATE USE_SIEVEBUFFERED)
target_link_libraries(sievebuffered cachelib fmt::fmt atomic)

add_executable(s3fifo main.cpp reader.cpp bench.cpp benchMT.cpp cache.cpp)
target_compile_definitions(s3fifo PRIVATE USE_S3FIFO)
target_link_libraries(s3fifo cachelib fmt::fmt atomic) 
