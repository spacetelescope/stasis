function(set_version_fallback)
    message(WARNING "Version information not available. Using fallback...")
    set(PROJECT_VERSION "0.0.0")
    set(PROJECT_VERSION ${PROJECT_VERSION} PARENT_SCOPE)
    set(PROJECT_VERSION_BRANCH "unknown")
    set(PROJECT_VERSION_BRANCH ${PROJECT_VERSION_BRANCH} PARENT_SCOPE)
endfunction()

function(get_version_from_git)
    find_package(Git QUIET)
    if(NOT Git_FOUND)
        message(WARNING "Git not found.")
        set_version_fallback()
        return()
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --long --dirty --tags --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL 0)
        message(WARNING "Failed to get git describe info")
        set_version_fallback()
        return()
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL 0)
        message(WARNING "Failed to get git branch")
        set_version_fallback()
        return()
    endif()

    string(REGEX REPLACE "^v" "" CLEAN_TAG "${GIT_TAG}")
    set(PROJECT_VERSION ${CLEAN_TAG})
    set(PROJECT_VERSION ${CLEAN_TAG} PARENT_SCOPE)
    set(PROJECT_VERSION_BRANCH ${GIT_BRANCH})
    set(PROJECT_VERSION_BRANCH ${GIT_BRANCH} PARENT_SCOPE)
endfunction()