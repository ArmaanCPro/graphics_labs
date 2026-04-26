file(GLOB_RECURSE BASIC_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE BASIC_HEADERS CONFIGURE_DEPENDS src/*.h)

add_executable(basic ${BASIC_SOURCES} ${BASIC_HEADERS})

target_include_directories(basic PRIVATE src)

target_link_enger(basic)

set_project_warnings(basic)
set_project_settings(basic)
enable_sanitizers(basic)

compile_slang_shaders(basic_shaders "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
add_dependencies(basic basic_shaders)

add_custom_command(TARGET basic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:basic>/assets"
        COMMENT "Copying assets to binary directory."
        VERBATIM
)
