function(embed_resources TARGET)
    set(RESOURCES ${ARGN})
    list(LENGTH RESOURCES RESOURCES_LEN)
    math(EXPR RESOURCES_ODD "${RESOURCES_LEN} % 2")
    if(RESOURCES_LEN EQUAL 0 OR RESOURCES_ODD)
        message(FATAL_ERROR "embed_resources() expects pairs of resource URL and file path.")
    endif()

    if(APPLE)
        set(SECTION_DIRECTIVE ".const_data")
        set(SYMBOL_PREFIX "_")
    else()
        set(SECTION_DIRECTIVE ".section .rodata")
        set(SYMBOL_PREFIX "")
    endif()

    set(RESOURCE_ASM "")
    set(RESOURCE_DECLS "")
    set(RESOURCE_LOOKUP "")
    set(RESOURCE_RC "")
    set(RESOURCE_FILES "")
    set(RESOURCE_ID 1000)
    while(RESOURCES)
        list(POP_FRONT RESOURCES RESOURCE_URL RESOURCE_PATH)
        list(APPEND RESOURCE_FILES "${RESOURCE_PATH}")
        if(WIN32)
            string(APPEND RESOURCE_RC "${RESOURCE_ID} RCDATA \"${RESOURCE_PATH}\"\n")
            string(APPEND RESOURCE_LOOKUP "  if(url == \"${RESOURCE_URL}\")\n    return LoadRcData(${RESOURCE_ID});\n")
        else()
            set(SYMBOL "pcbu_resource_${RESOURCE_ID}")
            string(APPEND RESOURCE_ASM
                    ".globl ${SYMBOL_PREFIX}${SYMBOL}\n"
                    ".globl ${SYMBOL_PREFIX}${SYMBOL}_end\n"
                    ".balign 16\n"
                    "${SYMBOL_PREFIX}${SYMBOL}:\n"
                    ".incbin \"${RESOURCE_PATH}\"\n"
                    "${SYMBOL_PREFIX}${SYMBOL}_end:\n")
            string(APPEND RESOURCE_DECLS
                    "extern \"C\" const uint8_t ${SYMBOL}[];\n"
                    "extern \"C\" const uint8_t ${SYMBOL}_end[];\n")
            string(APPEND RESOURCE_LOOKUP "  if(url == \"${RESOURCE_URL}\")\n    return {${SYMBOL}, ${SYMBOL}_end};\n")
        endif()
        math(EXPR RESOURCE_ID "${RESOURCE_ID} + 1")
    endwhile()

    set(RESOURCE_CPP "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_resources.cpp")
    set(CPP_CONTENT "#include \"utils/ResourceHelper.h\"\n\n")
    if(NOT WIN32)
        string(APPEND CPP_CONTENT "__asm__(R\"(\n${SECTION_DIRECTIVE}\n${RESOURCE_ASM}.text\n)\");\n\n${RESOURCE_DECLS}\n")
    endif()
    string(APPEND CPP_CONTENT "std::span<const uint8_t> ResourceHelper::GetResourceData(const std::string &url) {\n${RESOURCE_LOOKUP}  return {};\n}\n")
    file(CONFIGURE OUTPUT "${RESOURCE_CPP}" CONTENT "${CPP_CONTENT}" @ONLY)
    target_sources(${TARGET} PRIVATE "${RESOURCE_CPP}")

    if(WIN32)
        set(RESOURCE_RC_TEMPLATE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_resources.rc.in")
        set(RESOURCE_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_resources.rc")
        file(CONFIGURE OUTPUT "${RESOURCE_RC_TEMPLATE}" CONTENT "${RESOURCE_RC}" @ONLY)
        add_custom_command(
                OUTPUT "${RESOURCE_RC_FILE}"
                COMMAND ${CMAKE_COMMAND} -E copy "${RESOURCE_RC_TEMPLATE}" "${RESOURCE_RC_FILE}"
                COMMAND ${CMAKE_COMMAND} -E touch "${RESOURCE_RC_FILE}"
                DEPENDS "${RESOURCE_RC_TEMPLATE}" ${RESOURCE_FILES}
                VERBATIM
        )
        add_custom_target(${TARGET}_resources DEPENDS "${RESOURCE_RC_FILE}")
        add_dependencies(${TARGET} ${TARGET}_resources)
        target_sources(${TARGET} INTERFACE "${RESOURCE_RC_FILE}")
    else()
        set_source_files_properties("${RESOURCE_CPP}" PROPERTIES OBJECT_DEPENDS "${RESOURCE_FILES}")
    endif()
endfunction()
