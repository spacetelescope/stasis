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
target_include_directories(stasis PRIVATE
        ${core_INCLUDE}
        ${delivery_INCLUDE}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(stasis PUBLIC LibXml2::LibXml2)

install(TARGETS stasis RUNTIME)
