cmake_minimum_required(VERSION 3.15)
project(OMC C)
include(GNUInstallDirs)

set(nix_cflags -Wall -Wextra -fPIC)
set(win_cflags /Wall)
set(CMAKE_C_STANDARD 99)

link_libraries(curl)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
	message("gnu options")
	add_compile_options(${nix_cflags})
elseif (CMAKE_C_COMPILER_ID MATCHES "Clang")
	add_compile_options(${nix_cflags})
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
	message("microsoft visual c options")
	add_compile_options(${win_cflags})
endif()

add_subdirectory(src)

set(SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/config.h @ONLY)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/omc.ini DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/omc)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mission DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/omc)
