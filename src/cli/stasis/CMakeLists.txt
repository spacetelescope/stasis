add_executable(stasis
        stasis_main.c
)
target_link_libraries(stasis PRIVATE
	stasis_entrypoint
)
target_include_directories(stasis PRIVATE
	${entrypoint_INCLUDE}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(stasis PUBLIC LibXml2::LibXml2)

install(TARGETS stasis RUNTIME)
