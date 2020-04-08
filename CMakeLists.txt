cmake_minimum_required(VERSION 3.10)
project(ztnc CXX)

add_subdirectory(libzt)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")

add_executable(ztnc ztnc.cpp)
target_link_libraries(ztnc PRIVATE zt-static)
target_include_directories(ztnc PRIVATE libzt/include)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.61.0 COMPONENTS program_options filesystem REQUIRED)

if(Boost_FOUND)
    target_include_directories(ztnc PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_libraries(ztnc PRIVATE ${Boost_LIBRARIES})
endif()
