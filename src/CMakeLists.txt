include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${PROJECT_BINARY_DIR})

add_library(ohmycal STATIC
        globals.c
        str.c
        strlist.c
        ini.c
        conda.c
        environment.c
        utils.c
        system.c
        download.c
        deliverable.c
        recipe.c
        relocation.c
        wheel.c
        copy.c
        artifactory.c
        template.c
        rules.c
        docker.c
)

add_executable(omc
        omc_main.c
)
target_link_libraries(omc ohmycal)
add_executable(omc_indexer
        omc_indexer.c
)
target_link_libraries(omc_indexer ohmycal)
install(TARGETS omc omc_indexer RUNTIME)
