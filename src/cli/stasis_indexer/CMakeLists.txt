add_executable(stasis_indexer
        args.c
        stasis_indexer_main.c
        callbacks.c
        helpers.c
        junitxml_report.c
        website.c
        readmes.c
)
target_include_directories(stasis_indexer PRIVATE
        ${core_INCLUDE}
        ${delivery_INCLUDE}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(stasis_indexer PRIVATE
        stasis_delivery
)

install(TARGETS stasis_indexer RUNTIME)
