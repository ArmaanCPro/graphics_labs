file(GLOB_RECURSE ENGER_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE ENGER_HEADERS CONFIGURE_DEPENDS src/*.h)

add_library(enger SHARED)

target_sources(
        enger
        PRIVATE ${ENGER_SOURCES}
        PUBLIC
            FILE_SET HEADERS
                FILES ${ENGER_HEADERS}
            FILE_SET generated_headers
                TYPE HEADERS
                BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
                FILES ${CMAKE_CURRENT_BINARY_DIR}/enger_export.h
)

target_include_directories(enger PUBLIC src)

target_link_libraries(enger PUBLIC
        glfw glm::glm
        Vulkan::Headers GPUOpen::VulkanMemoryAllocator
        imgui::imgui fastgltf::fastgltf KTX::ktx
        nfd::nfd
        spdlog::spdlog
)

if (ENABLE_PROFILING)
    target_link_libraries(enger PUBLIC Tracy::TracyClient)
    target_compile_definitions(enger PUBLIC
            ENABLE_PROFILING TRACY_ENABLE TRACY_VULKAN TRACY_VK_USE_SYMBOL_TABLE) # symbol table is needed for vk-hpp dynamic dispatcher

    add_custom_command(TARGET enger POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Tracy::TracyClient>
            $<TARGET_FILE_DIR:enger>
    )
endif()

if (ENABLE_LOGGING)
    target_compile_definitions(enger PRIVATE ENABLE_LOGGING)
endif()

target_include_directories(enger PRIVATE ${Stb_INCLUDE_DIR})

target_compile_definitions(enger PUBLIC
        GLFW_INCLUDE_NONE
        VK_NO_PROTOTYPES
        VULKAN_HPP_NO_EXCEPTIONS
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1

        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_ENABLE_EXPERIMENTAL
)

# This function is needed to copy Enger's necessary files to the binary dir.
# With C++26 #embed, we don't need to copy assets. And if Tracy fixes their DLL location issue, we don't need that either. Then we can directly use target_link_libraries instead of this helper.
function(target_link_enger target)
    target_link_libraries(${target} PRIVATE enger)

    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/assets"
            "$<TARGET_FILE_DIR:${target}>/assets"
            COMMENT "Copying enger assets to ${target} binary directory."
            VERBATIM
    )

    # This is needed specifically for Tracy. The other DLLs are copied automatically.
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${target}>
            "$<TARGET_FILE_DIR:${target}>"
            COMMAND_EXPAND_LISTS
            COMMENT "Copying runtime DLLs to ${target} binary directory."
            VERBATIM
    )
endfunction()

set_project_warnings(enger)
set_project_settings(enger)
enable_sanitizers(enger)

generate_export_header(enger)
