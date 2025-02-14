add_library(stasis_entrypoint STATIC
        stasis_entrypoint.c
	args.c
	entrypoint_callbacks.c
	system_requirements.c
	tpl.c
)
target_link_libraries(stasis_entrypoint PRIVATE
        stasis_delivery
)
target_include_directories(stasis_entrypoint PRIVATE
        ${core_INCLUDE}
        ${delivery_INCLUDE}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(stasis_entrypoint PUBLIC LibXml2::LibXml2)
