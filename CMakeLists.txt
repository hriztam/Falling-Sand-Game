cmake_minimum_required(VERSION 3.10)
project(FallingSand)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find SFML
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

add_executable(app src/main.cpp)

target_link_libraries(app PRIVATE SFML::Graphics)
