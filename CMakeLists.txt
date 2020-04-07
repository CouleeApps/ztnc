cmake_minimum_required(VERSION 3.10)
project(ztnc CXX)

add_subdirectory(libzt)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")

add_executable(ztnc ztnc.cpp)
target_link_libraries(ztnc zt-static)
target_include_directories(ztnc PRIVATE libzt/include)