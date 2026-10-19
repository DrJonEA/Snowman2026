
if (NOT DEFINED KW307_DIR)
    set(KW307_DIR "${CMAKE_CURRENT_LIST_DIR}/lib/KW307_SDK_v2.3_base")
endif()
if (NOT DEFINED KW307_PORT)
    set(KW307_PORT "${CMAKE_CURRENT_LIST_DIR}/port/kw307")
endif()


add_library(kw307 STATIC)

target_sources(kw307 PUBLIC
	${KW307_DIR}/core/KW307_api.c
	${KW307_DIR}/core/KW307_cmd.c
	${KW307_DIR}/core/KW307_io.c
	${KW307_DIR}/core/KW307_protocol.c
	${KW307_DIR}/core/KW307_uart.c
	${KW307_PORT}/port_uart.c
	)


# Add include directory
target_include_directories(kw307 PUBLIC 
   ${KW307_DIR}/core
   ${KW307_PORT}
)

# Add the standard library to the build
target_link_libraries(kw307 PUBLIC 
	pico_stdlib

	)
