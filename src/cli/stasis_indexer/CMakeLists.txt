add_executable(stasis_indexer
        stasis_indexer_main.c
)
target_include_directories(stasis_indexer PRIVATE
        ${core_INCLUDE}
        ${delivery_INCLUDE}
	${index_INCLUDE}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(stasis_indexer PRIVATE
        stasis_index
)

install(TARGETS stasis_indexer RUNTIME)
