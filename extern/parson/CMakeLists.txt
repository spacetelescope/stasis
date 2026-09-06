project(parson)
include_directories(${extern_parson_INCLUDE})
add_library(parson parson.c)
