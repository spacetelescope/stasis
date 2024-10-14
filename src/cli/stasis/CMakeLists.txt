add_executable(stasis
        stasis_main.c
)
target_link_libraries(stasis PRIVATE stasis_core)
target_link_libraries(stasis PUBLIC LibXml2::LibXml2)

install(TARGETS stasis RUNTIME)
