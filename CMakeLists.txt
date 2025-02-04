cmake_minimum_required(VERSION 3.15)
project(STASIS C)
include(GNUInstallDirs)

set(nix_cflags -Wall -Wextra -fPIC -D_GNU_SOURCE)
set(win_cflags /Wall)
set(CMAKE_C_STANDARD 99)
find_package(LibXml2)
find_package(CURL)

option(ASAN OFF)
if (ASAN)
	add_compile_options(-fsanitize=address)
	add_link_options(-fsanitize=address)
endif()

link_libraries(CURL::libcurl)
link_libraries(LibXml2::LibXml2)
include_directories(${LIBXML2_INCLUDE_DIR})

option(FORTIFY_SOURCE OFF)
if (FORTIFY_SOURCE)
	set(nix_cflags ${nix_cflags} -O -D_FORTIFY_SOURCE=1)
endif ()

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
	add_compile_options(${nix_cflags})
elseif (CMAKE_C_COMPILER_ID MATCHES "Clang")
	add_compile_options(${nix_cflags})
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(${win_cflags})
endif()

set(core_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/core/include)
set(delivery_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/delivery/include)
set(SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}")
configure_file(${PROJECT_SOURCE_DIR}/include/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/config.h @ONLY)
include_directories(${PROJECT_BINARY_DIR}/include)
add_subdirectory(src)

# Toggle extremely verbose output
option(BUILD_TESTING_DEBUG OFF)
if (BUILD_TESTING_DEBUG)
	add_compile_options(-DDEBUG)
endif()

# Toggle regression testing on/off
option(BUILD_TESTING_RT ON)

# Toggle testing
option(BUILD_TESTING OFF)
message(CHECK_START "Run unit tests")
if (BUILD_TESTING)
	message(CHECK_PASS "yes")
	enable_testing()
	message(CHECK_START "Run regression tests")
	if (BUILD_TESTING_RT)
		message(CHECK_PASS "yes")
	else()
		message(CHECK_PASS "no")
	endif()
	add_subdirectory(tests)
else()
	message(CHECK_PASS "no")
endif()

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/stasis_pandoc.css DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/stasis)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/stasis.ini DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/stasis)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mission DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/stasis)
