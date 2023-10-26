cmake_minimum_required(VERSION 3.21)
project(ohmycal C)

set(CMAKE_C_STANDARD 99)
link_libraries(curl)
add_subdirectory(src)