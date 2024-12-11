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
target_include_directories(stasis_delivery PRIVATE
        ${core_INCLUDE}
        ${delivery_INCLUDE}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(stasis_delivery stasis_core)