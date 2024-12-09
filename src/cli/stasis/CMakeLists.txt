include_directories(${CMAKE_SOURCE_DIR})
add_executable(stasis
        stasis_main.c
        args.c
        callbacks.c
        system_requirements.c
        tpl.c
)
target_link_libraries(stasis PRIVATE
        stasis_core
        stasis_delivery
)
target_link_libraries(stasis PUBLIC LibXml2::LibXml2)

install(TARGETS stasis RUNTIME)
