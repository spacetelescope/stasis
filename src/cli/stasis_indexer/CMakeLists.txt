add_executable(stasis_indexer
        args.c
        stasis_indexer_main.c
        callbacks.c
        helpers.c
        junitxml_report.c
        website.c
        website.h
        readmes.c
        readmes.h
)
target_include_directories(stasis_indexer PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(stasis_indexer PRIVATE
        stasis_core
        stasis_delivery
)

install(TARGETS stasis_indexer RUNTIME)
