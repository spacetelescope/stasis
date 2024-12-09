include_directories(${PROJECT_BINARY_DIR})

add_library(stasis_delivery STATIC
        delivery_postprocess.c
        delivery_conda.c
        delivery_docker.c
        delivery_install.c
        delivery_artifactory.c
        delivery_test.c
        delivery_build.c
        delivery_show.c
        delivery_populate.c
        delivery_init.c
        delivery.c
)
target_link_libraries(stasis_delivery stasis_core)