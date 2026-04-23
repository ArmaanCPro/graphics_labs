file(GLOB_RECURSE ENGER_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE ENGER_HEADERS CONFIGURE_DEPENDS src/*.h)

add_executable(enger ${ENGER_SOURCES} ${ENGER_HEADERS})

target_include_directories(enger PRIVATE src)

target_link_libraries(enger PRIVATE
        glfw glm::glm
        Vulkan::Headers GPUOpen::VulkanMemoryAllocator
        imgui::imgui fastgltf::fastgltf KTX::ktx
        nfd::nfd
        spdlog::spdlog
)

if (ENABLE_PROFILING)
    target_link_libraries(enger PRIVATE Tracy::TracyClient)
    target_compile_definitions(enger PRIVATE
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

target_compile_definitions(enger PRIVATE
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

set_project_warnings(enger)
enable_sanitizers(enger)

compile_slang_shaders(enger_shaders "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
add_dependencies(enger enger_shaders)

add_custom_command(TARGET enger POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:enger>/assets"
        COMMENT "Copying assets to binary directory."
        VERBATIM
)
