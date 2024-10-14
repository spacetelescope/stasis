add_executable(stasis_indexer
        stasis_indexer.c
)
target_link_libraries(stasis_indexer PRIVATE stasis_core)

install(TARGETS stasis_indexer RUNTIME)
