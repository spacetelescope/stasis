add_library(stasis_index STATIC
        args.c
        helpers.c
        index_callbacks.c
        junitxml_report.c
        readmes.c
        stasis_index_entrypoint.c
        website.c

)
target_link_libraries(stasis_index PRIVATE
        stasis_delivery
)
target_include_directories(stasis_index PRIVATE
        ${core_INCLUDE}
        ${delivery_INCLUDE}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(stasis_index PUBLIC LibXml2::LibXml2)
