cmake_minimum_required(VERSION 3.15)
project(OMC C)
include(GNUInstallDirs)

set(CMAKE_C_STANDARD 99)
link_libraries(curl)
add_subdirectory(src)

set(SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/config.h @ONLY)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mission DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/omc)
