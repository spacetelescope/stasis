include_directories(
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/include
)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/tests)
set(CTEST_BINARY_DIRECTORY ${PROJECT_BINARY_DIR}/tests)

file(GLOB files "test_*.c")

foreach(file ${files})
    string(REGEX REPLACE "(^.*/|\\.[^.]*$)" "" file_without_ext ${file})
    add_executable(${file_without_ext} ${file})
    if(NOT(MSVC))
        target_compile_options(${file_without_ext} PRIVATE
                -Wno-unused-parameter
                -Wno-unused-but-set-variable
                -Wno-incompatible-pointer-types-discards-qualifiers
                -Wno-discarded-qualifiers)
    endif()
    target_link_libraries(${file_without_ext} PRIVATE ohmycal)
    add_test(${file_without_ext} ${file_without_ext})
    set_tests_properties(${file_without_ext}
            PROPERTIES
            TIMEOUT 120)
    set_tests_properties(${file_without_ext}
            PROPERTIES
            SKIP_RETURN_CODE 127)
endforeach()