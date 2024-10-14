include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${PROJECT_BINARY_DIR})

add_subdirectory(lib)
add_subdirectory(cli)

