cmake_minimum_required(VERSION 3.20)
project(AeroHedge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Extreme optimization and strict warnings for HFT
set(CMAKE_CXX_FLAGS "-O3 -march=native -flto -Wall -Wextra -Wpedantic -Werror")

include_directories(include)

add_library(aerohedge_core src/memory_allocator.cpp)
target_include_directories(aerohedge_core PUBLIC include)

add_executable(aerohedge_engine src/main.cpp)
target_link_libraries(aerohedge_engine PRIVATE aerohedge_core)
