find_program(SLANGC_EXECUTABLE slangc
    HINTS
        "$ENV{VULKAN_SDK}/bin"
    REQUIRED
)
message(STATUS "Found slangc: ${SLANGC_EXECUTABLE}")

function(compile_slang_shaders target shader_dir output_dir)
    file(GLOB_RECURSE SLANG_SOURCES CONFIGURE_DEPENDS "${shader_dir}/*.slang")

    if (NOT SLANG_SOURCES)
        message(WARNING "compile_slang_shaders: no .slang files found in ${SHADER_DIR}")
        return()
    endif()

    set(COMMON_FLAGS
        -target spirv
        -profile spirv_1_4
        -emit-spirv-directly
        -matrix-layout-column-major # GLM is column-major, but Slang is better as row-major. Consider transposing glm matrices later, for now we enforce column-major.
        -fvk-use-entrypoint-name
        -fvk-use-gl-layout
        -bindless-space-index 0
    )

    set(SPV_OUTPUTS "")

    foreach(SLANG_FILE IN LISTS SLANG_SOURCES)
        cmake_path(GET SLANG_FILE STEM SHADER_STEM)
        set(SPV_OUT "${output_dir}/${SHADER_STEM}.spv")

        set(OPT_FLAG $<IF:$<CONFIG:Debug>,-O0,-O2>)
        set(DEBUG_FLAGS $<IF:$<CONFIG:Debug>,-g3,-g0>)

        add_custom_command(
                OUTPUT "${SPV_OUT}"
                COMMAND "${SLANGC_EXECUTABLE}"
                    ${SLANG_FILE}
                    ${OPT_FLAG}
                    ${DEBUG_FLAGS}
                    ${COMMON_FLAGS}
                    -o "${SPV_OUT}"
                COMMAND
                    ${CMAKE_COMMAND} -E make_directory "${output_dir}"
                DEPENDS "${SLANG_FILE}" # if I ever use Slang imports, I would need to find a way to do dependency tracking. Or just depend on all Slang files ${SLANG_SOURCES}
                COMMENT "Compiling Slang shader: ${SHADER_STEM}.slang -> ${SHADER_STEM}.spv"
                VERBATIM
        )

        list(APPEND SPV_OUTPUTS "${SPV_OUT}")
    endforeach()

    add_custom_target(${target} ALL DEPENDS ${SPV_OUTPUTS})
endfunction()